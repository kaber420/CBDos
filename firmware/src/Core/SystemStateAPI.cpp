#include "SystemStateAPI.h"

// Packed state initialized to DISCONNECTED (0) in internal DRAM
std::atomic<uint32_t> SystemStateAPI::s_packedState(0);

void SystemStateAPI::updateWifi(bool connected, int rssi) {
    uint8_t levelVal = 0;
    if (connected) {
        if (rssi < -80) {
            levelVal = 1; // WEAK
        } else if (rssi < -70) {
            levelVal = 2; // MEDIUM
        } else {
            levelVal = 3; // EXCELLENT
        }
    }

    uint8_t rssiEncoded = (uint8_t)(rssi + 128); // Offset mapping (-128..127 -> 0..255)
    uint32_t packed = ((uint32_t)rssiEncoded & 0xFF) |
                     (((uint32_t)levelVal & 0x03) << 8) |
                     (connected ? (1U << 10) : 0U);

    s_packedState.store(packed, std::memory_order_relaxed);
}

SystemState SystemStateAPI::getSystemState() {
    uint32_t packed = s_packedState.load(std::memory_order_relaxed);

    SystemState state;
    state.wifiConnected = (packed & (1U << 10)) != 0;
    state.wifiLevel = (WifiSignalLevel)((packed >> 8) & 0x03);
    uint8_t rawRssi = (uint8_t)(packed & 0xFF);
    state.rssi = (int)rawRssi - 128;
    return state;
}

bool SystemStateAPI::isWifiConnected() {
    uint32_t packed = s_packedState.load(std::memory_order_relaxed);
    return (packed & (1U << 10)) != 0;
}

int SystemStateAPI::getWifiRSSI() {
    uint32_t packed = s_packedState.load(std::memory_order_relaxed);
    uint8_t rawRssi = (uint8_t)(packed & 0xFF);
    return (int)rawRssi - 128;
}

WifiSignalLevel SystemStateAPI::getWifiSignalLevel() {
    uint32_t packed = s_packedState.load(std::memory_order_relaxed);
    return (WifiSignalLevel)((packed >> 8) & 0x03);
}
