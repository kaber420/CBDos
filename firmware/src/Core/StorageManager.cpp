#include "StorageManager.h"
#include "LVFS_Driver.h"
#include <algorithm>
#include <cstdio>

#ifdef ARDUINO
#include <Arduino.h>
#include <SD.h>
#include <FS.h>
#include <LittleFS.h>

static bool s_flashMounted = false;

#endif

bool StorageManager::init() {
#ifdef ARDUINO
    // Inicializar LittleFS en la partición spiffs de la Flash interna
    if (!s_flashMounted) {
        if (LittleFS.begin(true, "/spiffs", 10, "spiffs")) {
            s_flashMounted = true;
            Serial.printf("[StorageManager] Flash LittleFS montado correctamente. Total: %llu bytes, Usado: %llu bytes\n",
                          (unsigned long long)LittleFS.totalBytes(), (unsigned long long)LittleFS.usedBytes());
        } else {
            Serial.println("[StorageManager] WARN: No se pudo montar Flash LittleFS");
        }
    }
    return true;
#else
    return true;
#endif
}

bool StorageManager::isSdAvailable() {
#ifdef ARDUINO
    return (SD.cardType() != CARD_NONE);
#else
    return true;
#endif
}

bool StorageManager::isFlashAvailable() {
#ifdef ARDUINO
    return s_flashMounted;
#else
    return true;
#endif
}

uint64_t StorageManager::getSdTotalBytes() {
#ifdef ARDUINO
    if (isSdAvailable()) {
        lv_fs_spi_lock();
        uint64_t total = SD.totalBytes();
        lv_fs_spi_unlock();
        return total;
    }
    return 0;
#else
    return 16ULL * 1024 * 1024 * 1024; // Mock 16GB
#endif
}

uint64_t StorageManager::getSdUsedBytes() {
#ifdef ARDUINO
    if (isSdAvailable()) {
        lv_fs_spi_lock();
        uint64_t used = SD.usedBytes();
        lv_fs_spi_unlock();
        return used;
    }
    return 0;
#else
    return 2ULL * 1024 * 1024 * 1024; // Mock 2GB
#endif
}

uint64_t StorageManager::getFlashTotalBytes() {
#ifdef ARDUINO
    if (s_flashMounted) {
        return LittleFS.totalBytes();
    }
    return 0;
#else
    return 6ULL * 1024 * 1024; // Mock 6MB
#endif
}

uint64_t StorageManager::getFlashUsedBytes() {
#ifdef ARDUINO
    if (s_flashMounted) {
        return LittleFS.usedBytes();
    }
    return 0;
#else
    return 512 * 1024; // Mock 512KB
#endif
}

std::vector<StorageFileInfo> StorageManager::listDirectory(StorageType storage, const std::string& path) {
    std::vector<StorageFileInfo> result;
    std::string normPath = path;
    if (normPath.empty() || normPath[0] != '/') {
        normPath = "/" + normPath;
    }

#ifdef ARDUINO
    fs::FS* targetFS = nullptr;
    bool isSD = (storage == StorageType::SD_CARD);

    if (isSD) {
        if (!isSdAvailable()) return result;
        targetFS = &SD;
        lv_fs_spi_lock();
    } else {
        if (!s_flashMounted) return result;
        targetFS = &LittleFS;
    }

    File dir = targetFS->open(normPath.c_str());
    if (dir && dir.isDirectory()) {
        File entry = dir.openNextFile();
        while (entry) {
            std::string name = entry.name();
            // Filtrar nombres con rutas completas si el driver las entrega completas
            size_t slashIdx = name.rfind('/');
            if (slashIdx != std::string::npos) {
                name = name.substr(slashIdx + 1);
            }

            // Ignorar archivos del sistema ocultos comunes
            if (name != "." && name != ".." && name != "System Volume Information") {
                std::string fullEntryPath = (normPath == "/") ? ("/" + name) : (normPath + "/" + name);
                result.push_back({
                    name,
                    fullEntryPath,
                    entry.isDirectory(),
                    (size_t)entry.size()
                });
            }
            entry.close();
            entry = dir.openNextFile();
        }
        dir.close();
    }

    if (isSD) {
        lv_fs_spi_unlock();
    }
#else
    // Mock files para desarrollo en emulador
    if (normPath == "/") {
        result.push_back({"music", "/music", true, 0});
        result.push_back({"wallpapers", "/wallpapers", true, 0});
        result.push_back({"roms", "/roms", true, 0});
        result.push_back({"config.json", "/config.json", false, 1024});
        result.push_back({"readme.txt", "/readme.txt", false, 2048});
    }
#endif

    // Ordenar: primero carpetas (alfabéticamente), luego archivos (alfabéticamente)
    std::sort(result.begin(), result.end(), [](const StorageFileInfo& a, const StorageFileInfo& b) {
        if (a.isDirectory != b.isDirectory) {
            return a.isDirectory > b.isDirectory;
        }
        return a.name < b.name;
    });

    return result;
}

bool StorageManager::deleteFile(StorageType storage, const std::string& path) {
#ifdef ARDUINO
    bool ok = false;
    if (storage == StorageType::SD_CARD) {
        if (!isSdAvailable()) return false;
        lv_fs_spi_lock();
        ok = SD.remove(path.c_str());
        lv_fs_spi_unlock();
    } else {
        if (!s_flashMounted) return false;
        ok = LittleFS.remove(path.c_str());
    }
    return ok;
#else
    return true;
#endif
}

bool StorageManager::deleteDirectory(StorageType storage, const std::string& path) {
#ifdef ARDUINO
    bool ok = false;
    if (storage == StorageType::SD_CARD) {
        if (!isSdAvailable()) return false;
        lv_fs_spi_lock();
        ok = SD.rmdir(path.c_str());
        lv_fs_spi_unlock();
    } else {
        if (!s_flashMounted) return false;
        ok = LittleFS.rmdir(path.c_str());
    }
    return ok;
#else
    return true;
#endif
}

std::string StorageManager::readFilePreview(StorageType storage, const std::string& path, size_t maxBytes) {
    std::string content = "";
#ifdef ARDUINO
    fs::FS* targetFS = nullptr;
    bool isSD = (storage == StorageType::SD_CARD);

    if (isSD) {
        if (!isSdAvailable()) return "[SD no disponible]";
        targetFS = &SD;
        lv_fs_spi_lock();
    } else {
        if (!s_flashMounted) return "[Flash no disponible]";
        targetFS = &LittleFS;
    }

    File f = targetFS->open(path.c_str(), "r");
    if (f) {
        size_t toRead = std::min((size_t)f.size(), maxBytes);
        content.resize(toRead);
        f.read((uint8_t*)&content[0], toRead);
        f.close();
    } else {
        content = "[Error al abrir archivo]";
    }

    if (isSD) {
        lv_fs_spi_unlock();
    }
#else
    content = "Contenido de ejemplo para " + path;
#endif
    return content;
}

std::string StorageManager::readFile(StorageType storage, const std::string& path) {
    std::string content = "";
#ifdef ARDUINO
    fs::FS* targetFS = nullptr;
    bool isSD = (storage == StorageType::SD_CARD);

    if (isSD) {
        if (!isSdAvailable()) return "";
        targetFS = &SD;
        lv_fs_spi_lock();
    } else {
        if (!s_flashMounted) return "";
        targetFS = &LittleFS;
    }

    File f = targetFS->open(path.c_str(), "r");
    if (f) {
        size_t fileSize = f.size();
        if (fileSize > 0) {
            content.resize(fileSize);
            f.read((uint8_t*)&content[0], fileSize);
        }
        f.close();
    }

    if (isSD) {
        lv_fs_spi_unlock();
    }
#else
    content = "-- Archivo de ejemplo en simulacion\nprint('Hola CBDos!')\n";
#endif
    return content;
}

bool StorageManager::writeFile(StorageType storage, const std::string& path, const std::string& content) {
#ifdef ARDUINO
    fs::FS* targetFS = nullptr;
    bool isSD = (storage == StorageType::SD_CARD);

    if (isSD) {
        if (!isSdAvailable()) return false;
        targetFS = &SD;
        lv_fs_spi_lock();
    } else {
        if (!s_flashMounted) return false;
        targetFS = &LittleFS;
    }

    // Asegurar directorio padre si contiene subdirectorios
    size_t lastSlash = path.rfind('/');
    if (lastSlash != std::string::npos && lastSlash > 0) {
        std::string dirPath = path.substr(0, lastSlash);
        if (!targetFS->exists(dirPath.c_str())) {
            targetFS->mkdir(dirPath.c_str());
        }
    }

    File f = targetFS->open(path.c_str(), FILE_WRITE);
    bool ok = false;
    if (f) {
        if (!content.empty()) {
            size_t written = f.write((const uint8_t*)content.data(), content.size());
            ok = (written == content.size());
        } else {
            ok = true;
        }
        f.close();
    }

    if (isSD) {
        lv_fs_spi_unlock();
    }
    return ok;
#else
    return true;
#endif
}

std::string StorageManager::formatBytes(uint64_t bytes) {
    char buf[32];
    if (bytes < 1024) {
        snprintf(buf, sizeof(buf), "%llu B", (unsigned long long)bytes);
    } else if (bytes < 1024 * 1024) {
        snprintf(buf, sizeof(buf), "%.1f KB", (double)bytes / 1024.0);
    } else if (bytes < 1024ULL * 1024 * 1024) {
        snprintf(buf, sizeof(buf), "%.2f MB", (double)bytes / (1024.0 * 1024.0));
    } else {
        snprintf(buf, sizeof(buf), "%.2f GB", (double)bytes / (1024.0 * 1024.0 * 1024.0));
    }
    return std::string(buf);
}
