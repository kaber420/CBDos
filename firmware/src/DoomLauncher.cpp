// ==========================================================================
// DoomLauncher.cpp — Cartucho / Punto de entrada independiente para DOOM (app1)
//
// Se compila exclusivamente en [env:doom] y no carga LVGL ni WiFi.
// Dedica el 100% de la CPU y memoria al motor del juego.
// Modo Vertical (Portrait 320x480) estilo Game Boy con CartridgeGamepad.
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
#include "CartridgeGamepad.h"

// ─── Variables globales compartidas con doomgeneric_esp32.c ──────────────
uint16_t* g_doomCanvasBuf = nullptr;
volatile uint16_t g_doomZoneBits = 0;
volatile bool g_doomRunning = true;

// ─── Drivers de Pantalla, Touch y Gamepad ─────────────────────────────────
static JC3248W535_Display s_display;
static JC3248W535_Touch   s_touch;
static CartridgeGamepad   s_gamepad;

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

    // 2. Inicializar Pantalla AMOLED en Portrait 320x480
    if (!s_display.begin()) {
        Serial.println("[DOOM] Error al inicializar display!");
        while (1) delay(1000);
    }
    s_display.setRotation(ROTATION_0); // Portrait 320x480
    s_display.backlightOn();

    // 3. Inicializar Panel Táctil y CartridgeGamepad en Portrait DOOM
    s_touch.begin();
    s_display.setTouchRotation(&s_touch);
    s_gamepad.begin(&s_display, &s_touch, LAYOUT_PORTRAIT_DOOM);

    // Limpiar pantalla inicial
    if (s_display.getCanvas()) {
        s_display.getCanvas()->fillScreen(0x0000);
        s_display.getCanvas()->setTextSize(2);
        s_display.getCanvas()->setTextColor(0xFFFF);
        s_display.getCanvas()->setCursor(60, 80);
        s_display.getCanvas()->print("Cargando DOOM...");
        s_gamepad.draw(true);
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
            s_gamepad.draw(true);
            s_display.flush();
        }
        while (1) {
            s_gamepad.read();
            if (s_gamepad.handleExit()) return;
            TouchPoint tp;
            if (s_touch.read(tp) && tp.touched) {
                CartridgeGamepad::exitToOS();
                return;
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

    // Redibujar Gamepad inferior completo
    s_gamepad.draw(true);
    s_display.flush();
}

// ─── Loop ─────────────────────────────────────────────────────────────────
void loop() {
    // 1. Leer Controles Táctiles mediante CartridgeGamepad
    uint16_t btns = s_gamepad.read();
    if (s_gamepad.handleExit()) {
        return;
    }

    uint16_t zones = 0;
    if (btns & PAD_STRAFE_L) zones |= (1 << 0);  // bit 0 = Strafe Izquierda
    if (btns & PAD_UP)       zones |= (1 << 1);  // bit 1 = Adelante (Up)
    if (btns & PAD_STRAFE_R) zones |= (1 << 2);  // bit 2 = Strafe Derecha
    if (btns & PAD_LEFT)     zones |= (1 << 3);  // bit 3 = Girar Izquierda
    if (btns & PAD_A)        zones |= (1 << 4);  // bit 4 = Disparo (Ctrl)
    if (btns & PAD_RIGHT)    zones |= (1 << 5);  // bit 5 = Girar Derecha
    if (btns & PAD_START)    zones |= (1 << 6);  // bit 6 = ENTER (Entrar / OK)
    if (btns & PAD_B)        zones |= (1 << 7);  // bit 7 = USAR / ABRIR PUERTAS (Spacebar)
    if (btns & PAD_DOWN)     zones |= (1 << 8);  // bit 8 = Atrás (Down)
    if (btns & PAD_SELECT)   zones |= (1 << 9);  // bit 9 = ESCAPE / Menú
    if (btns & PAD_RUN)      zones |= (1 << 10); // bit 10 = Run / Speed (Shift)

    g_doomZoneBits = zones;

    // 2. Ejecuta un tick del motor de juego
    doomgeneric_Tick();

    // 3. Dibujar el canvas de DOOM (320x200) en la parte superior (0, 0)
    if (s_display.getCanvas() && g_doomCanvasBuf) {
        s_display.getCanvas()->draw16bitRGBBitmap(0, 0, g_doomCanvasBuf, DOOMGENERIC_RESX, DOOMGENERIC_RESY);
        s_display.flush();
    }

    // Ceder CPU a FreeRTOS para alimentar el Watchdog Timer y evitar reinicios
    delay(1);
}
