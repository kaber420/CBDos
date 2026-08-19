#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace cbdos {
namespace storage {

enum class StorageType {
    InternalFlash,
    SdCard
};

struct FileEntry {
    std::string name;
    size_t size;
    bool isDirectory;
};

bool init();
bool isSdMounted();
std::vector<FileEntry> listDir(const char* path);
bool fileExists(const char* path);
size_t getFreeBytes(StorageType type);
size_t getTotalBytes(StorageType type);

} // namespace storage
} // namespace cbdos
