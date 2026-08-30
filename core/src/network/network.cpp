#include "cbdos/network.hpp"

namespace cbdos {
namespace network {

static INetworkAdapter* s_networkAdapter = nullptr;

void setNetworkAdapter(INetworkAdapter* adapter) {
    s_networkAdapter = adapter;
}

INetworkAdapter* getNetworkAdapter() {
    return s_networkAdapter;
}

bool init() {
    if (s_networkAdapter) {
        return s_networkAdapter->init();
    }
    return false;
}

bool connectWifi(const char* ssid, const char* password) {
    if (s_networkAdapter) {
        return s_networkAdapter->connectWifi(ssid, password);
    }
    return false;
}

bool connectWifiStatic(const char* ssid, const char* password, const char* ip, const char* gateway, const char* subnet, const char* dns) {
    if (s_networkAdapter) {
        return s_networkAdapter->connectWifiStatic(ssid, password, ip, gateway, subnet, dns);
    }
    return false;
}

void disconnectWifi() {
    if (s_networkAdapter) {
        s_networkAdapter->disconnectWifi();
    }
}

NetStatus getStatus() {
    if (s_networkAdapter) {
        return s_networkAdapter->getStatus();
    }
    return NetStatus::Disconnected;
}

bool isConnected() {
    if (s_networkAdapter) {
        return s_networkAdapter->isConnected();
    }
    return false;
}

std::string getIpAddress() {
    if (s_networkAdapter) {
        return s_networkAdapter->getIpAddress();
    }
    return "0.0.0.0";
}

int8_t getRssi() {
    if (s_networkAdapter) {
        return s_networkAdapter->getRssi();
    }
    return -127;
}

} // namespace network
} // namespace cbdos
