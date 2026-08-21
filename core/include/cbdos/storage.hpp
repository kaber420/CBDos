#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace cbdos {
namespace storage {

enum class StorageType {
    InternalFlash,
    SdCard,
    UsbDrive
};

struct FileEntry {
    std::string name;
    size_t size;
    bool isDirectory;
};

struct StorageStats {
    bool isMounted;
    uint64_t totalBytes;
    uint64_t usedBytes;
    uint64_t freeBytes;
    std::string label;
};

bool init();
bool mountSd();
bool unmountSd();
bool isSdMounted();

StorageStats getFlashStats();
StorageStats getSdCardStats();
StorageStats getUsbStats();

std::vector<FileEntry> listDir(const char* path);
bool fileExists(const char* path);
std::string readFile(const char* path);
bool writeFile(const char* path, const std::string& content);
bool deleteFile(const char* path);
size_t getFreeBytes(StorageType type);
size_t getTotalBytes(StorageType type);

} // namespace storage
} // namespace cbdos

