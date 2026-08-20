#if defined(ARDUINO)
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <SD.h>
#else
#include <esp_http_client.h>
#include <cJSON.h>
#endif

#include "RadioManager.hpp"
#include "cbdos/network.hpp"
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <esp_log.h>
#include <cctype>

static const char* TAG = "RadioManager";
static const char* RADIOS_FILE_PATH = "/sdcard/audio/radios.json";

namespace cbdos {
namespace audio {

#if defined(ARDUINO)
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
#endif

static std::string sanitizeString(const char* src) {
    if (!src) return "";
    while (*src && (unsigned char)*src <= ' ') src++;
    std::string out;
    while (*src) {
        char c = *src++;
        if (c == '\t' || c == '\r' || c == '\n') {
            c = ' ';
        }
        if ((unsigned char)c >= ' ' || (unsigned char)c >= 0x80) {
            out += c;
        }
    }
    while (!out.empty() && out.back() == ' ') {
        out.pop_back();
    }
    return out;
}

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

void RadioManager::init() {
    if (m_initialized) return;
    m_initialized = true;
    loadFavorites();
}

void RadioManager::addFavorite(const RadioStation& station) {
    for (const auto& fav : m_favorites) {
        if (fav.url == station.url) return;
    }
    RadioStation copy = station;
    copy.isFavorite = true;
    m_favorites.push_back(copy);
    saveFavorites();
}

void RadioManager::updateFavorite(size_t index, const RadioStation& station) {
    if (index < m_favorites.size()) {
        m_favorites[index] = station;
        m_favorites[index].isFavorite = true;
        saveFavorites();
    }
}

void RadioManager::removeFavorite(size_t index) {
    if (index < m_favorites.size()) {
        m_favorites.erase(m_favorites.begin() + index);
        saveFavorites();
    }
}

void RadioManager::loadFavorites() {
    m_favorites.clear();

#if defined(ARDUINO)
    if (SD.cardType() != CARD_NONE && SD.exists("/audio/radios.json")) {
        File file = SD.open("/audio/radios.json", FILE_READ);
        if (file) {
            JsonDocument doc(&s_spiRamJsonAllocator);
            DeserializationError err = deserializeJson(doc, file);
            file.close();

            if (!err && doc.is<JsonObject>()) {
                JsonArray stationsArr = doc["stations"].as<JsonArray>();
                for (JsonObject sObj : stationsArr) {
                    RadioStation st;
                    st.name = sObj["name"] | "Radio";
                    st.url = sObj["url"] | "";
                    st.country = sObj["country"] | "Global";
                    st.genre = sObj["genre"] | "Varios";
                    st.bitrate = sObj["bitrate"] | 128;
                    st.isFavorite = true;
                    if (!st.url.empty()) {
                        m_favorites.push_back(st);
                    }
                }
            }
        }
    }
#else
    FILE* f = fopen(RADIOS_FILE_PATH, "r");
    if (f) {
        fseek(f, 0, SEEK_END);
        long fileSize = ftell(f);
        fseek(f, 0, SEEK_SET);

        if (fileSize > 0 && fileSize < 512 * 1024) {
            std::string content(fileSize, '\0');
            size_t nRead = fread(&content[0], 1, fileSize, f);
            (void)nRead;
            fclose(f);
            f = nullptr;

            cJSON* root = cJSON_Parse(content.c_str());
            if (root) {
                cJSON* stationsArr = cJSON_GetObjectItem(root, "stations");
                if (cJSON_IsArray(stationsArr)) {
                    int count = cJSON_GetArraySize(stationsArr);
                    for (int i = 0; i < count; i++) {
                        cJSON* item = cJSON_GetArrayItem(stationsArr, i);
                        if (item) {
                            cJSON* nameItem = cJSON_GetObjectItem(item, "name");
                            cJSON* urlItem = cJSON_GetObjectItem(item, "url");
                            cJSON* countryItem = cJSON_GetObjectItem(item, "country");
                            cJSON* genreItem = cJSON_GetObjectItem(item, "genre");
                            cJSON* bitrateItem = cJSON_GetObjectItem(item, "bitrate");

                            RadioStation st;
                            st.name = nameItem && nameItem->valuestring ? nameItem->valuestring : "Desconocida";
                            st.url = urlItem && urlItem->valuestring ? urlItem->valuestring : "";
                            st.country = countryItem && countryItem->valuestring ? countryItem->valuestring : "Global";
                            st.genre = genreItem && genreItem->valuestring ? genreItem->valuestring : "General";
                            st.bitrate = bitrateItem ? bitrateItem->valueint : 128;
                            st.isFavorite = true;

                            if (!st.url.empty()) {
                                m_favorites.push_back(st);
                            }
                        }
                    }
                }
                cJSON_Delete(root);
            }
        }
        if (f) fclose(f);
    }
#endif

    // Si no existen favoritos en la SD, cargar la selección por defecto
    if (m_favorites.empty()) {
        ESP_LOGI(TAG, "No se encontraron emisoras en %s. Cargando emisoras por defecto...", RADIOS_FILE_PATH);
        m_favorites.push_back(RadioStation("SomaFM Groove Salad", "http://ice1.somafm.com/groovesalad-128-mp3", "USA", "Ambient / Chill", 128, true));
        m_favorites.push_back(RadioStation("Ibiza Global Radio", "http://listento.ibizaglobalradio.com:8024/stream", "Espana", "Electronic", 128, true));
        m_favorites.push_back(RadioStation("Radio Paradise", "http://stream.radioparadise.com/mp3-128", "USA", "Rock / Eclectic", 128, true));
        m_favorites.push_back(RadioStation("SomaFM Secret Agent", "http://ice1.somafm.com/secretagent-128-mp3", "USA", "Spy / Lounge", 128, true));
        saveFavorites();
    } else {
        ESP_LOGI(TAG, "Cargadas %d emisoras favoritas desde %s", (int)m_favorites.size(), RADIOS_FILE_PATH);
    }
}

void RadioManager::saveFavorites() {
#if defined(ARDUINO)
    if (SD.cardType() != CARD_NONE) {
        if (!SD.exists("/audio")) {
            SD.mkdir("/audio");
        }
        File file = SD.open("/audio/radios.json", FILE_WRITE);
        if (file) {
            JsonDocument doc(&s_spiRamJsonAllocator);
            JsonArray stationsArr = doc["stations"].to<JsonArray>();
            for (const auto& st : m_favorites) {
                JsonObject sObj = stationsArr.add<JsonObject>();
                sObj["name"] = st.name.c_str();
                sObj["url"] = st.url.c_str();
                sObj["country"] = st.country.c_str();
                sObj["genre"] = st.genre.c_str();
                sObj["bitrate"] = st.bitrate;
            }
            serializeJson(doc, file);
            file.close();
            ESP_LOGI(TAG, "Guardadas %d emisoras en /audio/radios.json", (int)m_favorites.size());
        }
    }
#else
    struct stat st;
    if (stat("/sdcard/audio", &st) != 0) {
        mkdir("/sdcard/audio", 0777);
    }

    cJSON* root = cJSON_CreateObject();
    if (!root) return;

    cJSON* stationsArr = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "stations", stationsArr);

    for (const auto& fav : m_favorites) {
        cJSON* sObj = cJSON_CreateObject();
        cJSON_AddStringToObject(sObj, "name", fav.name.c_str());
        cJSON_AddStringToObject(sObj, "url", fav.url.c_str());
        cJSON_AddStringToObject(sObj, "country", fav.country.c_str());
        cJSON_AddStringToObject(sObj, "genre", fav.genre.c_str());
        cJSON_AddNumberToObject(sObj, "bitrate", fav.bitrate);
        cJSON_AddItemToArray(stationsArr, sObj);
    }

    char* jsonStr = cJSON_PrintUnformatted(root);
    if (jsonStr) {
        FILE* f = fopen(RADIOS_FILE_PATH, "w");
        if (f) {
            fputs(jsonStr, f);
            fclose(f);
            ESP_LOGI(TAG, "Guardadas %d emisoras en %s", (int)m_favorites.size(), RADIOS_FILE_PATH);
        } else {
            ESP_LOGW(TAG, "No se pudo abrir %s para escritura (errno %d: %s)", RADIOS_FILE_PATH, errno, strerror(errno));
        }
        cJSON_free(jsonStr);
    }
    cJSON_Delete(root);
#endif
}

std::vector<RadioStation> RadioManager::searchStations(const std::string& query, int offset, int limit) {
    std::vector<RadioStation> list;

    if (!cbdos::network::isConnected()) {
        ESP_LOGW(TAG, "Sin conexion WiFi. No se puede realizar peticion HTTP.");
        return list;
    }

#if defined(ARDUINO)
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

    // 1. Búsqueda por nombre en HTTP plano con streaming directo
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

                    st.name = sanitizeString(n);
                    if (st.name.empty()) st.name = "Desconocida";
                    st.url = u;
                    st.country = sanitizeString(c);
                    if (st.country.empty()) st.country = "Global";
                    st.genre = sanitizeString(g);
                    if (st.genre.empty()) st.genre = "General";
                    st.bitrate = obj["bitrate"] | 128;
                    st.isFavorite = false;

                    if (!st.url.empty()) {
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

                        st.name = sanitizeString(n);
                        if (st.name.empty()) st.name = "Desconocida";
                        st.url = u;
                        st.country = sanitizeString(c);
                        if (st.country.empty()) st.country = "Global";
                        st.genre = sanitizeString(g);
                        if (st.genre.empty()) st.genre = "General";
                        st.bitrate = obj["bitrate"] | 128;
                        st.isFavorite = false;

                        if (!st.url.empty()) {
                            list.push_back(st);
                        }
                    }
                }
            }
            http.end();
        }
    }

    Serial.printf("[RadioSearch] Total emisoras encontradas: %d\n", (int)list.size());

#else // ESP-IDF (ESP32-P4)

    auto executeQuery = [&](const char* path) -> std::vector<RadioStation> {
        std::vector<RadioStation> subList;
        char fullUrl[384];
        snprintf(fullUrl, sizeof(fullUrl), "http://de1.api.radio-browser.info%s", path);

        esp_http_client_config_t config = {};
        config.url = fullUrl;
        config.timeout_ms = 4000;
        config.disable_auto_redirect = false;
        config.max_redirection_count = 3;
        config.user_agent = "CBDos-Radio/1.0 (ESP32-P4)";
        config.buffer_size = 2048;
        config.buffer_size_tx = 512;

        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (!client) return subList;

        esp_http_client_set_header(client, "Connection", "close");

        esp_err_t err = esp_http_client_open(client, 0);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Error conexion HTTP: %s", esp_err_to_name(err));
            esp_http_client_cleanup(client);
            return subList;
        }

        int contentLength = esp_http_client_fetch_headers(client);
        int statusCode = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP %s -> Status: %d, Length: %d", fullUrl, statusCode, contentLength);

        if (statusCode == 200) {
            std::string response;
            if (contentLength > 0 && contentLength < 64 * 1024) {
                response.resize(contentLength);
                int totalRead = 0;
                while (totalRead < contentLength) {
                    int r = esp_http_client_read(client, &response[totalRead], contentLength - totalRead);
                    if (r <= 0) break;
                    totalRead += r;
                }
                response.resize(totalRead);
            } else {
                char buf[1024];
                int r = 0;
                while ((r = esp_http_client_read(client, buf, sizeof(buf) - 1)) > 0) {
                    buf[r] = '\0';
                    response.append(buf, r);
                    if (esp_http_client_is_complete_data_received(client)) break;
                    if (response.size() > 64 * 1024) break;
                }
            }

            if (!response.empty()) {
                cJSON* jsonArr = cJSON_Parse(response.c_str());
                if (jsonArr && cJSON_IsArray(jsonArr)) {
                    int count = cJSON_GetArraySize(jsonArr);
                    for (int i = 0; i < count; i++) {
                        cJSON* item = cJSON_GetArrayItem(jsonArr, i);
                        if (!item) continue;

                        cJSON* nameItem = cJSON_GetObjectItem(item, "name");
                        cJSON* urlResolvedItem = cJSON_GetObjectItem(item, "url_resolved");
                        cJSON* urlItem = cJSON_GetObjectItem(item, "url");
                        cJSON* countryItem = cJSON_GetObjectItem(item, "country");
                        cJSON* tagsItem = cJSON_GetObjectItem(item, "tags");
                        cJSON* bitrateItem = cJSON_GetObjectItem(item, "bitrate");

                        std::string sUrl = (urlResolvedItem && urlResolvedItem->valuestring && strlen(urlResolvedItem->valuestring) > 0)
                                           ? urlResolvedItem->valuestring
                                           : (urlItem && urlItem->valuestring ? urlItem->valuestring : "");

                        if (!sUrl.empty()) {
                            RadioStation st;
                            const char* rawName = (nameItem && nameItem->valuestring) ? nameItem->valuestring : "Desconocida";
                            const char* rawCountry = (countryItem && countryItem->valuestring) ? countryItem->valuestring : "Global";
                            const char* rawGenre = (tagsItem && tagsItem->valuestring) ? tagsItem->valuestring : "General";

                            st.name = sanitizeString(rawName);
                            if (st.name.empty()) st.name = "Desconocida";
                            st.url = sUrl;
                            st.country = sanitizeString(rawCountry);
                            if (st.country.empty()) st.country = "Global";
                            st.genre = sanitizeString(rawGenre);
                            if (st.genre.empty()) st.genre = "General";
                            st.bitrate = bitrateItem ? bitrateItem->valueint : 128;
                            st.isFavorite = false;
                            subList.push_back(st);
                        }
                    }
                }
                if (jsonArr) cJSON_Delete(jsonArr);
            }
        }

        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return subList;
    };

    std::string encodedQuery = urlEncodeQuery(query);
    char path[384];
    snprintf(path, sizeof(path), "/json/stations/byname/%s?order=votes&reverse=true&limit=%d&offset=%d", encodedQuery.c_str(), limit, offset);
    list = executeQuery(path);

    if (list.empty() && offset == 0) {
        snprintf(path, sizeof(path), "/json/stations/bytag/%s?order=votes&reverse=true&limit=%d&offset=%d", encodedQuery.c_str(), limit, offset);
        list = executeQuery(path);
    }
#endif

    // Marcar favoritas
    for (auto& st : list) {
        for (const auto& fav : m_favorites) {
            if (fav.url == st.url) {
                st.isFavorite = true;
                break;
            }
        }
    }

    return list;
}

} // namespace audio
} // namespace cbdos
