// ==========================================================================
// DoomLauncher.cpp — Cartucho / Punto de entrada independiente para DOOM (app1)
//
// Se compila exclusivamente en [env:doom] y no carga LVGL ni WiFi.
// Dedica el 100% de la CPU y memoria al motor del juego.
// ==========================================================================

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <SPIFFS.h>
#include <JC3248W535.h>
#include <esp_ota_ops.h>
#include <esp_heap_caps.h>

#include "doomgeneric.h"
#include "doomkeys.h"

// ─── Variables globales compartidas con doomgeneric_esp32.c ──────────────
uint16_t* g_doomCanvasBuf = nullptr;
volatile uint8_t g_doomZoneBits = 0;
volatile bool g_doomRunning = true;

// ─── Drivers de Pantalla y Touch ──────────────────────────────────────────
static JC3248W535_Display s_display;
static JC3248W535_Touch s_touch;

// ─── Función para reportar errores en pantalla y serial ───────────────────
extern "C" void Doom_ReportError(const char* msg) {
    Serial.printf("\n[DOOM FATAL ERROR] %s\n", msg);
    if (s_display.getCanvas()) {
        s_display.getCanvas()->fillScreen(0x0000);
        s_display.getCanvas()->setTextSize(2);
        s_display.getCanvas()->setTextColor(0xF800); // Rojo
        s_display.getCanvas()->setCursor(20, 40);
        s_display.getCanvas()->println("ERROR DOOM:");
        s_display.getCanvas()->setTextSize(1);
        s_display.getCanvas()->setTextColor(0xFFFF);
        s_display.getCanvas()->setCursor(20, 80);
        s_display.getCanvas()->println(msg);
        
        s_display.getCanvas()->setCursor(20, 200);
        s_display.getCanvas()->setTextColor(0x07E0); // Verde
        s_display.getCanvas()->println("Toca cualquier parte para volver a espOS32");
        s_display.flush();
    }
}

// ─── Función para salir de DOOM y volver al Sistema Operativo (app0) ──────
static void exitToOperatingSystem() {
    Serial.println("[DOOM] Saliendo del juego... Configurando arranque en espOS32 (app0)");
    const esp_partition_t* os_partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);

    if (os_partition != NULL) {
        esp_ota_set_boot_partition(os_partition);
        delay(200);
        esp_restart();
    } else {
        Serial.println("[DOOM] ERROR: No se encontró la partición del OS (app0)");
        esp_restart();
    }
}

// ─── Dibujar controles táctiles visibles en pantalla ──────────────────────
static void drawTouchOverlay() {
    Arduino_Canvas* c = s_display.getCanvas();
    if (!c) return;

    // 1. Botón SALIR (Top Right: 420x5, W:55, H:38)
    c->fillRect(420, 5, 55, 38, 0xF800); // Fondo Rojo
    c->drawRect(420, 5, 55, 38, 0xFFFF); // Borde Blanco
    c->setTextSize(1);
    c->setTextColor(0xFFFF);
    c->setCursor(430, 20);
    c->print("SALIR");

    // 2. Botón ENTER / MENU (Top Center: 160x5, W:160, H:38)
    c->fillRect(160, 5, 160, 38, 0x03E0); // Fondo Verde oscuro
    c->drawRect(160, 5, 160, 38, 0x07E0); // Borde Verde brillante
    c->setTextSize(2);
    c->setTextColor(0xFFFF);
    c->setCursor(185, 16);
    c->print("ENTER / OK");

    // 3. Botón ABRIR / USE (Bottom Center: 160x275, W:160, H:38)
    c->fillRect(160, 275, 160, 38, 0x001F); // Fondo Azul
    c->drawRect(160, 275, 160, 38, 0x07FF); // Borde Cian
    c->setTextSize(2);
    c->setTextColor(0xFFFF);
    c->setCursor(170, 286);
    c->print("ABRIR / USE");

    // 4. Cruceta Izquierda (D-Pad Movimiento)
    // Arriba (Adelante)
    c->fillRect(15, 65, 50, 40, 0x2104);
    c->drawRect(15, 65, 50, 40, 0xCE79);
    c->setTextSize(2);
    c->setTextColor(0xFFFF);
    c->setCursor(33, 76);
    c->print("^");

    // Abajo
    c->fillRect(15, 205, 50, 40, 0x2104);
    c->drawRect(15, 205, 50, 40, 0xCE79);
    c->setCursor(33, 216);
    c->print("v");

    // Izq (Strafe L)
    c->fillRect(5, 125, 33, 60, 0x2104);
    c->drawRect(5, 125, 33, 60, 0xCE79);
    c->setCursor(12, 146);
    c->print("<");

    // Der (Strafe R)
    c->fillRect(42, 125, 33, 60, 0x2104);
    c->drawRect(42, 125, 33, 60, 0xCE79);
    c->setCursor(50, 146);
    c->print(">");

    // 5. Panel Derecho (Giro y Disparo)
    // Giro Izq
    c->fillRect(405, 65, 33, 50, 0x2104);
    c->drawRect(405, 65, 33, 50, 0xCE79);
    c->setCursor(412, 80);
    c->print("L");

    // Giro Der
    c->fillRect(442, 65, 33, 50, 0x2104);
    c->drawRect(442, 65, 33, 50, 0xCE79);
    c->setCursor(450, 80);
    c->print("R");

    // Botón FUEGO
    c->fillRect(405, 135, 70, 120, 0xC800); // Rojo anaranjado
    c->drawRect(405, 135, 70, 120, 0xF800);
    c->setTextSize(2);
    c->setTextColor(0xFFFF);
    c->setCursor(415, 185);
    c->print("FIRE");
}

// ─── Mapeo Táctil a Controles de Doom ────────────────────────────────────
static void updateTouchControls() {
    TouchPoint tp;
    if (s_touch.read(tp) && tp.touched) {
        // En rotación Landscape (480x320)
        int16_t x = tp.x;
        int16_t y = tp.y;

        // 1. Botón de salida (Esquina superior derecha: X >= 415, Y <= 48)
        if (x >= 415 && y <= 48) {
            exitToOperatingSystem();
            return;
        }

        uint8_t zones = 0;

        // 2. Botón ENTER / MENU (Barra superior central)
        if (x >= 140 && x <= 340 && y <= 55) {
            zones |= (1 << 6); // bit 6 = KEY_ENTER
        }
        // 3. Botón ABRIR / USE (Barra inferior central)
        else if (x >= 140 && x <= 340 && y >= 265) {
            zones |= (1 << 7); // bit 7 = KEY_USE
        }
        // 4. Panel Izquierdo (Cruceta de Movimiento)
        else if (x < 100) {
            if (y < 115) {
                zones |= (1 << 1); // Adelante (UP)
            } else if (y > 195) {
                zones |= (1 << 1); // También movimiento
            } else {
                if (x < 40) zones |= (1 << 0); // Strafe Izquierda
                else zones |= (1 << 2);        // Strafe Derecha
            }
        }
        // 5. Panel Derecho (Giro y Disparo)
        else if (x > 380) {
            if (y >= 125) {
                zones |= (1 << 4); // Disparo / FIRE (Ctrl)
            } else {
                if (x < 440) zones |= (1 << 3); // Giro Izquierda
                else zones |= (1 << 5);         // Giro Derecha
            }
        }

        g_doomZoneBits = zones;
    } else {
        g_doomZoneBits = 0;
    }
}

// ─── Setup ────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=================================");
    Serial.println("  DOOM Cartridge — espOS32 (app1)  ");
    Serial.println("=================================");
    Serial.printf("Free Heap: %u bytes\n", ESP.getFreeHeap());
    Serial.printf("Free PSRAM: %u bytes\n", ESP.getFreePsram());

    // 1. Asignar Buffer de Render en PSRAM (320 x 200 x 2 bytes = 128 KB)
    g_doomCanvasBuf = (uint16_t*)heap_caps_malloc(
        DOOMGENERIC_RESX * DOOMGENERIC_RESY * sizeof(uint16_t),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
    );
    if (!g_doomCanvasBuf) {
        Serial.println("[DOOM] FATAL: No se pudo asignar g_doomCanvasBuf en PSRAM");
        while (1) delay(1000);
    }

    // 2. Inicializar Pantalla AMOLED JC3248W535
    if (!s_display.begin()) {
        Serial.println("[DOOM] Error al inicializar display!");
        while (1) delay(1000);
    }
    s_display.setRotation(ROTATION_90); // Landscape 480x320
    s_display.backlightOn();

    // 3. Inicializar Panel Táctil
    s_touch.begin();
    s_display.setTouchRotation(&s_touch);

    // Limpiar pantalla inicial
    if (s_display.getCanvas()) {
        s_display.getCanvas()->fillScreen(0x0000);
        s_display.getCanvas()->setTextSize(2);
        s_display.getCanvas()->setTextColor(0xFFFF);
        s_display.getCanvas()->setCursor(140, 150);
        s_display.getCanvas()->print("Cargando DOOM...");
        s_display.flush();
    }

    // 4. Inicializar MicroSD y SPIFFS
    pinMode(10, OUTPUT);
    digitalWrite(10, HIGH);
    delay(50);

    SPIClass* sdSPI = new SPIClass(HSPI);
    sdSPI->begin(12, 13, 11, 10); // SCK, MISO, MOSI, SS
    bool sdMounted = false;
    for (int retry = 0; retry < 5 && !sdMounted; retry++) {
        sdMounted = SD.begin(10, *sdSPI, 10000000);
        if (!sdMounted) {
            sdMounted = SD.begin(10, *sdSPI, 4000000);
        }
        if (!sdMounted) delay(100);
    }

    if (sdMounted) {
        Serial.println("[DOOM] MicroSD montada correctamente en /sd");
        File root = SD.open("/");
        if (root) {
            File f = root.openNextFile();
            while (f) {
                Serial.printf("[SD FILE] %s (tamano: %lu bytes)\n", f.name(), (unsigned long)f.size());
                f = root.openNextFile();
            }
        }
    } else {
        Serial.println("[DOOM] MicroSD no detectada!");
        if (s_display.getCanvas()) {
            s_display.getCanvas()->fillScreen(0x0000);
            s_display.getCanvas()->setTextSize(2);
            s_display.getCanvas()->setTextColor(0xF800);
            s_display.getCanvas()->setCursor(20, 40);
            s_display.getCanvas()->println("ERROR TARJETA SD:");
            s_display.getCanvas()->setTextSize(1);
            s_display.getCanvas()->setTextColor(0xFFFF);
            s_display.getCanvas()->setCursor(20, 80);
            s_display.getCanvas()->println("No se detecto la tarjeta MicroSD en la ranura.");
            s_display.getCanvas()->setCursor(20, 110);
            s_display.getCanvas()->println("Verifica que este insertada y formateada en FAT32.");
            s_display.getCanvas()->setCursor(20, 160);
            s_display.getCanvas()->setTextColor(0x07E0);
            s_display.getCanvas()->println("Toca la pantalla para volver a espOS32");
            s_display.flush();
        }
        while (1) {
            TouchPoint tp;
            if (s_touch.read(tp) && tp.touched) {
                exitToOperatingSystem();
            }
            delay(50);
        }
    }

    // Localizar el archivo IWAD
    const char* iwad_path = NULL;
    if (SD.exists("/doom1.wad")) iwad_path = "/sd/doom1.wad";
    else if (SD.exists("/DOOM1.WAD")) iwad_path = "/sd/DOOM1.WAD";
    else if (SD.exists("/doom.wad")) iwad_path = "/sd/doom.wad";
    else if (SD.exists("/DOOM.WAD")) iwad_path = "/sd/DOOM.WAD";
    else if (SD.exists("/doom2.wad")) iwad_path = "/sd/doom2.wad";
    else if (SD.exists("/DOOM2.WAD")) iwad_path = "/sd/DOOM2.WAD";
    else if (SD.exists("/freedoom1.wad")) iwad_path = "/sd/freedoom1.wad";
    else if (SPIFFS.exists("/doom1.wad")) iwad_path = "/spiffs/doom1.wad";
    else if (SPIFFS.exists("/DOOM1.WAD")) iwad_path = "/spiffs/DOOM1.WAD";

    if (iwad_path != NULL) {
        Serial.printf("[DOOM] IWAD encontrado en: %s\n", iwad_path);
        FILE* f = fopen(iwad_path, "rb");
        if (f) {
            Serial.println("[DOOM] Test fopen OK!");
            fclose(f);
        } else {
            Serial.printf("[DOOM] ADVERTENCIA: fopen(%s) devolvio NULL!\n", iwad_path);
        }
    } else {
        Serial.println("[DOOM] ERROR: No se encontro ningun archivo .wad en SD ni SPIFFS");
    }

    Serial.println("[DOOM] Iniciando motor Doom Generic...");
    if (iwad_path != NULL) {
        char* doom_argv[] = {(char*)"doom", (char*)"-iwad", (char*)iwad_path, NULL};
        doomgeneric_Create(3, doom_argv);
    } else {
        char* doom_argv[] = {(char*)"doom", NULL};
        doomgeneric_Create(1, doom_argv);
    }
    Serial.println("[DOOM] doomgeneric_Create completado.");
}

// ─── Loop ─────────────────────────────────────────────────────────────────
void loop() {
    updateTouchControls();

    // Ejecuta un tick del motor de juego
    doomgeneric_Tick();

    // Dibujar el canvas de DOOM (320x200) centrado en la pantalla (480x320)
    // Offset X = (480 - 320) / 2 = 80
    // Offset Y = (320 - 200) / 2 = 60
    if (s_display.getCanvas() && g_doomCanvasBuf) {
        s_display.getCanvas()->draw16bitRGBBitmap(80, 60, g_doomCanvasBuf, DOOMGENERIC_RESX, DOOMGENERIC_RESY);
        
        // Dibujar botones y controles táctiles visibles
        drawTouchOverlay();

        s_display.flush();
    }

    // Ceder CPU a FreeRTOS para alimentar el Watchdog Timer y evitar reinicios
    delay(1);
}
