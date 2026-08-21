#include "cbdos/system.hpp"
#include "cbdos/display.hpp"
#include "cbdos/input.hpp"
#include "cbdos/audio.hpp"
#include "cbdos/network.hpp"
#include "cbdos/storage.hpp"

// Punto de entrada y fallbacks débiles (weak) del núcleo agnóstico
namespace cbdos {

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
}

namespace network {
    __attribute__((weak)) bool init() { return true; }
    __attribute__((weak)) bool connectWifi(const char*, const char*) { return true; }
    __attribute__((weak)) bool connectWifiStatic(const char*, const char*, const char*, const char*, const char*, const char*) { return true; }
    __attribute__((weak)) void disconnectWifi() {}
    __attribute__((weak)) NetStatus getStatus() { return NetStatus::Disconnected; }
    __attribute__((weak)) bool isConnected() { return false; }
    __attribute__((weak)) std::string getIpAddress() { return "0.0.0.0"; }
    __attribute__((weak)) int8_t getRssi() { return 0; }
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
