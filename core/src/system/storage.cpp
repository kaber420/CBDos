#include "cbdos/storage.hpp"

namespace cbdos {
namespace storage {

static IStorageBackend* s_backend = nullptr;

void setBackend(IStorageBackend* backend) {
    s_backend = backend;
}

IStorageBackend* getBackend() {
    return s_backend;
}

bool init() {
    if (s_backend) {
        return s_backend->init();
    }
    return false;
}

bool mountSd() {
    if (s_backend) {
        return s_backend->mountSd();
    }
    return false;
}

bool unmountSd() {
    if (s_backend) {
        return s_backend->unmountSd();
    }
    return false;
}

bool formatSd() {
    if (s_backend) {
        return s_backend->formatSd();
    }
    return false;
}

bool isSdMounted() {
    if (s_backend) {
        return s_backend->isSdMounted();
    }
    return false;
}

bool isFlashMounted() {
    if (s_backend) {
        return s_backend->isFlashMounted();
    }
    return false;
}

StorageStats getFlashStats() {
    if (s_backend) {
        return s_backend->getFlashStats();
    }
    return StorageStats{ false, 0, 0, 0, "Flash Interna" };
}

StorageStats getSdCardStats() {
    if (s_backend) {
        return s_backend->getSdCardStats();
    }
    return StorageStats{ false, 0, 0, 0, "MicroSD" };
}

StorageStats getUsbStats() {
    if (s_backend) {
        return s_backend->getUsbStats();
    }
    return StorageStats{ false, 0, 0, 0, "USB Drive" };
}

std::vector<FileEntry> listDir(const char* path, bool includeDeleted) {
    if (s_backend) {
        return s_backend->listDir(path, includeDeleted);
    }
    return {};
}

bool fileExists(const char* path) {
    if (s_backend) {
        return s_backend->fileExists(path);
    }
    return false;
}

std::string readFile(const char* path) {
    if (s_backend) {
        return s_backend->readFile(path);
    }
    return "";
}

bool writeFile(const char* path, const std::string& content) {
    if (s_backend) {
        return s_backend->writeFile(path, content);
    }
    return false;
}

bool deleteFile(const char* path) {
    if (s_backend) {
        return s_backend->deleteFile(path);
    }
    return false;
}

bool copyFile(const char* srcPath, const char* dstPath) {
    if (s_backend) {
        return s_backend->copyFile(srcPath, dstPath);
    }
    return false;
}

bool makeDir(const char* path) {
    if (s_backend) {
        return s_backend->makeDir(path);
    }
    return false;
}

bool recoverFile(const char* srcPath, const char* dstPath) {
    if (s_backend) {
        return s_backend->recoverFile(srcPath, dstPath);
    }
    return false;
}

bool undeleteFile(const char* path) {
    if (s_backend) {
        return s_backend->undeleteFile(path);
    }
    return false;
}

size_t getFreeBytes(StorageType type) {
    if (s_backend) {
        return s_backend->getFreeBytes(type);
    }
    return 0;
}

size_t getTotalBytes(StorageType type) {
    if (s_backend) {
        return s_backend->getTotalBytes(type);
    }
    return 0;
}

} // namespace storage
} // namespace cbdos
