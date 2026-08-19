#include "cbdos/network.hpp"
#include <Arduino.h>
#include <WiFi.h>

namespace cbdos {
namespace network {

bool init() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    return true;
}

bool connectWifi(const char* ssid, const char* password) {
    if (!ssid || strlen(ssid) == 0) return false;
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.disconnect();
    WiFi.config(IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0));
    WiFi.begin(ssid, password ? password : "");
    return true;
}

bool connectWifiStatic(const char* ssid, const char* password, const char* ipStr, const char* gwStr, const char* subStr, const char* dnsStr) {
    if (!ssid || strlen(ssid) == 0) return false;
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.disconnect();

    IPAddress ip, gw, sub(255, 255, 255, 0), dns1;
    if (ipStr && ip.fromString(ipStr) && gwStr && gw.fromString(gwStr)) {
        if (subStr && strlen(subStr) > 0) sub.fromString(subStr);
        if (dnsStr && strlen(dnsStr) > 0 && dns1.fromString(dnsStr)) {
            WiFi.config(ip, gw, sub, dns1);
        } else {
            WiFi.config(ip, gw, sub);
        }
    }
    WiFi.begin(ssid, password ? password : "");
    return true;
}

void disconnectWifi() {
    WiFi.disconnect();
}

NetStatus getStatus() {
    wl_status_t st = WiFi.status();
    switch (st) {
        case WL_CONNECTED:
            return NetStatus::Connected;
        case WL_DISCONNECTED:
        case WL_IDLE_STATUS:
            return NetStatus::Disconnected;
        case WL_CONNECT_FAILED:
        case WL_CONNECTION_LOST:
            return NetStatus::Error;
        default:
            return NetStatus::Connecting;
    }
}

bool isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

std::string getIpAddress() {
    if (WiFi.status() == WL_CONNECTED) {
        return WiFi.localIP().toString().c_str();
    }
    return "0.0.0.0";
}

int8_t getRssi() {
    if (WiFi.status() == WL_CONNECTED) {
        return (int8_t)WiFi.RSSI();
    }
    return -127;
}

} // namespace network
} // namespace cbdos
