// ==========================================================================
// GBCLauncher.cpp — Cartucho Game Boy & Game Boy Color (Selector Visual de ROMs)
//
// 1. Selector táctil de juegos (.gb / .gbc) con soporte de paginación
// 2. Renderizado acelerado 60 FPS (flushGameArea 320x288)
// 3. Audio I2S DMA en pines 42, 2, 41
// ==========================================================================

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <SPIFFS.h>
#include <vector>
#include <JC3248W535.h>
#include <esp_ota_ops.h>
#include <esp_heap_caps.h>
#include <driver/i2s.h>

#define ENABLE_LCD 1
#define ENABLE_SOUND 1
#define WALNUT_FULL_GBC_SUPPORT 1
#define WALNUT_GB_12_COLOUR 1
#define MINIGB_APU_AUDIO_FORMAT_S16SYS 1
#define AUDIO_SAMPLE_RATE 32768

// Declaraciones previas requeridas por walnut_cgb.h
extern "C" {
    uint8_t audio_read(const uint_fast16_t addr);
    void audio_write(const uint_fast16_t addr, const uint8_t val);
}

#include "minigb_apu.h"
#include "walnut_cgb.h"
#include "CartridgeGamepad.h"

// ─── Extensión rápida de Canvas para refrescar solo el área de juego 320x288 ─
class FastGameCanvas : public Arduino_Canvas {
public:
    void flushGameArea() {
        if (_output && _framebuffer) {
            _output->draw16bitRGBBitmap(0, 0, _framebuffer, 320, 288);
        }
    }
};

// ─── Hardware y Drivers ─────────────────────────────────────────────────────
static JC3248W535_Display s_display;
static JC3248W535_Touch   s_touch;
static CartridgeGamepad   s_gamepad;
static FastGameCanvas*    s_fastCanvas = nullptr;
static uint16_t*          s_fbPtr = nullptr;

// ─── Memoria y Estado del Emulador ──────────────────────────────────────────
static uint8_t* s_romData = nullptr;
static size_t   s_romSize = 0;
static uint8_t* s_cartRam = nullptr;
static size_t   s_cartRamSize = 131072; // 128 KB para SRAM
static String   s_currentSavePath = "";
static bool     s_cartRamDirty = false;

static struct gb_s            s_gb;
static struct minigb_apu_ctx  s_apu;
static int16_t                s_audioBuffer[AUDIO_SAMPLES * 2];

// ─── Paleta clásica Game Boy monocromático (RGB565) ─────────────────────────
static const uint16_t s_palette[4] = {
    0xFFFF, // Blanco (0)
    0xAD55, // Gris claro (1)
    0x52AA, // Gris oscuro (2)
    0x0000  // Negro (3)
};

// ─── Callbacks de Walnut-CGB ────────────────────────────────────────────────
static uint8_t gb_rom_read(struct gb_s *gb, const uint_fast32_t addr) {
    (void)gb;
    if (addr < s_romSize && s_romData) {
        return s_romData[addr];
    }
    return 0xFF;
}

static uint16_t gb_rom_read16(struct gb_s *gb, const uint_fast32_t addr) {
    (void)gb;
    if (addr + 1 < s_romSize && s_romData) {
        return *(uint16_t*)(s_romData + addr);
    }
    return 0xFFFF;
}

static uint32_t gb_rom_read32(struct gb_s *gb, const uint_fast32_t addr) {
    (void)gb;
    if (addr + 3 < s_romSize && s_romData) {
        return *(uint32_t*)(s_romData + addr);
    }
    return 0xFFFFFFFF;
}

static uint8_t gb_cart_ram_read(struct gb_s *gb, const uint_fast32_t addr) {
    (void)gb;
    if (s_cartRam && addr < s_cartRamSize) {
        return s_cartRam[addr];
    }
    return 0xFF;
}

static void gb_cart_ram_write(struct gb_s *gb, const uint_fast32_t addr, const uint8_t val) {
    (void)gb;
    if (s_cartRam && addr < s_cartRamSize) {
        s_cartRam[addr] = val;
        s_cartRamDirty = true;
    }
}

static void gb_error(struct gb_s *gb, const enum gb_error_e err, const uint16_t val) {
    (void)gb;
    Serial.printf("[GBC] Error del emulador: %d (val=0x%04X)\n", (int)err, val);
}

// ─── Renderizado de Línea Ultra-Rápido Especializado para CGB (Color Real) ──
static void lcd_draw_line_cgb(struct gb_s *gb, const uint8_t *pixels, const uint_fast8_t line) {
    if (!s_fbPtr || line >= 144) return;

    int y1 = line * 2;
    int y2 = y1 + 1;
    uint32_t* row1 = (uint32_t*)(s_fbPtr + (y1 * 320));
    uint32_t* row2 = (uint32_t*)(s_fbPtr + (y2 * 320));
    const uint16_t* fixPal = gb->cgb.fixPalette;

    for (int x = 0; x < 160; x++) {
        uint16_t color = fixPal[pixels[x] & 0x3F];
        uint32_t pair = (uint32_t)color | ((uint32_t)color << 16);
        row1[x] = pair;
        row2[x] = pair;
    }
}

// ─── Renderizado de Línea Ultra-Rápido Especializado para DMG (Clásico) ──────
static void lcd_draw_line_dmg(struct gb_s *gb, const uint8_t *pixels, const uint_fast8_t line) {
    (void)gb;
    if (!s_fbPtr || line >= 144) return;

    int y1 = line * 2;
    int y2 = y1 + 1;
    uint32_t* row1 = (uint32_t*)(s_fbPtr + (y1 * 320));
    uint32_t* row2 = (uint32_t*)(s_fbPtr + (y2 * 320));

    for (int x = 0; x < 160; x++) {
        uint16_t color = s_palette[pixels[x] & 0x03];
        uint32_t pair = (uint32_t)color | ((uint32_t)color << 16);
        row1[x] = pair;
        row2[x] = pair;
    }
}

// ─── Callbacks de Audio MiniGB APU ──────────────────────────────────────────
extern "C" uint8_t audio_read(const uint_fast16_t addr) {
    return minigb_apu_audio_read(&s_apu, addr);
}

extern "C" void audio_write(const uint_fast16_t addr, const uint8_t val) {
    minigb_apu_audio_write(&s_apu, addr, val);
}

// ─── Inicialización de I2S para Audio DMA ───────────────────────────────────
static bool initI2S() {
    i2s_config_t cfg = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate          = (uint32_t)AUDIO_SAMPLE_RATE,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count        = 4,
        .dma_buf_len          = 256,
        .use_apll             = false,
        .tx_desc_auto_clear   = true,
        .fixed_mclk           = 0
    };

    i2s_pin_config_t pins = {
        .bck_io_num   = 42,
        .ws_io_num    = 2,
        .data_out_num = 41,
        .data_in_num  = I2S_PIN_NO_CHANGE
    };

    if (i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL) != ESP_OK) {
        Serial.println("[GBC Audio] Error al instalar driver I2S");
        return false;
    }
    if (i2s_set_pin(I2S_NUM_0, &pins) != ESP_OK) {
        Serial.println("[GBC Audio] Error al configurar pines I2S");
        return false;
    }
    Serial.printf("[GBC Audio] I2S listo a %d Hz (BCLK=42, LRC=2, DOUT=41)\n", AUDIO_SAMPLE_RATE);
    return true;
}

// ─── Guardar y Cargar Partida (SRAM) ────────────────────────────────────────
static void loadSaveFile(const String& romPath) {
    int dotIdx = romPath.lastIndexOf('.');
    if (dotIdx > 0) {
        s_currentSavePath = romPath.substring(0, dotIdx) + ".sav";
    } else {
        s_currentSavePath = romPath + ".sav";
    }

    if (SD.exists(s_currentSavePath.c_str())) {
        File f = SD.open(s_currentSavePath.c_str(), FILE_READ);
        if (f) {
            size_t readBytes = f.read(s_cartRam, s_cartRamSize);
            f.close();
            Serial.printf("[GBC Save] Partida cargada: %s (%u bytes)\n", s_currentSavePath.c_str(), (unsigned)readBytes);
        }
    }
}

static void saveGame() {
    if (!s_cartRamDirty || s_currentSavePath.isEmpty() || !s_cartRam) return;
    File f = SD.open(s_currentSavePath.c_str(), FILE_WRITE);
    if (f) {
        f.write(s_cartRam, s_cartRamSize);
        f.close();
        s_cartRamDirty = false;
        Serial.printf("[GBC Save] Partida guardada con exito: %s\n", s_currentSavePath.c_str());
    }
}

// ─── Función auxiliar para agregar ROMs de un directorio ───────────────────
static void scanDir(const String& path, std::vector<String>& roms) {
    if (!SD.exists(path.c_str())) return;
    File dir = SD.open(path.c_str());
    if (!dir) return;

    File f = dir.openNextFile();
    while (f) {
        if (!f.isDirectory()) {
            String name = f.name();
            String fullPath = "";
            if (name.startsWith("/")) {
                fullPath = name;
            } else {
                String cleanPath = path;
                if (!cleanPath.endsWith("/")) cleanPath += "/";
                fullPath = cleanPath + name;
            }

            String lower = fullPath;
            lower.toLowerCase();
            if (lower.endsWith(".gb") || lower.endsWith(".gbc") || lower.endsWith(".cgb")) {
                bool exists = false;
                for (const auto& r : roms) {
                    if (r == fullPath) { exists = true; break; }
                }
                if (!exists) roms.push_back(fullPath);
            }
        }
        f.close(); // Cierra el descriptor para no saturar el driver FatFS
        f = dir.openNextFile();
    }
    dir.close();
}

// ─── Escaneo exhaustivo de ROMs en MicroSD ──────────────────────────────────
static std::vector<String> scanROMs() {
    std::vector<String> roms;
    scanDir("/roms/gbc", roms);
    scanDir("/roms/gb", roms);
    scanDir("/roms/gameboy", roms);
    scanDir("/roms", roms);
    scanDir("/games", roms);
    scanDir("/download", roms);
    scanDir("/", roms);

    Serial.printf("[GBC Scanner] Juegos encontrados en SD: %d\n", (int)roms.size());
    for (size_t i = 0; i < roms.size(); i++) {
        Serial.printf("  [%d] %s\n", (int)i, roms[i].c_str());
    }
    return roms;
}

// ─── Selector Visual de Juegos Táctil (Siempre Visible) ─────────────────────
static String selectROM(const std::vector<String>& roms) {
    if (roms.empty()) return "";

    int page = 0;
    const int itemsPerPage = 5;
    int totalPages = (roms.size() + itemsPerPage - 1) / itemsPerPage;
    if (totalPages == 0) totalPages = 1;

    while (true) {
        // 1. Dibujar Interfaz de Selección
        s_fastCanvas->fillScreen(0x10A2); // Fondo azul oscuro retro

        // Encabezado
        s_fastCanvas->fillRect(0, 0, 320, 48, 0x0188);
        s_fastCanvas->setTextSize(2);
        s_fastCanvas->setTextColor(0xFFE0); // Amarillo arcade
        s_fastCanvas->setCursor(16, 14);
        s_fastCanvas->println("SELECCIONA JUEGO");

        // Botón Salir
        s_fastCanvas->fillRect(240, 8, 70, 32, 0xB000);
        s_fastCanvas->setTextSize(1);
        s_fastCanvas->setTextColor(0xFFFF);
        s_fastCanvas->setCursor(252, 20);
        s_fastCanvas->println("SALIR");

        // Lista de juegos
        int startIdx = page * itemsPerPage;
        for (int i = 0; i < itemsPerPage; i++) {
            int idx = startIdx + i;
            int y = 60 + (i * 68);

            if (idx < (int)roms.size()) {
                // Tarjeta de juego
                s_fastCanvas->fillRoundRect(12, y, 296, 56, 8, 0x2124);
                s_fastCanvas->drawRoundRect(12, y, 296, 56, 8, 0x4A69);

                // Nombre limpio del archivo
                String displayName = roms[idx];
                int lastSlash = displayName.lastIndexOf('/');
                if (lastSlash >= 0) displayName = displayName.substring(lastSlash + 1);
                if (displayName.length() > 24) displayName = displayName.substring(0, 22) + "..";

                // Icono CGB / DMG
                bool isCGB = displayName.endsWith(".gbc") || displayName.endsWith(".GBC");
                s_fastCanvas->fillRoundRect(22, y + 10, 40, 36, 4, isCGB ? 0xA01F : 0x05E0);
                s_fastCanvas->setTextSize(1);
                s_fastCanvas->setTextColor(0xFFFF);
                s_fastCanvas->setCursor(26, y + 24);
                s_fastCanvas->println(isCGB ? "CGB" : "DMG");

                // Texto del título
                s_fastCanvas->setTextSize(1);
                s_fastCanvas->setTextColor(0xFFFF);
                s_fastCanvas->setCursor(70, y + 22);
                s_fastCanvas->println(displayName);
            }
        }

        // Barra inferior de paginación
        s_fastCanvas->fillRect(0, 420, 320, 60, 0x0188);

        if (page > 0) {
            s_fastCanvas->fillRoundRect(16, 430, 90, 40, 6, 0x3186);
            s_fastCanvas->setTextSize(1);
            s_fastCanvas->setTextColor(0xFFFF);
            s_fastCanvas->setCursor(32, 444);
            s_fastCanvas->println("< ANTERIOR");
        }

        s_fastCanvas->setTextSize(1);
        s_fastCanvas->setTextColor(0xFFFF);
        s_fastCanvas->setCursor(125, 444);
        s_fastCanvas->printf("Pag %d/%d", page + 1, totalPages);

        if (page < totalPages - 1) {
            s_fastCanvas->fillRoundRect(214, 430, 90, 40, 6, 0x3186);
            s_fastCanvas->setTextSize(1);
            s_fastCanvas->setTextColor(0xFFFF);
            s_fastCanvas->setCursor(226, 444);
            s_fastCanvas->println("SIGUIENTE >");
        }

        s_display.flush();

        // 2. Esperar toque del usuario
        while (true) {
            TouchPoint p;
            if (s_touch.read(p)) {
                // Tocar botón Salir
                if (p.x >= 230 && p.x <= 320 && p.y >= 0 && p.y <= 50) {
                    CartridgeGamepad::exitToOS();
                    return "";
                }

                // Tocar botón Anterior
                if (page > 0 && p.x >= 10 && p.x <= 110 && p.y >= 420 && p.y <= 480) {
                    page--;
                    delay(200);
                    break;
                }

                // Tocar botón Siguiente
                if (page < totalPages - 1 && p.x >= 210 && p.x <= 310 && p.y >= 420 && p.y <= 480) {
                    page++;
                    delay(200);
                    break;
                }

                // Tocar alguna fila de juego
                for (int i = 0; i < itemsPerPage; i++) {
                    int idx = startIdx + i;
                    int y = 60 + (i * 68);
                    if (idx < (int)roms.size() && p.x >= 10 && p.x <= 310 && p.y >= y && p.y <= (y + 56)) {
                        // Feedback visual de selección
                        s_fastCanvas->fillRoundRect(12, y, 296, 56, 8, 0x07E0);
                        s_display.flush();
                        delay(150);
                        return roms[idx];
                    }
                }
            }
            delay(30);
        }
    }
}

// ─── Pantalla de Error Gráfica ──────────────────────────────────────────────
static void showErrorMessage(const char* title, const char* msg) {
    if (!s_fastCanvas) return;
    s_fastCanvas->fillScreen(0x0000);
    s_fastCanvas->setTextSize(2);
    s_fastCanvas->setTextColor(0xF800);
    s_fastCanvas->setCursor(20, 40);
    s_fastCanvas->println(title);

    s_fastCanvas->setTextSize(1);
    s_fastCanvas->setTextColor(0xFFFF);
    s_fastCanvas->setCursor(20, 80);
    s_fastCanvas->println(msg);

    s_fastCanvas->setCursor(20, 240);
    s_fastCanvas->setTextColor(0x07E0);
    s_fastCanvas->println("Toca [SALIR] para volver a espOS32");

    s_gamepad.draw(true);
    s_display.flush();

    while (1) {
        s_gamepad.read();
        if (s_gamepad.handleExit()) return;
        delay(50);
    }
}

// ─── SETUP ──────────────────────────────────────────────────────────────────
void setup() {
    setCpuFrequencyMhz(240);

    Serial.begin(115200);
    delay(200);
    Serial.println("\n==========================================");
    Serial.println("  Game Boy & Game Boy Color (Con Selector) ");
    Serial.printf("  CPU: %u MHz | PSRAM: %u KB              \n", 
                  (unsigned)getCpuFrequencyMhz(), (unsigned)(ESP.getFreePsram() / 1024));
    Serial.println("==========================================");

    // 1. Inicializar Display en Portrait 320x480 con driver oficial
    if (!s_display.begin()) {
        Serial.println("[GBC] Error al inicializar display!");
        while (1) delay(1000);
    }
    s_display.setRotation(ROTATION_0); // Portrait 320x480
    s_display.backlightOn();
    s_fastCanvas = (FastGameCanvas*)s_display.getCanvas();
    if (s_fastCanvas) {
        s_fbPtr = s_fastCanvas->getFramebuffer();
    }

    // 2. Inicializar Touch y Gamepad
    s_touch.begin();
    s_display.setTouchRotation(&s_touch);
    s_gamepad.begin(&s_display, &s_touch, LAYOUT_PORTRAIT_GBC);

    // 3. Inicializar MicroSD robusta (HSPI dedicada igual que en DOOM)
    pinMode(10, OUTPUT);
    digitalWrite(10, HIGH);
    delay(50);

    static SPIClass s_sdSPI(HSPI);
    s_sdSPI.begin(12, 13, 11, 10); // SCK=12, MISO=13, MOSI=11, CS=10

    bool sdMounted = false;
    for (int retry = 0; retry < 5 && !sdMounted; retry++) {
        sdMounted = SD.begin(10, s_sdSPI, 10000000);
        if (!sdMounted) {
            sdMounted = SD.begin(10, s_sdSPI, 4000000);
        }
        if (!sdMounted) delay(100);
    }

    if (!sdMounted) {
        Serial.println("[GBC] Error: MicroSD no detectada!");
        showErrorMessage("ERROR MICROSD", "No se detecta la tarjeta SD.\nInserta una tarjeta formateada en FAT32.");
        return;
    }
    Serial.println("[GBC] MicroSD montada correctamente.");

    // 4. Buscar y Seleccionar ROM de Game Boy / Game Boy Color
    std::vector<String> romList = scanROMs();
    if (romList.empty()) {
        Serial.println("[GBC] No se encontraron archivos .gb o .gbc");
        showErrorMessage("ROM NO ENCONTRADA", "Coloca tus juegos (.gb / .gbc)\nen la carpeta:\n/sd/roms/gbc/\no en la raiz de la MicroSD.");
        return;
    }

    String romPath = selectROM(romList);
    if (romPath.isEmpty()) {
        return;
    }
    Serial.printf("[GBC] ROM seleccionada: %s\n", romPath.c_str());

    // 5. Cargar ROM 100% en PSRAM
    File romFile = SD.open(romPath.c_str(), FILE_READ);
    if (!romFile) {
        Serial.printf("[GBC] Error al abrir archivo en SD: %s\n", romPath.c_str());
        showErrorMessage("ERROR AL ABRIR", ("No se pudo abrir el archivo:\n" + romPath).c_str());
        return;
    }

    s_romSize = romFile.size();
    Serial.printf("[GBC] Tamano de la ROM: %u bytes (%.2f MB)\n", (unsigned)s_romSize, s_romSize / (1024.0 * 1024.0));

    s_romData = (uint8_t*)heap_caps_malloc(s_romSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_romData) {
        romFile.close();
        showErrorMessage("ERROR MEMORIA", "No hay suficiente PSRAM libre para cargar la ROM.");
        return;
    }

    size_t bytesRead = romFile.read(s_romData, s_romSize);
    romFile.close();
    Serial.printf("[GBC] %u bytes leidos en PSRAM con exito.\n", (unsigned)bytesRead);

    // 6. Asignar memoria para SRAM (Partidas guardadas)
    s_cartRam = (uint8_t*)heap_caps_calloc(1, s_cartRamSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    loadSaveFile(romPath);

    // 7. Inicializar Audio I2S y APU
    initI2S();
    minigb_apu_audio_init(&s_apu);

    // 8. Inicializar Walnut-CGB
    enum gb_init_error_e gb_err = gb_init(
        &s_gb,
        gb_rom_read,
        gb_rom_read16,
        gb_rom_read32,
        gb_cart_ram_read,
        gb_cart_ram_write,
        gb_error,
        NULL
    );

    if (gb_err != GB_INIT_NO_ERROR) {
        Serial.printf("[GBC] Error al inicializar Walnut-CGB: %d\n", (int)gb_err);
        showErrorMessage("ERROR GBC CORE", "El formato del archivo ROM no es valido.");
        return;
    }

    // Registrar función de render especializada según el modo
    if (s_gb.cgb.cgbMode) {
        gb_init_lcd(&s_gb, lcd_draw_line_cgb);
    } else {
        gb_init_lcd(&s_gb, lcd_draw_line_dmg);
    }

    s_gb.direct.joypad = 0xFF;

    Serial.printf("[GBC] Modo detectado: %s\n", s_gb.cgb.cgbMode ? "GAME BOY COLOR (CGB)" : "GAME BOY CLASICO (DMG)");

    // Limpiar pantalla y dibujar Gamepad inferior completo
    s_fastCanvas->fillScreen(0x0000);
    s_gamepad.draw(true);
    s_display.flush();

    Serial.println("[GBC] Motor listo. Arrancando emulacion a 60 FPS...");
}

// ─── LOOP PRINCIPAL ULTRA OPTIMIZADO (60 FPS) ───────────────────────────────
void loop() {
    // 1. Leer Controles Táctiles cada 2 frames
    static uint8_t s_touchDiv = 0;
    if (++s_touchDiv >= 2) {
        s_touchDiv = 0;
        uint16_t btns = s_gamepad.read();
        if (s_gamepad.handleExit()) {
            saveGame();
            CartridgeGamepad::exitToOS();
            return;
        }

        uint8_t joypad = 0xFF;
        if (btns & PAD_A)      joypad &= ~JOYPAD_A;
        if (btns & PAD_B)      joypad &= ~JOYPAD_B;
        if (btns & PAD_SELECT) joypad &= ~JOYPAD_SELECT;
        if (btns & PAD_START)  joypad &= ~JOYPAD_START;
        if (btns & PAD_RIGHT)  joypad &= ~JOYPAD_RIGHT;
        if (btns & PAD_LEFT)   joypad &= ~JOYPAD_LEFT;
        if (btns & PAD_UP)     joypad &= ~JOYPAD_UP;
        if (btns & PAD_DOWN)   joypad &= ~JOYPAD_DOWN;
        s_gb.direct.joypad = joypad;
    }

    // 2. Ejecutar 1 Frame de emulación
    gb_run_frame_dualfetch(&s_gb);

    // 3. Sintetizar y enviar Audio I2S DMA
    minigb_apu_audio_callback(&s_apu, s_audioBuffer);
    size_t written = 0;
    i2s_write(I2S_NUM_0, (const char*)s_audioBuffer, sizeof(s_audioBuffer), &written, 0);

    // 4. Refrescar ÚNICAMENTE el área de juego (320x288) para máxima velocidad
    if (s_fastCanvas) {
        s_fastCanvas->flushGameArea();
    } else {
        s_display.flush();
    }
}
