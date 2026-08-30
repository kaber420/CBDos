#include "cbdos/network.hpp"
#include <Arduino.h>
#include <WiFi.h>

namespace cbdos {
namespace bsp {

class S3NetworkAdapter : public network::INetworkAdapter {
public:
    ~S3NetworkAdapter() override = default;

    bool init() override {
        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(true);
        return true;
    }

    bool connectWifi(const char* ssid, const char* password) override {
        if (!ssid || strlen(ssid) == 0) return false;
        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(true);
        WiFi.disconnect();
        WiFi.config(IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0));
        WiFi.begin(ssid, password ? password : "");
        return true;
    }

    bool connectWifiStatic(const char* ssid, const char* password, const char* ipStr, const char* gwStr, const char* subStr, const char* dnsStr) override {
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

    void disconnectWifi() override {
        WiFi.disconnect();
    }

    network::NetStatus getStatus() const override {
        wl_status_t st = WiFi.status();
        switch (st) {
            case WL_CONNECTED:
                return network::NetStatus::Connected;
            case WL_DISCONNECTED:
            case WL_IDLE_STATUS:
                return network::NetStatus::Disconnected;
            case WL_CONNECT_FAILED:
            case WL_CONNECTION_LOST:
                return network::NetStatus::Error;
            default:
                return network::NetStatus::Connecting;
        }
    }

    bool isConnected() const override {
        return (WiFi.status() == WL_CONNECTED);
    }

    std::string getIpAddress() const override {
        if (WiFi.status() == WL_CONNECTED) {
            return WiFi.localIP().toString().c_str();
        }
        return "0.0.0.0";
    }

    int8_t getRssi() const override {
        if (WiFi.status() == WL_CONNECTED) {
            return (int8_t)WiFi.RSSI();
        }
        return -127;
    }
};

static S3NetworkAdapter s_s3NetworkAdapter;

void initNetworkAdapterS3() {
    network::setNetworkAdapter(&s_s3NetworkAdapter);
    Serial.println("[NET_S3] S3 Network Adapter inicializado e inyectado.");
}

} // namespace bsp
} // namespace cbdos
