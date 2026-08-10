#include "ConfigManager.h"

#ifdef ARDUINO

#include <SD.h>
#include <mbedtls/aes.h>
#include <mbedtls/gcm.h>
#include <mbedtls/pkcs5.h>
#include <mbedtls/md.h>
#include <mbedtls/build_info.h>
#include <ArduinoJson.h>

bool ConfigManager::init() {
    bool res = preferences.begin("tablehub", true);
    if (res) {
        preferences.end();
    }
    return res;
}

bool ConfigManager::loadConfig(DeviceConfig& config) {
    if (_isLoaded) {
        config = _cachedConfig;
        return _cachedConfig.isConfigured;
    }

    if (!preferences.begin("tablehub", true)) {
        return false;
    }

    _cachedConfig.wifiSsid = preferences.getString("ssid", "");
    _cachedConfig.wifiPass = preferences.getString("pass", "");
    _cachedConfig.hubIp = preferences.getString("hub_ip", "");
    _cachedConfig.mqttPort = preferences.getInt("mqtt_port", 1883);
    _cachedConfig.bootstrapToken = preferences.getString("btoken", "");
    _cachedConfig.tableId = preferences.getString("table_id", "");
    _cachedConfig.isConfigured = preferences.getBool("configured", false);

    preferences.end();
    _isLoaded = true;

    config = _cachedConfig;
    return config.isConfigured;
}

bool ConfigManager::saveConfig(const DeviceConfig& config) {
    if (!preferences.begin("tablehub", false)) {
        return false;
    }

    preferences.putString("ssid", config.wifiSsid);
    preferences.putString("pass", config.wifiPass);
    preferences.putString("hub_ip", config.hubIp);
    preferences.putInt("mqtt_port", config.mqttPort);
    preferences.putString("btoken", config.bootstrapToken);
    preferences.putString("table_id", config.tableId);
    preferences.putBool("configured", true);

    preferences.end();

    _cachedConfig = config;
    _cachedConfig.isConfigured = true;
    _isLoaded = true;
    return true;
}

bool ConfigManager::clearConfig() {
    if (!preferences.begin("tablehub", false)) {
        return false;
    }
    preferences.clear();
    preferences.end();

    _cachedConfig = DeviceConfig();
    _isLoaded = true;
    return true;
}

static const size_t PBKDF2_SALT_LEN = 8;
static const size_t GCM_IV_LEN = 12;
static const size_t GCM_TAG_LEN = 16;
static const int PBKDF2_ITERATIONS = 10000;

bool ConfigManager::attemptProvisioning(const std::string& pin, String& errorMessage) {
    File file = SD.open("/tablehub.enc", FILE_READ);
    if (!file) {
        errorMessage = "Fallo SD.open: ¿tarjeta extraída o error SPI?";
        return false;
    }

    size_t fileSize = file.size();
    size_t headerSize = PBKDF2_SALT_LEN + GCM_IV_LEN;
    size_t minSize = headerSize + GCM_TAG_LEN;

    if (fileSize < minSize) {
        errorMessage = "El archivo está corrupto o es inválido.";
        file.close();
        return false;
    }

    uint8_t* buffer = (uint8_t*)ps_malloc(fileSize);
    if (!buffer) {
        buffer = (uint8_t*)malloc(fileSize);
        if (!buffer) {
            errorMessage = "Error de memoria (OOM).";
            file.close();
            return false;
        }
    }

    size_t bytesRead = file.read(buffer, fileSize);
    file.close();

    if (bytesRead != fileSize) {
        errorMessage = "Error al leer el archivo desde la SD.";
        free(buffer);
        return false;
    }

    uint8_t* salt = buffer;
    uint8_t* iv = buffer + PBKDF2_SALT_LEN;
    size_t ciphertextLen = fileSize - headerSize - GCM_TAG_LEN;
    uint8_t* ciphertext = buffer + headerSize;
    uint8_t* tag = buffer + headerSize + ciphertextLen;

    uint8_t key[32];
    int ret = mbedtls_pkcs5_pbkdf2_hmac_ext(MBEDTLS_MD_SHA256,
        (const unsigned char*)pin.c_str(), pin.length(),
        salt, PBKDF2_SALT_LEN, PBKDF2_ITERATIONS, 32, key);

    if (ret != 0) {
        errorMessage = "PIN incorrecto o archivo corrupto.";
        free(buffer);
        return false;
    }

    uint8_t* plaintext = (uint8_t*)malloc(ciphertextLen + 1);
    if (!plaintext) {
        errorMessage = "Error de memoria (OOM).";
        free(buffer);
        return false;
    }

    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256);

    ret = mbedtls_gcm_auth_decrypt(&gcm, ciphertextLen,
        iv, GCM_IV_LEN, NULL, 0, tag, GCM_TAG_LEN,
        ciphertext, plaintext);

    mbedtls_gcm_free(&gcm);

    if (ret != 0) {
        errorMessage = "PIN incorrecto o archivo corrupto.";
        free(plaintext);
        free(buffer);
        return false;
    }

    plaintext[ciphertextLen] = '\0';

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, plaintext, ciphertextLen);
    if (err) {
        errorMessage = "El archivo está corrupto (JSON inválido).";
        free(plaintext);
        free(buffer);
        return false;
    }

    DeviceConfig config;
    config.wifiSsid = doc["wifi_ssid"].as<const char*>();
    config.wifiPass = doc["wifi_pass"].as<const char*>();
    config.hubIp = doc["hub_ip"].as<const char*>();
    config.mqttPort = doc["mqtt_port"].as<int>();
    config.bootstrapToken = doc["bootstrap_token"].as<const char*>();
    config.tableId = doc["table_id"].as<const char*>();
    config.isConfigured = true;

    bool saved = saveConfig(config);

    free(plaintext);
    free(buffer);

    if (saved) {
        SD.remove("/tablehub.enc");
    }

    return saved;
}

#else

// Mock para entorno Emulator (Native)
static DeviceConfig mockConfig;

bool ConfigManager::init() {
    return true;
}

bool ConfigManager::loadConfig(DeviceConfig& config) {
    if (_isLoaded) {
        config = _cachedConfig;
        return _cachedConfig.isConfigured;
    }
    config = mockConfig;
    _cachedConfig = mockConfig;
    _isLoaded = true;
    return mockConfig.isConfigured;
}

bool ConfigManager::saveConfig(const DeviceConfig& config) {
    mockConfig = config;
    mockConfig.isConfigured = true;
    _cachedConfig = mockConfig;
    _isLoaded = true;
    return true;
}

bool ConfigManager::clearConfig() {
    mockConfig = DeviceConfig();
    _cachedConfig = mockConfig;
    _isLoaded = true;
    return true;
}

bool ConfigManager::attemptProvisioning(const std::string& pin, std::string& errorMessage) {
    if (pin == "1234") {
        DeviceConfig config;
        config.wifiSsid = "MockWiFi";
        config.wifiPass = "MockPass";
        config.hubIp = "192.168.1.100";
        config.mqttPort = 8883;
        config.bootstrapToken = "mock-token";
        config.tableId = "mesa-mock";
        config.isConfigured = true;
        return saveConfig(config);
    }
    errorMessage = "PIN incorrecto.";
    return false;
}

#endif
