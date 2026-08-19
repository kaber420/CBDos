#include "cbdos/network.hpp"
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_netif.h>

namespace cbdos {
namespace network {

static NetStatus s_status = NetStatus::Disconnected;
static std::string s_ip = "0.0.0.0";
static int8_t s_rssi = 0;

bool init() {
    s_status = NetStatus::Disconnected;
    return true;
}

bool connectWifi(const char* ssid, const char* password) {
    if (!ssid) return false;
    s_status = NetStatus::Connecting;
    // La conexión real WiFi se gestionará mediante SDIO con el ESP32-C6
    return true;
}

bool connectWifiStatic(const char* ssid, const char* password, const char* ip, const char* gateway, const char* subnet, const char* dns) {
    if (!ssid) return false;
    s_status = NetStatus::Connecting;
    return true;
}

void disconnectWifi() {
    s_status = NetStatus::Disconnected;
    s_ip = "0.0.0.0";
}

NetStatus getStatus() {
    return s_status;
}

bool isConnected() {
    return (s_status == NetStatus::Connected);
}

std::string getIpAddress() {
    return s_ip;
}

int8_t getRssi() {
    return s_rssi;
}

} // namespace network
} // namespace cbdos
