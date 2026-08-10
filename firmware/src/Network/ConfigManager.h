#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <string>
#include <vector>

#ifndef ARDUINO
// ─── Emulator Definitions ───
struct WiFiConfig {
    std::string ssid;
    std::string password;
    bool useStaticIp = false;
    std::string staticIp;
    std::string gateway;
    std::string subnet;
    std::string dns1;
    std::string dns2;
};

struct LoRaConfig {
    float frequency = 915.0f;
    int8_t txPower = 14;
    float bandwidth = 250.0f;
    uint8_t spreadingFactor = 7;
    uint8_t codingRate = 5;
    uint16_t syncWord = 0x32;
    bool enableCRC = true;
    uint16_t preambleLength = 8;
};

struct FLRCConfig {
    float frequency = 2.400f;
    int8_t txPower = 10;
    float bandwidth = 1.2f;
    uint8_t dataRate = 1;
    uint8_t codingRate = 2;
    uint16_t syncWord = 0x7B5A;
    bool enableCRC = true;
    uint16_t preambleLength = 8;
};

struct GatewayConfig {
    std::string id;
    std::string name;
    std::string address;
    std::string domain;
    int mqttPort = 1883;
    bool mqttUseTls = false;
    std::string authToken;
    std::string authType = "token";
    std::string discoveryMethod = "static";
    std::string notes;
};

class ConfigManager {
public:
    static ConfigManager& getInstance() {
        static ConfigManager instance;
        return instance;
    }

    bool init();

    // WiFi
    bool loadWiFi(WiFiConfig& cfg);
    bool saveWiFi(const WiFiConfig& cfg);

    // LoRa
    bool loadLoRa(LoRaConfig& cfg);
    bool saveLoRa(const LoRaConfig& cfg);

    // FLRC
    bool loadFLRC(FLRCConfig& cfg);
    bool saveFLRC(const FLRCConfig& cfg);

    // Gateways
    bool importGateway(const std::string& encPath, const std::string& pin, std::string& errorOut);
    bool removeGateway(const std::string& gwId);
    std::vector<GatewayConfig> listGateways();
    bool setActiveGateway(const std::string& gwId);
    bool loadActiveGateway(GatewayConfig& gw);

    // Limpieza NVS
    bool clearLegacyConfig();
    bool clearAllNvs();

private:
    ConfigManager() = default;
};

#else
// ─── Arduino / ESP32 Definitions ───
#include <Arduino.h>
#include <Preferences.h>

struct WiFiConfig {
    String ssid;
    String password;
    bool useStaticIp = false;
    String staticIp;
    String gateway;
    String subnet;
    String dns1;
    String dns2;
};

struct LoRaConfig {
    float frequency = 915.0f;
    int8_t txPower = 14;
    float bandwidth = 250.0f;
    uint8_t spreadingFactor = 7;
    uint8_t codingRate = 5;
    uint16_t syncWord = 0x32;
    bool enableCRC = true;
    uint16_t preambleLength = 8;
};

struct FLRCConfig {
    float frequency = 2.400f;
    int8_t txPower = 10;
    float bandwidth = 1.2f;
    uint8_t dataRate = 1;
    uint8_t codingRate = 2;
    uint16_t syncWord = 0x7B5A;
    bool enableCRC = true;
    uint16_t preambleLength = 8;
};

struct GatewayConfig {
    String id;
    String name;
    String address;
    String domain;
    int mqttPort = 1883;
    bool mqttUseTls = false;
    String authToken;
    String authType = "token";
    String discoveryMethod = "static";
    String notes;
};

class ConfigManager {
public:
    static ConfigManager& getInstance() {
        static ConfigManager instance;
        return instance;
    }

    bool init();

    // WiFi
    bool loadWiFi(WiFiConfig& cfg);
    bool saveWiFi(const WiFiConfig& cfg);

    // LoRa
    bool loadLoRa(LoRaConfig& cfg);
    bool saveLoRa(const LoRaConfig& cfg);

    // FLRC
    bool loadFLRC(FLRCConfig& cfg);
    bool saveFLRC(const FLRCConfig& cfg);

    // Gateways
    bool importGateway(const String& encPath, const String& pin, String& errorOut);
    bool removeGateway(const String& gwId);
    std::vector<GatewayConfig> listGateways();
    bool setActiveGateway(const String& gwId);
    bool loadActiveGateway(GatewayConfig& gw);

    // Limpieza NVS
    bool clearLegacyConfig();
    bool clearAllNvs();

private:
    ConfigManager() = default;
    Preferences preferences;
};

#endif
#endif

