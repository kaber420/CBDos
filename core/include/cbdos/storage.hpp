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

class IStorageBackend {
public:
    virtual ~IStorageBackend() = default;

    virtual bool init() = 0;
    virtual bool mountSd() = 0;
    virtual bool unmountSd() = 0;
    virtual bool isSdMounted() const = 0;
    virtual bool isFlashMounted() const = 0;

    virtual StorageStats getFlashStats() const = 0;
    virtual StorageStats getSdCardStats() const = 0;
    virtual StorageStats getUsbStats() const = 0;

    virtual std::vector<FileEntry> listDir(const char* path) = 0;
    virtual bool fileExists(const char* path) = 0;
    virtual std::string readFile(const char* path) = 0;
    virtual bool writeFile(const char* path, const std::string& content) = 0;
    virtual bool deleteFile(const char* path) = 0;
    virtual bool copyFile(const char* srcPath, const char* dstPath) = 0;
    virtual bool makeDir(const char* path) = 0;
    virtual size_t getFreeBytes(StorageType type) const = 0;
    virtual size_t getTotalBytes(StorageType type) const = 0;
};

void setBackend(IStorageBackend* backend);
IStorageBackend* getBackend();

// APIs públicas de conveniencia (mantienen 100% de compatibilidad)
bool init();
bool mountSd();
bool unmountSd();
bool isSdMounted();
bool isFlashMounted();

StorageStats getFlashStats();
StorageStats getSdCardStats();
StorageStats getUsbStats();

std::vector<FileEntry> listDir(const char* path);
bool fileExists(const char* path);
std::string readFile(const char* path);
bool writeFile(const char* path, const std::string& content);
bool deleteFile(const char* path);
bool copyFile(const char* srcPath, const char* dstPath);
bool makeDir(const char* path);
size_t getFreeBytes(StorageType type);
size_t getTotalBytes(StorageType type);

} // namespace storage
} // namespace cbdos

