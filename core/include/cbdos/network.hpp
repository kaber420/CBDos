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

class INetworkAdapter {
public:
    virtual ~INetworkAdapter() = default;

    virtual bool init() = 0;
    virtual bool connectWifi(const char* ssid, const char* password) = 0;
    virtual bool connectWifiStatic(const char* ssid, const char* password, const char* ip, const char* gateway, const char* subnet = "255.255.255.0", const char* dns = nullptr) = 0;
    virtual void disconnectWifi() = 0;
    virtual NetStatus getStatus() const = 0;
    virtual bool isConnected() const = 0;
    virtual std::string getIpAddress() const = 0;
    virtual int8_t getRssi() const = 0;
};

void setNetworkAdapter(INetworkAdapter* adapter);
INetworkAdapter* getNetworkAdapter();

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
