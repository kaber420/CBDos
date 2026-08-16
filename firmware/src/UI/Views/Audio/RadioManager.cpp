#include "RadioManager.h"
#include "../../../Core/LVFS_Driver.h"
#include <cstdio>
#include <vector>
#include <string>

#ifdef ARDUINO
#include <Arduino.h>
#include <SD.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#endif

// Funciones estáticas de trios removidas

void RadioManager::init() {
    loadFavorites();
}

const std::vector<RadioStation>& RadioManager::getFavorites() {
    if (favorites.empty()) {
        loadFavorites();
    }
    return favorites;
}

void RadioManager::addFavorite(const RadioStation& station) {
    RadioStation st = station;
    st.isFavorite = true;
    favorites.push_back(st);
    saveFavorites();
}

void RadioManager::updateFavorite(size_t index, const RadioStation& station) {
    if (index < favorites.size()) {
        favorites[index] = station;
        favorites[index].isFavorite = true;
        saveFavorites();
    }
}

void RadioManager::removeFavorite(size_t index) {
    if (index < favorites.size()) {
        favorites.erase(favorites.begin() + index);
        saveFavorites();
    }
}

void RadioManager::loadFavorites() {
    favorites.clear();

#ifdef ARDUINO
    if (SD.cardType() != CARD_NONE) {
        lv_fs_spi_lock();
        if (SD.exists("/audio/radios.json")) {
            File file = SD.open("/audio/radios.json", FILE_READ);
            if (file) {
                JsonDocument doc;
                DeserializationError err = deserializeJson(doc, file);
                file.close();
                lv_fs_spi_unlock();

                if (!err && doc.is<JsonObject>()) {
                    JsonArray stationsArr = doc["stations"].as<JsonArray>();
                    for (JsonObject sObj : stationsArr) {
                        RadioStation st;
                        const char* n = sObj["name"] | "Radio";
                        const char* u = sObj["url"] | "";
                        const char* c = sObj["country"] | "Global";
                        const char* g = sObj["genre"] | "Varios";
                        strncpy(st.name, n, sizeof(st.name) - 1);
                        strncpy(st.url, u, sizeof(st.url) - 1);
                        strncpy(st.country, c, sizeof(st.country) - 1);
                        strncpy(st.genre, g, sizeof(st.genre) - 1);
                        st.bitrate = sObj["bitrate"] | 128;
                        st.isFavorite = true;
                        if (st.url[0] != '\0') {
                            favorites.push_back(st);
                        }
                    }
                }
            } else {
                lv_fs_spi_unlock();
            }
        } else {
            lv_fs_spi_unlock();
        }
    }
#endif

    if (favorites.empty()) {
        auto addDefault = [&](const char* name, const char* url, const char* country, const char* genre, int bitrate) {
            RadioStation st;
            strncpy(st.name, name, sizeof(st.name) - 1);
            strncpy(st.url, url, sizeof(st.url) - 1);
            strncpy(st.country, country, sizeof(st.country) - 1);
            strncpy(st.genre, genre, sizeof(st.genre) - 1);
            st.bitrate = bitrate;
            st.isFavorite = true;
            favorites.push_back(st);
        };

        addDefault("SomaFM Groove Salad", "http://ice1.somafm.com/groovesalad-128-mp3", "USA", "Ambient / Chill", 128);
        addDefault("Ibiza Global Radio", "http://listento.ibizaglobalradio.com:8024/stream", "Espana", "Electronic", 128);
        addDefault("Radio Paradise", "http://stream.radioparadise.com/mp3-128", "USA", "Rock / Eclectic", 128);
        addDefault("SomaFM Secret Agent", "http://ice1.somafm.com/secretagent-128-mp3", "USA", "Spy / Lounge", 128);
    }
}

void RadioManager::saveFavorites() {
#ifdef ARDUINO
    if (SD.cardType() != CARD_NONE) {
        lv_fs_spi_lock();
        if (!SD.exists("/audio")) {
            SD.mkdir("/audio");
        }
        File file = SD.open("/audio/radios.json", FILE_WRITE);
        if (file) {
            JsonDocument doc;
            JsonArray stationsArr = doc["stations"].to<JsonArray>();
            for (const auto& st : favorites) {
                JsonObject sObj = stationsArr.add<JsonObject>();
                sObj["name"] = st.name;
                sObj["url"] = st.url;
                sObj["country"] = st.country;
                sObj["genre"] = st.genre;
                sObj["bitrate"] = st.bitrate;
            }
            serializeJson(doc, file);
            file.close();
        }
        lv_fs_spi_unlock();
    }
#endif
}

#ifdef ARDUINO
struct SpiRamJsonAllocator : ArduinoJson::Allocator {
    void* allocate(size_t size) override {
        return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    void deallocate(void* pointer) override {
        heap_caps_free(pointer);
    }
    void* reallocate(void* ptr, size_t new_size) override {
        return heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
};

static SpiRamJsonAllocator s_spiRamJsonAllocator;

static std::string urlEncodeQuery(const std::string& str) {
    std::string encoded;
    for (char c : str) {
        if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded += c;
        } else if (c == ' ') {
            encoded += "%20";
        } else {
            char buf[4];
            snprintf(buf, sizeof(buf), "%%%02X", (unsigned char)c);
            encoded += buf;
        }
    }
    return encoded;
}
#endif

std::vector<RadioStation> RadioManager::searchStations(const std::string& query, int offset, int limit) {
    std::vector<RadioStation> list;

#ifdef ARDUINO
    WiFiClient client;
    client.setTimeout(6000);

    HTTPClient http;
    http.setTimeout(6000);
    http.setUserAgent("CBDos-Radio/1.0 (ESP32-S3)");
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);

    std::string encodedQuery = urlEncodeQuery(query);
    char url[320];

    // Configurar filtro ArduinoJson para descartar campos irrelevantes y ahorrar RAM
    JsonDocument filter;
    filter[0]["name"] = true;
    filter[0]["url"] = true;
    filter[0]["url_resolved"] = true;
    filter[0]["country"] = true;
    filter[0]["tags"] = true;
    filter[0]["bitrate"] = true;

    // 1. Intentar búsqueda por nombre en HTTP plano (puerto 80 sin consumo de memoria SSL)
    snprintf(url, sizeof(url), "http://de1.api.radio-browser.info/json/stations/byname/%s?order=votes&reverse=true&limit=%d&offset=%d", encodedQuery.c_str(), limit, offset);
    Serial.printf("[RadioSearch] Consultando API HTTP: %s\n", url);

    if (http.begin(client, url)) {
        int httpCode = http.GET();
        Serial.printf("[RadioSearch] HTTP Code (Name Search): %d\n", httpCode);

        if (httpCode == HTTP_CODE_OK) {
            JsonDocument doc(&s_spiRamJsonAllocator);
            DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
            
            if (!err && doc.is<JsonArray>()) {
                JsonArray arr = doc.as<JsonArray>();
                for (JsonObject obj : arr) {
                    RadioStation st;
                    const char* n = obj["name"] | "Desconocida";
                    const char* u = obj["url_resolved"] | (obj["url"] | "");
                    const char* c = obj["country"] | "Global";
                    const char* g = obj["tags"] | "Varios";

                    strncpy(st.name, n, sizeof(st.name) - 1);
                    st.name[sizeof(st.name) - 1] = '\0';
                    strncpy(st.url, u, sizeof(st.url) - 1);
                    st.url[sizeof(st.url) - 1] = '\0';
                    strncpy(st.country, c, sizeof(st.country) - 1);
                    st.country[sizeof(st.country) - 1] = '\0';
                    strncpy(st.genre, g, sizeof(st.genre) - 1);
                    st.genre[sizeof(st.genre) - 1] = '\0';
                    st.bitrate = obj["bitrate"] | 128;
                    st.isFavorite = false;
                    
                    if (st.url[0] != '\0') {
                        list.push_back(st);
                    }
                }
            } else if (err) {
                Serial.printf("[RadioSearch] JSON Deserialization error: %s\n", err.c_str());
            }
        } else if (httpCode < 0) {
            Serial.printf("[RadioSearch] HTTP Error: %s\n", http.errorToString(httpCode).c_str());
        }
        http.end();
    }

    // 2. Si no encontró por nombre, intentar búsqueda por género/tag
    if (list.empty() && offset == 0) {
        snprintf(url, sizeof(url), "http://de1.api.radio-browser.info/json/stations/bytag/%s?order=votes&reverse=true&limit=%d&offset=%d", encodedQuery.c_str(), limit, offset);
        Serial.printf("[RadioSearch] Reintentando por Tag HTTP: %s\n", url);
        if (http.begin(client, url)) {
            int httpCode = http.GET();
            Serial.printf("[RadioSearch] HTTP Code (Tag Search): %d\n", httpCode);
            if (httpCode == HTTP_CODE_OK) {
                JsonDocument doc(&s_spiRamJsonAllocator);
                DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
                if (!err && doc.is<JsonArray>()) {
                    JsonArray arr = doc.as<JsonArray>();
                    for (JsonObject obj : arr) {
                        RadioStation st;
                        const char* n = obj["name"] | "Desconocida";
                        const char* u = obj["url_resolved"] | (obj["url"] | "");
                        const char* c = obj["country"] | "Global";
                        const char* g = obj["tags"] | "Varios";

                        strncpy(st.name, n, sizeof(st.name) - 1);
                        st.name[sizeof(st.name) - 1] = '\0';
                        strncpy(st.url, u, sizeof(st.url) - 1);
                        st.url[sizeof(st.url) - 1] = '\0';
                        strncpy(st.country, c, sizeof(st.country) - 1);
                        st.country[sizeof(st.country) - 1] = '\0';
                        strncpy(st.genre, g, sizeof(st.genre) - 1);
                        st.genre[sizeof(st.genre) - 1] = '\0';
                        st.bitrate = obj["bitrate"] | 128;
                        st.isFavorite = false;
                        
                        if (st.url[0] != '\0') {
                            list.push_back(st);
                        }
                    }
                }
            }
            http.end();
        }
    }

    Serial.printf("[RadioSearch] Total emisoras encontradas: %d\n", (int)list.size());
#endif

    // Marcar favoritas
    for (auto& st : list) {
        for (const auto& fav : favorites) {
            if (strcmp(fav.url, st.url) == 0) {
                st.isFavorite = true;
                break;
            }
        }
    }

    return list;
}
