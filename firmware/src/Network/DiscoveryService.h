#ifndef DISCOVERY_SERVICE_H
#define DISCOVERY_SERVICE_H

#ifdef ARDUINO
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ESPmDNS.h>
#else
#include <string>
#include <cstdint>
typedef std::string String;
#endif

#include "ConfigManager.h"

enum class DiscoveryState {
    IDLE,
    TRY_STATIC_IP,
    TRY_MDNS,
    TRY_UDP_BROADCAST,
    SUCCESS,
    FAILED
};

class DiscoveryService {
public:
    static DiscoveryService& getInstance() {
        static DiscoveryService instance;
        return instance;
    }

    void startDiscovery(const GatewayConfig& gw);
    DiscoveryState update();
    
    String getDiscoveredHubIp() const { return hubIp; }
    int getDiscoveredMqttPort() const { return mqttPort; }
    DiscoveryState getState() const { return currentState; }

private:
    DiscoveryService() = default;

    DiscoveryState currentState = DiscoveryState::IDLE;
    String targetStaticIp;
    String hubIp;
    int mqttPort = 1883;

    uint32_t stateStartTime = 0;
    int retryCount = 0;

#ifdef ARDUINO
    WiFiUDP udp;
    bool sendUdpBroadcast();
    bool checkUdpResponse();
#endif
};

#endif
