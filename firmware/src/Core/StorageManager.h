#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <cstddef>

enum class StorageType {
    SD_CARD,
    FLASH_FS
};

struct StorageFileInfo {
    std::string name;
    std::string path;
    bool isDirectory;
    size_t size;
};

class StorageManager {
public:
    static bool init();
    static bool isSdAvailable();
    static bool isFlashAvailable();

    static uint64_t getSdTotalBytes();
    static uint64_t getSdUsedBytes();
    static uint64_t getFlashTotalBytes();
    static uint64_t getFlashUsedBytes();

    static std::vector<StorageFileInfo> listDirectory(StorageType storage, const std::string& path);
    static bool deleteFile(StorageType storage, const std::string& path);
    static bool deleteDirectory(StorageType storage, const std::string& path);
    static std::string readFilePreview(StorageType storage, const std::string& path, size_t maxBytes = 4096);
    static std::string readFile(StorageType storage, const std::string& path);
    static bool writeFile(StorageType storage, const std::string& path, const std::string& content);
    static std::string formatBytes(uint64_t bytes);
};
