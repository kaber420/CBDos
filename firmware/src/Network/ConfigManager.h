#ifndef CONFIG_MANAGER_H
#ifndef ARDUINO
#define CONFIG_MANAGER_H

#include <string>

struct DeviceConfig {
    std::string wifiSsid;
    std::string wifiPass;
    std::string hubIp;
    int mqttPort = 1883;
    std::string bootstrapToken;
    std::string tableId;
    bool isConfigured = false;
};

class ConfigManager {
public:
    static ConfigManager& getInstance() {
        static ConfigManager instance;
        return instance;
    }

    bool init();
    bool loadConfig(DeviceConfig& config);
    bool saveConfig(const DeviceConfig& config);
    bool clearConfig();
    bool attemptProvisioning(const std::string& pin, std::string& errorMessage);

private:
    ConfigManager() = default;
    DeviceConfig _cachedConfig;
    bool _isLoaded = false;
};

#else
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include <string>

struct DeviceConfig {
    String wifiSsid;
    String wifiPass;
    String hubIp;
    int mqttPort = 1883;
    String bootstrapToken;
    String tableId;
    bool isConfigured = false;
};

class ConfigManager {
public:
    static ConfigManager& getInstance() {
        static ConfigManager instance;
        return instance;
    }

    bool init();
    bool loadConfig(DeviceConfig& config);
    bool saveConfig(const DeviceConfig& config);
    bool clearConfig();
    bool attemptProvisioning(const std::string& pin, String& errorMessage);

private:
    ConfigManager() = default;
    Preferences preferences;
    DeviceConfig _cachedConfig;
    bool _isLoaded = false;
};

#endif
#endif
