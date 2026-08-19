#pragma once
#include <cstdint>
#include <string>

namespace cbdos {
namespace network {

enum class NetStatus {
    Disconnected,
    Connecting,
    Connected,
    Error
};

bool init();
bool connectWifi(const char* ssid, const char* password);
bool connectWifiStatic(const char* ssid, const char* password, const char* ip, const char* gateway, const char* subnet = "255.255.255.0", const char* dns = nullptr);
void disconnectWifi();
NetStatus getStatus();
bool isConnected();
std::string getIpAddress();
int8_t getRssi();

} // namespace network
} // namespace cbdos
