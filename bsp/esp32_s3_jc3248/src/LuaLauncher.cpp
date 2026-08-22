// ==========================================================================
// LuaLauncher.cpp — Cartucho Standalone Dual Lua 5.4 & PICO-8 para ESP32-S3
//
// Se compila exclusivamente en [env:lua] (app1 / offset 0x510000).
// Dedica el 100% de la CPU al motor de juego sin sobrecarga de LVGL.
// Soporta carga de juegos .p8, .p8.png y scripts .lua desde la MicroSD.
// ==========================================================================

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <SPIFFS.h>
#include <JC3248W535.h>
#include <esp_ota_ops.h>
#include <esp_heap_caps.h>
#include <driver/i2s.h>
#include <vector>
#include <string>
#include <algorithm>

#include "SharedState.hpp"
#include "AsyncFS.hpp"
#include "LuaContext.hpp"
#include "P8Cartridge.hpp"
#include "P8Api.hpp"
#include "P8Synth.hpp"
#include "CbdApi.hpp"
#include "VirtualKeyboard.hpp"

CartridgeSharedState g_cartState;

static JC3248W535_Display s_display;
static JC3248W535_Touch   s_touch;
static SPIClass*          s_sdSPI = nullptr;

static uint16_t* s_p8RenderBuf = nullptr;   // 128x128 = 32 KB
static uint16_t* s_s3ScaledCanvas = nullptr; // 320x320 = 204.8 KB en PSRAM
static bool s_sdMounted = false;
static bool s_loadingRom = false;

// ─── Configuración I2S Audio para ESP32-S3 (JC3248W535) ───────────────────
#define I2S_NUM         I2S_NUM_0
#define I2S_BCK_IO      42
#define I2S_WS_IO       2
#define I2S_DO_IO       41
#define I2S_SAMPLE_RATE 44100

static void initI2S() {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = I2S_SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = 0,
        .dma_buf_count = 8,
        .dma_buf_len = 256,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
        .mck_io_num = I2S_PIN_NO_CHANGE,
        .bck_io_num = I2S_BCK_IO,
        .ws_io_num = I2S_WS_IO,
        .data_out_num = I2S_DO_IO,
        .data_in_num = I2S_PIN_NO_CHANGE
    };

    i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM, &pin_config);
    i2s_zero_dma_buffer(I2S_NUM);
}

// ─── Tarea Core 0: Audio I2S, Touch Polling y E/S SD ──────────────────────
static void core0_task(void* pvParameters) {
    static int16_t pcmBuffer[256 * 2];
    TouchPoint tp;

    while (true) {
        // 1. Muestreo Táctil GT911 (~120 Hz)
        if (s_touch.read(tp) && tp.touched) {
            g_cartState.touchX.store(tp.x, std::memory_order_relaxed);
            g_cartState.touchY.store(tp.y, std::memory_order_relaxed);
            g_cartState.touchPressed.store(true, std::memory_order_relaxed);

            // Mapeo Gamepad S3 (Portrait 320x480)
            uint16_t state = 0;
            
            // D-Pad: Cruceta real en cruz (Centro en cx=70, cy=390)
            if (tp.x >= 15 && tp.x <= 125 && tp.y >= 340 && tp.y <= 440) {
                int dx = tp.x - 70;
                int dy = tp.y - 390;
                if (std::abs(dx) > std::abs(dy)) {
                    if (dx < -10) state |= BTN_LEFT;
                    else if (dx > 10) state |= BTN_RIGHT;
                } else {
                    if (dy < -10) state |= BTN_UP;
                    else if (dy > 10) state |= BTN_DOWN;
                }
            }

            // Botón O (cx=205, cy=405)
            int dOx = tp.x - 205, dOy = tp.y - 405;
            if (dOx * dOx + dOy * dOy <= 32 * 32) {
                state |= BTN_O;
            }

            // Botón X (cx=265, cy=365)
            int dXx = tp.x - 265, dXy = tp.y - 365;
            if (dXx * dXx + dXy * dXy <= 32 * 32) {
                state |= BTN_X;
            }

            // Botón Salir a CBDos (arriba derecha)
            if (tp.x > 240 && tp.y < 50) {
                state |= BTN_PAUSE;
                g_cartState.exitToCBDosRequested.store(true);
            }
            g_cartState.btnState.store(state, std::memory_order_relaxed);
        } else {
            g_cartState.touchPressed.store(false, std::memory_order_relaxed);
            g_cartState.btnState.store(0, std::memory_order_relaxed);
        }

        // 2. Síntesis y volcado de Audio I2S DMA
        P8Synth::getInstance().renderAudio(pcmBuffer, 256);
        size_t written = 0;
        i2s_write(I2S_NUM, pcmBuffer, 256 * 2 * sizeof(int16_t), &written, pdMS_TO_TICKS(20));

        // 3. E/S Asíncrona MicroSD (solo si no estamos cargando una ROM)
        if (!s_loadingRom) {
            AsyncFS::getInstance().updateCore0();
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

// ─── Escalado por Software 128x128 a 320x320 para S3 ─────────────────────
static void scale128to320(const uint16_t* src128, uint16_t* dst320) {
    for (int dy = 0; dy < 320; dy++) {
        int sy = (dy * 128) / 320;
        const uint16_t* srcRow = src128 + (sy * 128);
        uint16_t* dstRow = dst320 + (dy * 320);

        for (int dx = 0; dx < 320; dx++) {
            int sx = (dx * 128) / 320;
            dstRow[dx] = srcRow[sx];
        }
    }
}

// ─── Escaneo de Juegos en MicroSD ─────────────────────────────────────────
static std::vector<std::string> scanSDGames() {
    std::vector<std::string> games;
    if (!s_sdMounted) return games;

    const char* dirs[] = {"/p8", "/games/p8", "/games/lua", "/games", "/"};

    for (const char* dirPath : dirs) {
        File root = SD.open(dirPath);
        if (!root || !root.isDirectory()) continue;

        File file = root.openNextFile();
        while (file) {
            if (!file.isDirectory()) {
                String name = file.name();
                if (name.endsWith(".p8") || name.endsWith(".png") || name.endsWith(".lua")) {
                    String fullPath = String(dirPath);
                    if (!fullPath.endsWith("/")) fullPath += "/";
                    fullPath += name;
                    // Evitar duplicados
                    if (std::find(games.begin(), games.end(), fullPath.c_str()) == games.end()) {
                        games.push_back(fullPath.c_str());
                    }
                }
            }
            file = root.openNextFile();
        }
    }

    std::sort(games.begin(), games.end());
    return games;
}

// ─── Selector Táctil de Juegos ────────────────────────────────────────────
static std::string showGameSelector(const std::vector<std::string>& gameList) {
    if (!s_display.getCanvas() || gameList.empty()) return "";

    int selectedIdx = -1;
    int scrollOffset = 0;

    while (selectedIdx < 0 && !g_cartState.exitToCBDosRequested.load()) {
        auto canvas = s_display.getCanvas();
        canvas->fillScreen(0x0845); // Azul Cyberdeck oscuro

        // Cabecera
        canvas->fillRect(0, 0, 320, 45, 0x10A8);
        canvas->setTextSize(2);
        canvas->setTextColor(0xFD00); // Amarillo retro
        canvas->setCursor(15, 12);
        canvas->print("PICO-8 ROMS");

        // Botón Salir
        canvas->fillRoundRect(245, 8, 68, 28, 5, 0xA800);
        canvas->drawRoundRect(245, 8, 68, 28, 5, 0xF800);
        canvas->setTextSize(1);
        canvas->setTextColor(0xFFFF);
        canvas->setCursor(256, 17);
        canvas->print("CBDos");

        // Lista de juegos
        int y = 55;
        for (size_t i = scrollOffset; i < gameList.size() && y < 450; i++) {
            canvas->fillRoundRect(10, y, 300, 40, 6, (i % 2 == 0) ? 0x18C8 : 0x212B);
            canvas->drawRoundRect(10, y, 300, 40, 6, 0x4A69);

            // Icono
            canvas->fillRect(20, y + 10, 20, 20, 0xFBB7);

            // Nombre
            std::string path = gameList[i];
            size_t slash = path.rfind('/');
            std::string name = (slash != std::string::npos) ? path.substr(slash + 1) : path;

            canvas->setTextSize(1);
            canvas->setTextColor(0xFFFF);
            canvas->setCursor(50, y + 15);
            canvas->print(name.c_str());

            y += 48;
        }

        s_display.flush();

        // Detectar toque
        if (g_cartState.touchPressed.load(std::memory_order_relaxed)) {
            int ty = g_cartState.touchY.load(std::memory_order_relaxed);

            if (ty >= 55) {
                int clicked = scrollOffset + (ty - 55) / 48;
                if (clicked >= 0 && clicked < (int)gameList.size()) {
                    selectedIdx = clicked;
                    return gameList[selectedIdx];
                }
            }
        }

        delay(50);
    }

    return "";
}

// ─── Setup ────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\n=== Cartucho Lua 5.4 & PICO-8 Engine (ESP32-S3) ===");

    // 1. Asignar búferes de render en PSRAM
    s_p8RenderBuf = (uint16_t*)heap_caps_malloc(128 * 128 * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_s3ScaledCanvas = (uint16_t*)heap_caps_malloc(320 * 320 * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    // 2. Inicializar Pantalla QSPI
    s_display.begin();
    s_display.setRotation(ROTATION_0); // 320x480
    s_display.backlightOn();

    // 3. Inicializar Táctil
    s_touch.begin();
    s_display.setTouchRotation(&s_touch);

    // 4. Inicializar Audio I2S
    initI2S();

    // 5. Inicializar MicroSD en bus SPI dedicado HSPI (Pines 12, 13, 11, 10 @ 10 MHz)
    pinMode(10, OUTPUT);
    digitalWrite(10, HIGH);
    delay(50);

    s_sdSPI = new SPIClass(HSPI);
    s_sdSPI->begin(12, 13, 11, 10); // SCK=12, MISO=13, MOSI=11, SS=10
    
    for (int retry = 0; retry < 5 && !s_sdMounted; retry++) {
        s_sdMounted = SD.begin(10, *s_sdSPI, 10000000);
        if (!s_sdMounted) {
            s_sdMounted = SD.begin(10, *s_sdSPI, 4000000);
        }
        if (!s_sdMounted) delay(100);
    }

    if (s_sdMounted) {
        Serial.println("[SD] MicroSD montada correctamente en bus HSPI");
    } else {
        Serial.println("[SD] Aviso: No se detectó MicroSD");
    }

    // 6. Lanzar Tarea Core 0 para Audio y Touch
    xTaskCreatePinnedToCore(core0_task, "s3_audio_touch", 8192, NULL, 6, NULL, 0);

    Serial.println("[Setup] Hardware listo.");
}

// ─── Loop Principal (Core 1) ──────────────────────────────────────────────
void loop() {
    // 1. Escanear y seleccionar juego si hay SD
    std::string selectedGame;
    auto games = scanSDGames();
    if (!games.empty()) {
        selectedGame = showGameSelector(games);
    }

    // 2. Inicializar VM Lua
    LuaContext lua;
    lua.init(true);

    P8Cartridge cart;
    bool isPico8 = true;

    if (!selectedGame.empty()) {
        Serial.printf("[ROM] Cargando: %s\n", selectedGame.c_str());
        s_loadingRom = true;

        if (selectedGame.rfind(".lua") == selectedGame.length() - 4) {
            isPico8 = false;
        }

        if (isPico8) {
            File f = SD.open(selectedGame.c_str(), FILE_READ);
            if (f) {
                size_t fsize = f.size();
                Serial.printf("[ROM] Tamaño del archivo: %u bytes\n", (unsigned int)fsize);
                std::vector<uint8_t> buffer(fsize);
                
                size_t bytesRead = 0;
                while (bytesRead < fsize && f.available()) {
                    size_t chunk = (fsize - bytesRead > 4096) ? 4096 : (fsize - bytesRead);
                    size_t rd = f.read(buffer.data() + bytesRead, chunk);
                    if (rd == 0) break;
                    bytesRead += rd;
                }
                f.close();
                Serial.printf("[ROM] Leídos: %u bytes\n", (unsigned int)bytesRead);

                if (selectedGame.rfind(".png") != std::string::npos || selectedGame.rfind(".PNG") != std::string::npos) {
                    if (cart.loadFromP8PngBytes(buffer.data(), bytesRead)) {
                        Serial.println("[ROM] Cartucho .p8.png decodificado con éxito");
                    } else {
                        Serial.println("[ROM] ERROR al decodificar .p8.png");
                    }
                } else {
                    std::string strContent((const char*)buffer.data(), bytesRead);
                    cart.loadFromP8String(strContent);
                    Serial.println("[ROM] Cartucho .p8 cargado con éxito");
                }
            } else {
                Serial.println("[ROM] ERROR: No se pudo abrir el archivo");
            }
        }
        s_loadingRom = false;
    } else {
        // Demo por defecto si no hay juego seleccionado
        std::string defaultCode = "function _init() cls(1) t=0 end\n"
                                  "function _update60() t+=1 end\n"
                                  "function _draw() cls(1) print('CBDOS PICO-8 S3', 30, 30, 7) print('COLOCA JUEGOS .P8 EN SD', 10, 50, 11) circfill(64+cos(t/30)*25, 85+sin(t/30)*25, 8, 8+flr(t/10)%8) end\n";
        cart.loadFromP8String(defaultCode);
    }

    if (isPico8) {
        P8Api::init(&cart);
        P8Api::registerAll(lua.getRawState());
        P8Synth::getInstance().init(&cart, 44100);

        std::string err;
        if (!lua.loadAndRunString(cart.getLuaCode(), &err)) {
            Serial.printf("[LUA ERR] %s\n", err.c_str());
        }
    } else {
        CbdApi::init(s_s3ScaledCanvas, 320, 320);
        CbdApi::registerAll(lua.getRawState());
    }

    lua.runFunction("_init");

    TickType_t lastTick = xTaskGetTickCount();
    const TickType_t delayTicks = pdMS_TO_TICKS(16); // ~60 FPS
    uint16_t prevBtnState = 0;

    while (!g_cartState.exitToCBDosRequested.load()) {
        // Actualizar flanco de subida para btnp()
        uint16_t currentBtn = g_cartState.btnState.load(std::memory_order_relaxed);
        uint16_t pressedBtn = currentBtn & ~prevBtnState;
        prevBtnState = currentBtn;
        g_cartState.btnpState.store(pressedBtn, std::memory_order_relaxed);

        if (lua.hasFunction("_update60")) lua.runFunction("_update60");
        else if (lua.hasFunction("_update")) lua.runFunction("_update");

        if (lua.hasFunction("_draw")) lua.runFunction("_draw");

        // Convertir VRAM a RGB565
        P8Api::blitToRGB565(s_p8RenderBuf, 128);

        // Escalar de 128x128 a 320x320
        if (s_s3ScaledCanvas) {
            scale128to320(s_p8RenderBuf, s_s3ScaledCanvas);
        }

        // Volcar al Canvas de la pantalla
        if (s_display.getCanvas()) {
            auto canvas = s_display.getCanvas();
            if (s_s3ScaledCanvas) {
                canvas->draw16bitRGBBitmap(0, 0, s_s3ScaledCanvas, 320, 320);
            }

            // ─── Fondo del Gamepad Inferior (320x160) ───
            canvas->fillRect(0, 320, 320, 160, 0x18C3); // Fondo gris Cyberdeck
            canvas->drawFastHLine(0, 320, 320, 0x4A69);  // Línea divisoria superior

            // ─── D-Pad: Cruceta Auténtica en Cruz (+) ───
            canvas->fillRect(55, 345, 30, 90, 0x2104);
            canvas->drawRect(55, 345, 30, 90, 0x632C);
            canvas->fillRect(25, 375, 90, 30, 0x2104);
            canvas->drawRect(25, 375, 90, 30, 0x632C);
            canvas->fillCircle(70, 390, 7, 0x18C3);

            // Flechas en relieve del D-Pad
            canvas->setTextSize(1);
            canvas->setTextColor(0xAD55);
            canvas->setCursor(67, 350); canvas->print("^"); // Arriba
            canvas->setCursor(67, 420); canvas->print("v"); // Abajo
            canvas->setCursor(32, 386); canvas->print("<"); // Izquierda
            canvas->setCursor(102, 386); canvas->print(">"); // Derecha

            // ─── Botones de Acción PICO-8 (🅾️ y ❎ en Diagonal) ───
            canvas->fillCircle(205, 405, 22, 0xFBB7);
            canvas->drawCircle(205, 405, 22, 0xFFFF);
            canvas->setTextSize(2);
            canvas->setTextColor(0x0000);
            canvas->setCursor(200, 398);
            canvas->print("O");

            canvas->fillCircle(265, 365, 22, 0x2D7F);
            canvas->drawCircle(265, 365, 22, 0xFFFF);
            canvas->setTextColor(0xFFFF);
            canvas->setCursor(260, 358);
            canvas->print("X");

            // ─── Botón Salir a CBDos (Arriba a la derecha) ───
            canvas->fillRoundRect(245, 6, 70, 26, 6, 0xA800); // Fondo rojo
            canvas->drawRoundRect(245, 6, 70, 26, 6, 0xF800); // Borde rojo brillante
            canvas->setTextSize(1);
            canvas->setTextColor(0xFFFF);
            canvas->setCursor(256, 15);
            canvas->print("CBDos");
        }

        s_display.flush();

        vTaskDelayUntil(&lastTick, delayTicks);
    }

    // Salir a CBDos (Factory / app0)
    Serial.println("[LUA S3] Retornando a CBDos...");
    const esp_partition_t* factory = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
    if (factory) {
        esp_ota_set_boot_partition(factory);
        esp_restart();
    }
    while (1) delay(1000);
}
