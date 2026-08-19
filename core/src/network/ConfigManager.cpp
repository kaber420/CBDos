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
    // Probar abrir namespaces
    bool res = preferences.begin("wifi", true);
    if (res) preferences.end();
    res = preferences.begin("lora", true);
    if (res) preferences.end();
    res = preferences.begin("flrc", true);
    if (res) preferences.end();
    res = preferences.begin("gateways", true);
    if (res) preferences.end();
    res = preferences.begin("time", true);
    if (res) preferences.end();
    return true;
}

bool ConfigManager::clearLegacyConfig() {
    if (preferences.begin("tablehub", false)) {
        preferences.clear();
        preferences.end();
        Serial.println("[NVS] Namespace legacy 'tablehub' borrado completamente");
        return true;
    }
    return false;
}

bool ConfigManager::clearAllNvs() {
    const char* namespaces[] = {"wifi", "lora", "flrc", "gateways", "time", "tablehub"};
    for (const char* ns : namespaces) {
        if (preferences.begin(ns, false)) {
            preferences.clear();
            preferences.end();
        }
    }
    Serial.println("[NVS] Todos los namespaces NVS borrados correctamente");
    return true;
}

// ─── WiFi Config ───
bool ConfigManager::loadWiFi(WiFiConfig& cfg) {
    if (preferences.begin("wifi", true)) {
        cfg.ssid = preferences.getString("ssid", "");
        cfg.password = preferences.getString("pass", "");
        cfg.useStaticIp = preferences.getBool("static", false);
        cfg.staticIp = preferences.getString("ip", "");
        cfg.gateway = preferences.getString("gw", "");
        cfg.subnet = preferences.getString("sub", "");
        cfg.dns1 = preferences.getString("dns1", "");
        cfg.dns2 = preferences.getString("dns2", "");
        preferences.end();
    }

    // Fallback de migración: Si en "wifi" no hay SSID, buscar en namespace legacy "tablehub"
    if (cfg.ssid.length() == 0) {
        if (preferences.begin("tablehub", true)) {
            cfg.ssid = preferences.getString("ssid", "");
            cfg.password = preferences.getString("pass", "");
            preferences.end();
            if (cfg.ssid.length() > 0) {
                saveWiFi(cfg);
            }
        }
    }
    return cfg.ssid.length() > 0;
}

bool ConfigManager::saveWiFi(const WiFiConfig& cfg) {
    if (!preferences.begin("wifi", false)) {
        return false;
    }
    preferences.putString("ssid", cfg.ssid);
    preferences.putString("pass", cfg.password);
    preferences.putBool("static", cfg.useStaticIp);
    preferences.putString("ip", cfg.staticIp);
    preferences.putString("gw", cfg.gateway);
    preferences.putString("sub", cfg.subnet);
    preferences.putString("dns1", cfg.dns1);
    preferences.putString("dns2", cfg.dns2);
    preferences.end();
    return true;
}

// ─── LoRa Config ───
bool ConfigManager::loadLoRa(LoRaConfig& cfg) {
    if (!preferences.begin("lora", true)) {
        return false;
    }
    cfg.frequency = preferences.getFloat("freq", 915.0f);
    cfg.txPower = (int8_t)preferences.getChar("txpwr", 14);
    cfg.bandwidth = preferences.getFloat("bw", 250.0f);
    cfg.spreadingFactor = preferences.getUChar("sf", 7);
    cfg.codingRate = preferences.getUChar("cr", 5);
    cfg.syncWord = preferences.getUShort("sync", 0x32);
    cfg.enableCRC = preferences.getBool("crc", true);
    cfg.preambleLength = preferences.getUShort("preamb", 8);
    preferences.end();
    return true;
}

bool ConfigManager::saveLoRa(const LoRaConfig& cfg) {
    if (!preferences.begin("lora", false)) {
        return false;
    }
    preferences.putFloat("freq", cfg.frequency);
    preferences.putChar("txpwr", cfg.txPower);
    preferences.putFloat("bw", cfg.bandwidth);
    preferences.putUChar("sf", cfg.spreadingFactor);
    preferences.putUChar("cr", cfg.codingRate);
    preferences.putUShort("sync", cfg.syncWord);
    preferences.putBool("crc", cfg.enableCRC);
    preferences.putUShort("preamb", cfg.preambleLength);
    preferences.end();
    return true;
}

// ─── FLRC Config ───
bool ConfigManager::loadFLRC(FLRCConfig& cfg) {
    if (!preferences.begin("flrc", true)) {
        return false;
    }
    cfg.frequency = preferences.getFloat("freq", 2.400f);
    cfg.txPower = (int8_t)preferences.getChar("txpwr", 10);
    cfg.bandwidth = preferences.getFloat("bw", 1.2f);
    cfg.dataRate = preferences.getUChar("rate", 1);
    cfg.codingRate = preferences.getUChar("cr", 2);
    cfg.syncWord = preferences.getUShort("sync", 0x7B5A);
    cfg.enableCRC = preferences.getBool("crc", true);
    cfg.preambleLength = preferences.getUShort("preamb", 8);
    preferences.end();
    return true;
}

bool ConfigManager::saveFLRC(const FLRCConfig& cfg) {
    if (!preferences.begin("flrc", false)) {
        return false;
    }
    preferences.putFloat("freq", cfg.frequency);
    preferences.putChar("txpwr", cfg.txPower);
    preferences.putFloat("bw", cfg.bandwidth);
    preferences.putUChar("rate", cfg.dataRate);
    preferences.putUChar("cr", cfg.codingRate);
    preferences.putUShort("sync", cfg.syncWord);
    preferences.putBool("crc", cfg.enableCRC);
    preferences.putUShort("preamb", cfg.preambleLength);
    preferences.end();
    return true;
}

// ─── Time / NTP Config ───
bool ConfigManager::loadTime(TimeConfig& cfg) {
    if (!preferences.begin("time", true)) {
        return false;
    }
    cfg.ntpServer = preferences.getString("server", "pool.ntp.org");
    cfg.gmtOffsetSeconds = preferences.getInt("offset", -21600);
    cfg.daylightOffsetSeconds = preferences.getInt("dst", 0);
    cfg.enabled = preferences.getBool("enabled", true);
    preferences.end();
    return true;
}

bool ConfigManager::saveTime(const TimeConfig& cfg) {
    if (!preferences.begin("time", false)) {
        return false;
    }
    preferences.putString("server", cfg.ntpServer);
    preferences.putInt("offset", cfg.gmtOffsetSeconds);
    preferences.putInt("dst", cfg.daylightOffsetSeconds);
    preferences.putBool("enabled", cfg.enabled);
    preferences.end();
    return true;
}

// ─── Gateways Config (MsgPack en SD) ───
static const size_t PBKDF2_SALT_LEN = 8;
static const size_t GCM_IV_LEN = 12;
static const size_t GCM_TAG_LEN = 16;
static const int PBKDF2_ITERATIONS = 10000;

std::vector<GatewayConfig> ConfigManager::listGateways() {
    std::vector<GatewayConfig> list;
    if (!SD.exists("/config")) {
        SD.mkdir("/config");
    }
    if (!SD.exists("/config/gateways.bin")) {
        return list;
    }

    File file = SD.open("/config/gateways.bin", FILE_READ);
    if (!file) {
        return list;
    }

    JsonDocument doc;
    DeserializationError err = deserializeMsgPack(doc, file);
    file.close();

    if (err) {
        Serial.println("[ConfigManager] Error deserializando gateways.bin");
        return list;
    }

    JsonArray arr = doc.as<JsonArray>();
    for (JsonVariant val : arr) {
        GatewayConfig gw;
        gw.id = val["id"].as<String>();
        gw.name = val["name"].as<String>();
        gw.address = val["address"].as<String>();
        gw.domain = val["domain"].as<String>();
        gw.mqttPort = val["mqtt_port"].as<int>();
        gw.mqttUseTls = val["mqtt_tls"].as<bool>();
        gw.authToken = val["auth_token"].as<String>();
        gw.authType = val["auth_type"].as<String>();
        gw.discoveryMethod = val["discovery"].as<String>();
        gw.notes = val["notes"].as<String>();
        list.push_back(gw);
    }
    return list;
}

static bool saveGatewayList(const std::vector<GatewayConfig>& list) {
    if (!SD.exists("/config")) {
        SD.mkdir("/config");
    }
    File file = SD.open("/config/gateways.bin", FILE_WRITE);
    if (!file) {
        return false;
    }

    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (const auto& gw : list) {
        JsonObject obj = arr.add<JsonObject>();
        obj["id"] = gw.id;
        obj["name"] = gw.name;
        obj["address"] = gw.address;
        obj["domain"] = gw.domain;
        obj["mqtt_port"] = gw.mqttPort;
        obj["mqtt_tls"] = gw.mqttUseTls;
        obj["auth_token"] = gw.authToken;
        obj["auth_type"] = gw.authType;
        obj["discovery"] = gw.discoveryMethod;
        obj["notes"] = gw.notes;
    }

    size_t written = serializeMsgPack(doc, file);
    file.close();
    return written > 0;
}

bool ConfigManager::importGateway(const String& encPath, const String& pin, String& errorOut) {
    File file = SD.open(encPath, FILE_READ);
    if (!file) {
        errorOut = "Fallo SD.open: " + encPath;
        return false;
    }

    size_t fileSize = file.size();
    size_t headerSize = PBKDF2_SALT_LEN + GCM_IV_LEN;
    size_t minSize = headerSize + GCM_TAG_LEN;

    if (fileSize < minSize) {
        errorOut = "Archivo corrupto.";
        file.close();
        return false;
    }

    uint8_t* buffer = (uint8_t*)ps_malloc(fileSize);
    if (!buffer) {
        buffer = (uint8_t*)malloc(fileSize);
        if (!buffer) {
            errorOut = "Error de memoria (OOM).";
            file.close();
            return false;
        }
    }

    size_t bytesRead = file.read(buffer, fileSize);
    file.close();

    if (bytesRead != fileSize) {
        errorOut = "Error al leer SD.";
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
        errorOut = "PIN incorrecto o archivo corrupto.";
        free(buffer);
        return false;
    }

    uint8_t* plaintext = (uint8_t*)malloc(ciphertextLen + 1);
    if (!plaintext) {
        errorOut = "Error de memoria (OOM).";
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
        errorOut = "PIN incorrecto o archivo corrupto.";
        free(plaintext);
        free(buffer);
        return false;
    }

    plaintext[ciphertextLen] = '\0';

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, plaintext, ciphertextLen);
    free(plaintext);
    free(buffer);

    if (err) {
        errorOut = "JSON corrupto.";
        return false;
    }

    GatewayConfig gw;
    gw.id = "gw_" + String(millis()); // Generar ID basado en milisegundos
    gw.name = doc["name"].as<const char*>() ? doc["name"].as<const char*>() : "Gateway";
    gw.address = doc["address"].as<const char*>() ? doc["address"].as<const char*>() : "";
    gw.domain = doc["domain"].as<const char*>() ? doc["domain"].as<const char*>() : "";
    gw.mqttPort = doc["mqtt_port"].as<int>() ? doc["mqtt_port"].as<int>() : 1883;
    gw.mqttUseTls = doc["mqtt_use_tls"].as<bool>();
    gw.authToken = doc["auth_token"].as<const char*>() ? doc["auth_token"].as<const char*>() : "";
    gw.authType = doc["auth_type"].as<const char*>() ? doc["auth_type"].as<const char*>() : "token";
    gw.discoveryMethod = doc["discovery_method"].as<const char*>() ? doc["discovery_method"].as<const char*>() : "static";
    gw.notes = doc["notes"].as<const char*>() ? doc["notes"].as<const char*>() : "";

    auto list = listGateways();
    list.push_back(gw);
    
    if (saveGatewayList(list)) {
        SD.remove(encPath);
        return true;
    } else {
        errorOut = "Error guardando lista MsgPack.";
        return false;
    }
}

bool ConfigManager::removeGateway(const String& gwId) {
    auto list = listGateways();
    bool found = false;
    for (auto it = list.begin(); it != list.end(); ++it) {
        if (it->id == gwId) {
            list.erase(it);
            found = true;
            break;
        }
    }
    if (found) {
        saveGatewayList(list);
        // Si el activo era este, limpiarlo
        if (preferences.begin("gateways", false)) {
            if (preferences.getString("active_id", "") == gwId) {
                preferences.remove("active_id");
            }
            preferences.end();
        }
    }
    return found;
}

bool ConfigManager::setActiveGateway(const String& gwId) {
    if (!preferences.begin("gateways", false)) {
        return false;
    }
    preferences.putString("active_id", gwId);
    preferences.end();
    return true;
}

bool ConfigManager::loadActiveGateway(GatewayConfig& gw) {
    if (!preferences.begin("gateways", true)) {
        return false;
    }
    String activeId = preferences.getString("active_id", "");
    preferences.end();

    if (activeId.length() == 0) {
        return false;
    }

    auto list = listGateways();
    for (const auto& item : list) {
        if (item.id == activeId) {
            gw = item;
            return true;
        }
    }
    return false;
}

#else

// ─── Mock para entorno Emulator (Native) ───
static WiFiConfig mockWiFi;
static LoRaConfig mockLoRa;
static FLRCConfig mockFLRC;
static std::vector<GatewayConfig> mockGateways;
static std::string mockActiveGwId = "";

bool ConfigManager::init() {
    return true;
}

bool ConfigManager::loadWiFi(WiFiConfig& cfg) {
    cfg = mockWiFi;
    return true;
}

bool ConfigManager::saveWiFi(const WiFiConfig& cfg) {
    mockWiFi = cfg;
    return true;
}

bool ConfigManager::loadLoRa(LoRaConfig& cfg) {
    cfg = mockLoRa;
    return true;
}

bool ConfigManager::saveLoRa(const LoRaConfig& cfg) {
    mockLoRa = cfg;
    return true;
}

bool ConfigManager::loadFLRC(FLRCConfig& cfg) {
    cfg = mockFLRC;
    return true;
}

bool ConfigManager::saveFLRC(const FLRCConfig& cfg) {
    mockFLRC = cfg;
    return true;
}

static TimeConfig mockTime;

bool ConfigManager::loadTime(TimeConfig& cfg) {
    cfg = mockTime;
    return true;
}

bool ConfigManager::saveTime(const TimeConfig& cfg) {
    mockTime = cfg;
    return true;
}

bool ConfigManager::importGateway(const std::string& encPath, const std::string& pin, std::string& errorOut) {
    if (pin == "1234") {
        GatewayConfig gw;
        gw.id = "gw_mock_" + std::to_string(mockGateways.size() + 1);
        gw.name = "Mock Gateway";
        gw.address = "127.0.0.1";
        gw.mqttPort = 1883;
        gw.authToken = "auth-mock-token-abc";
        mockGateways.push_back(gw);
        return true;
    }
    errorOut = "PIN incorrecto (usa 1234).";
    return false;
}

bool ConfigManager::removeGateway(const std::string& gwId) {
    for (auto it = mockGateways.begin(); it != mockGateways.end(); ++it) {
        if (it->id == gwId) {
            mockGateways.erase(it);
            if (mockActiveGwId == gwId) mockActiveGwId = "";
            return true;
        }
    }
    return false;
}

std::vector<GatewayConfig> ConfigManager::listGateways() {
    return mockGateways;
}

bool ConfigManager::setActiveGateway(const std::string& gwId) {
    mockActiveGwId = gwId;
    return true;
}

bool ConfigManager::loadActiveGateway(GatewayConfig& gw) {
    for (const auto& item : mockGateways) {
        if (item.id == mockActiveGwId) {
            gw = item;
            return true;
        }
    }
    return false;
}

bool ConfigManager::clearLegacyConfig() {
    return true;
}

bool ConfigManager::clearAllNvs() {
    mockGateways.clear();
    mockActiveGwId = "";
    return true;
}

#endif

