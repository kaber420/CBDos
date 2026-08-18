#include "WallpaperManager.h"
#include "Assets/default_wallpaper.h"
#include <cstdio>
#include <cstring>

#if defined(ARDUINO)
#include <Preferences.h>
#include <SD.h>
#include <LittleFS.h>
#include <esp_heap_caps.h>
#include "../Core/LVFS_Driver.h"
#endif

static const size_t WALLPAPER_WIDTH = 320;
static const size_t WALLPAPER_HEIGHT = 480;
static const size_t WALLPAPER_BUFFER_SIZE = WALLPAPER_WIDTH * WALLPAPER_HEIGHT * 2; // 307200 bytes

WallpaperManager::WallpaperManager() 
    : currentPath("default"), hasCustomWallpaper(false), customBuffer(nullptr) {
    memset(&customDsc, 0, sizeof(customDsc));
}

WallpaperManager::~WallpaperManager() {
    if (customBuffer) {
        free(customBuffer);
        customBuffer = nullptr;
    }
}

bool WallpaperManager::ensureBufferAllocated() {
    if (customBuffer) return true;
#if defined(ARDUINO)
    customBuffer = (uint8_t*)heap_caps_malloc(WALLPAPER_BUFFER_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!customBuffer) {
        customBuffer = (uint8_t*)malloc(WALLPAPER_BUFFER_SIZE);
    }
#else
    customBuffer = (uint8_t*)malloc(WALLPAPER_BUFFER_SIZE);
#endif
    if (!customBuffer) {
#if defined(ARDUINO)
        Serial.println("[Wallpaper] ERROR: No se pudo asignar memoria para el fondo en PSRAM");
#endif
        return false;
    }

    customDsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    customDsc.header.cf = LV_COLOR_FORMAT_RGB565;
    customDsc.header.flags = 0;
    customDsc.header.w = WALLPAPER_WIDTH;
    customDsc.header.h = WALLPAPER_HEIGHT;
    customDsc.header.stride = WALLPAPER_WIDTH * 2;
    customDsc.header.reserved_2 = 0;
    customDsc.data_size = WALLPAPER_BUFFER_SIZE;
    customDsc.data = customBuffer;
    customDsc.reserved = NULL;
    return true;
}

bool WallpaperManager::loadFromFlash() {
#if defined(ARDUINO)
    if (!LittleFS.exists("/wallpaper.bin")) {
        return false;
    }
    File f = LittleFS.open("/wallpaper.bin", "r");
    if (!f) {
        Serial.println("[Wallpaper] No se pudo abrir /wallpaper.bin desde LittleFS");
        return false;
    }
    if (f.size() < WALLPAPER_BUFFER_SIZE) {
        Serial.printf("[Wallpaper] /wallpaper.bin tamaño invalido: %u\n", (unsigned int)f.size());
        f.close();
        return false;
    }
    if (!ensureBufferAllocated()) {
        f.close();
        return false;
    }
    size_t bytesRead = f.read(customBuffer, WALLPAPER_BUFFER_SIZE);
    f.close();
    if (bytesRead == WALLPAPER_BUFFER_SIZE) {
        hasCustomWallpaper = true;
        Serial.println("[Wallpaper] Fondo cargado exitosamente desde LittleFS a PSRAM");
        return true;
    }
#endif
    return false;
}

void WallpaperManager::init() {
#if defined(ARDUINO)
    Preferences prefs;
    bool isCustom = false;
    if (prefs.begin("wallpaper", true)) {
        isCustom = prefs.getBool("custom", false);
        String p = prefs.getString("path", "default");
        currentPath = p.c_str();
        prefs.end();
    }
    if (isCustom) {
        if (!loadFromFlash()) {
            hasCustomWallpaper = false;
            currentPath = "default";
        }
    } else {
        hasCustomWallpaper = false;
        currentPath = "default";
    }
#else
    hasCustomWallpaper = false;
    currentPath = "default";
#endif
}

void WallpaperManager::applyWallpaper(lv_obj_t* parent) {
    if (!parent) return;

    if (hasCustomWallpaper && customBuffer) {
        lv_obj_set_style_bg_image_src(parent, &customDsc, 0);
    } else {
        lv_obj_set_style_bg_image_src(parent, &default_wallpaper, 0);
    }
    lv_obj_set_style_bg_image_opa(parent, LV_OPA_COVER, 0);
}

bool WallpaperManager::setWallpaper(const std::string& path) {
#if defined(ARDUINO)
    if (path == "default" || path.empty()) {
        restoreDefault();
        return true;
    }

    std::string sdPath = path;
    if (sdPath.rfind("A:/", 0) == 0) {
        sdPath = sdPath.substr(2); // Quitar "A:"
    }
    if (sdPath.rfind("S:/", 0) == 0) {
        sdPath = sdPath.substr(2); // Quitar "S:"
    }

    if (!ensureBufferAllocated()) {
        return false;
    }

    lv_fs_spi_lock();
    File f = SD.open(sdPath.c_str(), "r");
    if (!f) {
        lv_fs_spi_unlock();
        Serial.printf("[Wallpaper] ERROR: No se pudo abrir %s en SD\n", sdPath.c_str());
        return false;
    }

    size_t fSize = f.size();
    std::string lowerPath = sdPath;
    for (auto& c : lowerPath) c = tolower(c);

    bool loadSuccess = false;

    if (lowerPath.rfind(".bin") != std::string::npos) {
        if (fSize == WALLPAPER_BUFFER_SIZE + 12) {
            // Tiene cabecera LVGL 9 de 12 bytes
            f.seek(12);
            size_t rd = f.read(customBuffer, WALLPAPER_BUFFER_SIZE);
            loadSuccess = (rd == WALLPAPER_BUFFER_SIZE);
        } else if (fSize >= WALLPAPER_BUFFER_SIZE) {
            size_t rd = f.read(customBuffer, WALLPAPER_BUFFER_SIZE);
            loadSuccess = (rd == WALLPAPER_BUFFER_SIZE);
        }
    } else if (lowerPath.rfind(".bmp") != std::string::npos) {
        // Soporte de decodificación BMP 24 bits 320x480 directo a RGB565
        uint8_t header[54];
        if (f.read(header, 54) == 54 && header[0] == 'B' && header[1] == 'M') {
            uint32_t dataOffset = header[10] | (header[11] << 8) | (header[12] << 16) | (header[13] << 24);
            int32_t w = header[18] | (header[19] << 8) | (header[20] << 16) | (header[21] << 24);
            int32_t h = header[22] | (header[23] << 8) | (header[24] << 16) | (header[25] << 24);
            uint16_t bpp = header[28] | (header[29] << 8);

            if (w == (int32_t)WALLPAPER_WIDTH && abs(h) == (int32_t)WALLPAPER_HEIGHT && bpp == 24) {
                f.seek(dataOffset);
                bool topDown = (h < 0);
                uint8_t rowBuf[WALLPAPER_WIDTH * 3]; // 960 bytes por fila
                loadSuccess = true;

                for (int y = 0; y < (int)WALLPAPER_HEIGHT; y++) {
                    int targetY = topDown ? y : (WALLPAPER_HEIGHT - 1 - y);
                    if (f.read(rowBuf, sizeof(rowBuf)) != sizeof(rowBuf)) {
                        loadSuccess = false;
                        break;
                    }
                    uint16_t* destRow = (uint16_t*)(customBuffer + targetY * WALLPAPER_WIDTH * 2);
                    for (int x = 0; x < (int)WALLPAPER_WIDTH; x++) {
                        uint8_t b = rowBuf[x * 3 + 0];
                        uint8_t g = rowBuf[x * 3 + 1];
                        uint8_t r = rowBuf[x * 3 + 2];
                        uint16_t rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
                        destRow[x] = rgb565;
                    }
                }
            }
        }
    }

    f.close();
    lv_fs_spi_unlock();

    if (!loadSuccess) {
        Serial.println("[Wallpaper] ERROR: Formato o tamaño de imagen no soportado para copia rápida");
        return false;
    }

    // Guardar en LittleFS para persistencia permanente en Flash interna
    File flashFile = LittleFS.open("/wallpaper.bin", "w");
    if (flashFile) {
        flashFile.write(customBuffer, WALLPAPER_BUFFER_SIZE);
        flashFile.close();
        Serial.println("[Wallpaper] Fondo copiado y guardado exitosamente en LittleFS (/wallpaper.bin)");
    } else {
        Serial.println("[Wallpaper] WARN: No se pudo guardar en LittleFS Flash");
    }

    hasCustomWallpaper = true;
    currentPath = path;

    Preferences prefs;
    if (prefs.begin("wallpaper", false)) {
        prefs.putBool("custom", true);
        prefs.putString("path", currentPath.c_str());
        prefs.end();
    }

    return true;
#else
    return false;
#endif
}

void WallpaperManager::restoreDefault() {
    hasCustomWallpaper = false;
    currentPath = "default";
#if defined(ARDUINO)
    if (LittleFS.exists("/wallpaper.bin")) {
        LittleFS.remove("/wallpaper.bin");
    }
    Preferences prefs;
    if (prefs.begin("wallpaper", false)) {
        prefs.putBool("custom", false);
        prefs.putString("path", "default");
        prefs.end();
    }
#endif
}

std::vector<std::string> WallpaperManager::getAvailableWallpapers() {
    std::vector<std::string> list;
#if defined(ARDUINO)
    lv_fs_spi_lock();
    File dir = SD.open("/wallpapers");
    if (dir && dir.isDirectory()) {
        File file = dir.openNextFile();
        while (file) {
            if (!file.isDirectory()) {
                String fname = file.name();
                String lower = fname;
                lower.toLowerCase();
                if (lower.endsWith(".bin") || lower.endsWith(".bmp") || lower.endsWith(".jpg") || lower.endsWith(".jpeg") || lower.endsWith(".png")) {
                    String fullPath = "A:/wallpapers/" + fname;
                    list.push_back(fullPath.c_str());
                }
            }
            file = dir.openNextFile();
        }
        dir.close();
    }
    lv_fs_spi_unlock();
#endif
    return list;
}
