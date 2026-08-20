#include "cbdos/storage.hpp"
#include <Arduino.h>
#include <FS.h>
#include <SD.h>

namespace cbdos {
namespace storage {

static bool s_sdMounted = false;

bool init() {
    return true;
}

bool isSdMounted() {
    return s_sdMounted;
}

StorageStats getFlashStats() {
    StorageStats stats = { true, 0, 0, 0 };
    stats.totalBytes = ESP.getFlashChipSize();
    if (stats.totalBytes == 0) stats.totalBytes = 16 * 1024 * 1024;
    
    // Partición aproximada / uso
    stats.usedBytes = ESP.getSketchSize();
    stats.freeBytes = stats.totalBytes > stats.usedBytes ? (stats.totalBytes - stats.usedBytes) : 0;
    return stats;
}

StorageStats getSdCardStats() {
    StorageStats stats = { s_sdMounted, 0, 0, 0 };
    if (s_sdMounted) {
        stats.totalBytes = SD.totalBytes();
        stats.usedBytes = SD.usedBytes();
        stats.freeBytes = stats.totalBytes > stats.usedBytes ? (stats.totalBytes - stats.usedBytes) : 0;
    }
    return stats;
}

std::vector<FileEntry> listDir(const char* path) {
    return {};
}

bool fileExists(const char* path) {
    return false;
}

size_t getFreeBytes(StorageType type) {
    if (type == StorageType::InternalFlash) return (size_t)getFlashStats().freeBytes;
    return (size_t)getSdCardStats().freeBytes;
}

size_t getTotalBytes(StorageType type) {
    if (type == StorageType::InternalFlash) return (size_t)getFlashStats().totalBytes;
    return (size_t)getSdCardStats().totalBytes;
}

} // namespace storage
} // namespace cbdos
