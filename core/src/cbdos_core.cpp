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
    __attribute__((weak)) bool mountSdCard() { return true; }
    __attribute__((weak)) void unmountSdCard() {}
    __attribute__((weak)) bool isSdCardMounted() { return false; }
    __attribute__((weak)) size_t getSdTotalBytes() { return 0; }
    __attribute__((weak)) size_t getSdUsedBytes() { return 0; }
}

} // namespace cbdos
