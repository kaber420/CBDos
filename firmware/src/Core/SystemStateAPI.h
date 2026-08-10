#ifndef SYSTEM_STATE_API_H
#define SYSTEM_STATE_API_H

#include <stdint.h>
#include <atomic>

enum class WifiSignalLevel : uint8_t {
    DISCONNECTED = 0,
    WEAK = 1,      // RSSI < -80
    MEDIUM = 2,    // -80 <= RSSI < -70
    EXCELLENT = 3  // RSSI >= -70
};

struct SystemState {
    bool wifiConnected;
    int rssi;
    WifiSignalLevel wifiLevel;
};

class SystemStateAPI {
public:
    static void updateWifi(bool connected, int rssi);
    static SystemState getSystemState();
    static bool isWifiConnected();
    static int getWifiRSSI();
    static WifiSignalLevel getWifiSignalLevel();

private:
    // Atomic 32-bit packed word for 100% lock-free single-instruction cross-core synchronization on Xtensa LX7
    // Bits 0-7: RSSI offset (+128 so range is 0 to 255)
    // Bits 8-9: WifiSignalLevel (0 to 3)
    // Bit 10: wifiConnected (0 or 1)
    static std::atomic<uint32_t> s_packedState;
};

#endif // SYSTEM_STATE_API_H
