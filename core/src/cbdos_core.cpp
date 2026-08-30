#include "cbdos/system.hpp"
#include "cbdos/display.hpp"
#include "cbdos/input.hpp"
#include "cbdos/audio.hpp"
#include "cbdos/network.hpp"
#include "cbdos/storage.hpp"
#include "cbdos/persistence.hpp"
#include "cbdos/memory.hpp"
#include "cbdos/log.hpp"
#include "cbdos/rtos.hpp"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

// Punto de entrada y fallbacks débiles (weak) del núcleo agnóstico
namespace cbdos {

namespace mem {
    __attribute__((weak)) void* alloc_psram(size_t size) { return ::malloc(size); }
    __attribute__((weak)) void* realloc_psram(void* ptr, size_t size) { return ::realloc(ptr, size); }
    __attribute__((weak)) void* alloc_dma(size_t size) { return ::malloc(size); }
    __attribute__((weak)) void* alloc_internal(size_t size) { return ::malloc(size); }
    __attribute__((weak)) void* realloc_internal(void* ptr, size_t size) { return ::realloc(ptr, size); }
    __attribute__((weak)) void free_mem(void* ptr) { if (ptr) ::free(ptr); }
}

namespace log {
    __attribute__((weak)) void write(LogLevel level, const char* tag, const char* format, ...) {
        const char* lvlStr = "I";
        switch (level) {
            case LogLevel::Error:   lvlStr = "E"; break;
            case LogLevel::Warn:    lvlStr = "W"; break;
            case LogLevel::Info:    lvlStr = "I"; break;
            case LogLevel::Debug:   lvlStr = "D"; break;
            case LogLevel::Verbose: lvlStr = "V"; break;
        }
        printf("[%s][%s] ", lvlStr, tag ? tag : "CBDos");
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
        printf("\n");
    }
}

namespace rtos {
    __attribute__((weak)) TaskHandle createTask(TaskFunction fn, const char* name, uint32_t stackSize, void* param, uint32_t priority, int coreId) {
        (void)name; (void)stackSize; (void)priority; (void)coreId;
        if (fn) fn(param);
        return nullptr;
    }
    __attribute__((weak)) void deleteTask(TaskHandle handle) { (void)handle; }
    __attribute__((weak)) void sleepMs(uint32_t ms) { (void)ms; }

    __attribute__((weak)) MutexHandle createMutex() { return nullptr; }
    __attribute__((weak)) bool lockMutex(MutexHandle handle, uint32_t timeoutMs) { (void)handle; (void)timeoutMs; return true; }
    __attribute__((weak)) void unlockMutex(MutexHandle handle) { (void)handle; }
    __attribute__((weak)) void deleteMutex(MutexHandle handle) { (void)handle; }
}

namespace persistence {
    static IPersistenceBackend* s_backend = nullptr;

    void setBackend(IPersistenceBackend* backend) {
        s_backend = backend;
    }

    IPersistenceBackend* getBackend() {
        return s_backend;
    }
}


const char* getVersion() {
    return "0.2.0-CyBerDeck";
}

namespace system {
    __attribute__((weak)) float getCpuTemperature() { return 0.0f; }
}

namespace audio {
    __attribute__((weak)) bool init() { return true; }
    __attribute__((weak)) bool playStream(const char*) { return true; }
    __attribute__((weak)) bool playFile(const char*) { return true; }
    __attribute__((weak)) void stop() {}
    __attribute__((weak)) void pause() {}
    __attribute__((weak)) void resume() {}
    __attribute__((weak)) void setVolume(uint8_t) {}
    __attribute__((weak)) uint8_t getVolume() { return 75; }
    __attribute__((weak)) AudioStats getStats() { return AudioStats{}; }
    __attribute__((weak)) size_t readAudio(void*, size_t, uint32_t) { return 0; }
}

namespace storage {
    __attribute__((weak)) bool init() { return true; }
    __attribute__((weak)) bool mountSd() { return false; }
    __attribute__((weak)) bool unmountSd() { return false; }
    __attribute__((weak)) bool isSdMounted() { return false; }
    __attribute__((weak)) StorageStats getFlashStats() {
        return StorageStats{ true, 16 * 1024 * 1024, 0, 16 * 1024 * 1024, "Flash Interna" };
    }
    __attribute__((weak)) StorageStats getSdCardStats() {
        return StorageStats{ false, 0, 0, 0, "MicroSD" };
    }
    __attribute__((weak)) StorageStats getUsbStats() {
        return StorageStats{ false, 0, 0, 0, "USB Drive" };
    }
    __attribute__((weak)) std::vector<FileEntry> listDir(const char*) { return {}; }
    __attribute__((weak)) bool fileExists(const char*) { return false; }
    __attribute__((weak)) std::string readFile(const char*) { return ""; }
    __attribute__((weak)) bool writeFile(const char*, const std::string&) { return false; }
    __attribute__((weak)) bool deleteFile(const char*) { return false; }
    __attribute__((weak)) size_t getFreeBytes(StorageType type) {
        return (type == StorageType::InternalFlash) ? (16 * 1024 * 1024) : 0;
    }
    __attribute__((weak)) size_t getTotalBytes(StorageType type) {
        return (type == StorageType::InternalFlash) ? (16 * 1024 * 1024) : 0;
    }
}

} // namespace cbdos
