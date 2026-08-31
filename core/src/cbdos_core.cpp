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

} // namespace cbdos



