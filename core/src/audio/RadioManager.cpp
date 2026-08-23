#if defined(ARDUINO)
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#else
#include <esp_http_client.h>
#include <cJSON.h>
#endif

#include "RadioManager.hpp"
#include "cbdos/storage.hpp"
#include "cbdos/network.hpp"
#include "cbdos/msgpack_util.hpp"
#include <cstdio>
#include <cstring>
#include <esp_log.h>
#include <cctype>
#include <sstream>

static const char* TAG = "RadioManager";
static const char* PLAYLISTS_STORAGE_PATH = "audio/playlists.msgpack";

namespace cbdos {
namespace audio {

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
    loadPlaylists();
}

RadioPlaylist* RadioManager::getPlaylist(const std::string& id) {
    for (auto& pl : m_playlists) {
        if (pl.id == id) return &pl;
    }
    return nullptr;
}

bool RadioManager::createPlaylist(const std::string& name) {
    if (name.empty()) return false;
    std::string id = "pl_" + std::to_string(m_playlists.size() + 1);
    m_playlists.push_back(RadioPlaylist(id, name, false));
    savePlaylists();
    return true;
}

bool RadioManager::deletePlaylist(const std::string& id) {
    for (size_t i = 0; i < m_playlists.size(); i++) {
        if (m_playlists[i].id == id) {
            if (m_playlists[i].isDefault) {
                ESP_LOGW(TAG, "No se puede eliminar la lista por defecto: %s", id.c_str());
                return false;
            }
            m_playlists.erase(m_playlists.begin() + i);
            savePlaylists();
            return true;
        }
    }
    return false;
}

bool RadioManager::addStationToPlaylist(const std::string& playlistId, const RadioStation& station) {
    RadioPlaylist* pl = getPlaylist(playlistId);
    if (!pl) return false;

    for (const auto& s : pl->stations) {
        if (s.url == station.url) return false; // Evitar duplicados
    }

    RadioStation copy = station;
    copy.isFavorite = true;
    pl->stations.push_back(copy);
    savePlaylists();
    return true;
}

bool RadioManager::removeStationFromPlaylist(const std::string& playlistId, size_t stationIdx) {
    RadioPlaylist* pl = getPlaylist(playlistId);
    if (!pl || stationIdx >= pl->stations.size()) return false;

    pl->stations.erase(pl->stations.begin() + stationIdx);
    savePlaylists();
    return true;
}

const std::vector<RadioStation>& RadioManager::getFavorites() const {
    for (const auto& pl : m_playlists) {
        if (pl.isDefault || pl.id == "fav") {
            return pl.stations;
        }
    }
    static const std::vector<RadioStation> emptyList;
    return emptyList;
}

void RadioManager::addFavorite(const RadioStation& station) {
    if (m_playlists.empty()) {
        m_playlists.push_back(RadioPlaylist("fav", "Favoritos", true));
    }
    addStationToPlaylist("fav", station);
}

void RadioManager::updateFavorite(size_t index, const RadioStation& station) {
    RadioPlaylist* pl = getPlaylist("fav");
    if (!pl && !m_playlists.empty()) pl = &m_playlists[0];
    if (pl && index < pl->stations.size()) {
        pl->stations[index] = station;
        pl->stations[index].isFavorite = true;
        savePlaylists();
    }
}

void RadioManager::removeFavorite(size_t index) {
    removeStationFromPlaylist("fav", index);
}

// ────────────────────────────────────────────────────────────────
//  Serialización / Deserialización MessagePack
// ────────────────────────────────────────────────────────────────
static std::string serializePlaylistsToMsgPack(const std::vector<RadioPlaylist>& playlists) {
    cbdos::msgpack::MsgPackWriter writer;
    writer.writeArrayHeader(playlists.size());

    for (const auto& pl : playlists) {
        writer.writeMapHeader(4);

        writer.writeString("id");
        writer.writeString(pl.id);

        writer.writeString("name");
        writer.writeString(pl.name);

        writer.writeString("def");
        writer.writeBool(pl.isDefault);

        writer.writeString("stations");
        writer.writeArrayHeader(pl.stations.size());
        for (const auto& st : pl.stations) {
            writer.writeMapHeader(6);
            writer.writeString("name");
            writer.writeString(st.name);

            writer.writeString("url");
            writer.writeString(st.url);

            writer.writeString("country");
            writer.writeString(st.country);

            writer.writeString("genre");
            writer.writeString(st.genre);

            writer.writeString("bitrate");
            writer.writeInt(st.bitrate);

            writer.writeString("fav");
            writer.writeBool(st.isFavorite);
        }
    }

    return writer.toString();
}

static bool deserializePlaylistsFromMsgPack(const std::string& data, std::vector<RadioPlaylist>& outPlaylists) {
    if (data.empty()) return false;

    cbdos::msgpack::MsgPackReader reader(data);
    size_t plCount = 0;
    if (!reader.readArrayHeader(plCount)) return false;

    outPlaylists.clear();
    for (size_t i = 0; i < plCount; i++) {
        size_t mapSize = 0;
        if (!reader.readMapHeader(mapSize)) break;

        RadioPlaylist pl;
        for (size_t m = 0; m < mapSize; m++) {
            std::string key;
            if (!reader.readString(key)) break;

            if (key == "id") {
                reader.readString(pl.id);
            } else if (key == "name") {
                reader.readString(pl.name);
            } else if (key == "def") {
                reader.readBool(pl.isDefault);
            } else if (key == "stations") {
                size_t stCount = 0;
                if (reader.readArrayHeader(stCount)) {
                    for (size_t s = 0; s < stCount; s++) {
                        size_t stMapSize = 0;
                        if (!reader.readMapHeader(stMapSize)) break;

                        RadioStation st;
                        for (size_t sm = 0; sm < stMapSize; sm++) {
                            std::string sKey;
                            if (!reader.readString(sKey)) break;

                            if (sKey == "name") reader.readString(st.name);
                            else if (sKey == "url") reader.readString(st.url);
                            else if (sKey == "country") reader.readString(st.country);
                            else if (sKey == "genre") reader.readString(st.genre);
                            else if (sKey == "bitrate") reader.readInt(st.bitrate);
                            else if (sKey == "fav") reader.readBool(st.isFavorite);
                            else reader.skipValue();
                        }
                        if (!st.url.empty()) {
                            pl.stations.push_back(st);
                        }
                    }
                }
            } else {
                reader.skipValue();
            }
        }
        outPlaylists.push_back(pl);
    }

    return !outPlaylists.empty();
}

void RadioManager::loadPlaylists() {
    m_playlists.clear();

    // 1. Intentar cargar desde Flash SPIFFS en formato MessagePack
    if (cbdos::storage::fileExists(PLAYLISTS_STORAGE_PATH)) {
        std::string rawData = cbdos::storage::readFile(PLAYLISTS_STORAGE_PATH);
        if (deserializePlaylistsFromMsgPack(rawData, m_playlists)) {
            ESP_LOGI(TAG, "Cargadas %d listas de reproduccion desde %s (MessagePack)", (int)m_playlists.size(), PLAYLISTS_STORAGE_PATH);
            return;
        }
    }

    // 2. Si no existe o está vacío, inicializar listas por defecto
    ESP_LOGI(TAG, "Inicializando listas por defecto en Flash SPIFFS...");
    RadioPlaylist favPlaylist("fav", "Favoritos", true);
    favPlaylist.stations.push_back(RadioStation("SomaFM Groove Salad", "http://ice1.somafm.com/groovesalad-128-mp3", "USA", "Ambient / Chill", 128, true));
    favPlaylist.stations.push_back(RadioStation("Ibiza Global Radio", "http://listento.ibizaglobalradio.com:8024/stream", "Espana", "Electronic", 128, true));
    favPlaylist.stations.push_back(RadioStation("Radio Paradise", "http://stream.radioparadise.com/mp3-128", "USA", "Rock / Eclectic", 128, true));
    favPlaylist.stations.push_back(RadioStation("SomaFM Secret Agent", "http://ice1.somafm.com/secretagent-128-mp3", "USA", "Spy / Lounge", 128, true));

    m_playlists.push_back(favPlaylist);
    savePlaylists();
}

void RadioManager::savePlaylists() {
    std::string binData = serializePlaylistsToMsgPack(m_playlists);
    if (!binData.empty()) {
        bool ok = cbdos::storage::writeFile(PLAYLISTS_STORAGE_PATH, binData);
        if (ok) {
            ESP_LOGI(TAG, "Guardadas %d listas en %s (MessagePack, %d bytes)", (int)m_playlists.size(), PLAYLISTS_STORAGE_PATH, (int)binData.size());
        } else {
            ESP_LOGE(TAG, "Error al escribir en %s", PLAYLISTS_STORAGE_PATH);
        }
    }
}

// ────────────────────────────────────────────────────────────────
//  Portabilidad con Tarjeta MicroSD
// ────────────────────────────────────────────────────────────────
bool RadioManager::exportPlaylistToSd(const std::string& playlistId, const std::string& sdPath) {
    RadioPlaylist* pl = getPlaylist(playlistId);
    if (!pl) return false;

    if (!cbdos::storage::isSdMounted()) {
        ESP_LOGW(TAG, "exportPlaylistToSd: MicroSD no montada.");
        return false;
    }

    std::vector<RadioPlaylist> singleList = { *pl };
    std::string binData = serializePlaylistsToMsgPack(singleList);
    return cbdos::storage::writeFile(sdPath.c_str(), binData);
}

bool RadioManager::importPlaylistFromSd(const std::string& sdPath) {
    if (!cbdos::storage::fileExists(sdPath.c_str())) return false;

    std::string rawData = cbdos::storage::readFile(sdPath.c_str());
    std::vector<RadioPlaylist> imported;
    if (!deserializePlaylistsFromMsgPack(rawData, imported)) return false;

    for (auto& pl : imported) {
        pl.isDefault = false; // Las listas importadas nunca sobreescriben la lista protegida por defecto
        pl.id = "pl_imp_" + std::to_string(m_playlists.size() + 1);
        m_playlists.push_back(pl);
    }

    savePlaylists();
    return true;
}

bool RadioManager::importM3uFromSd(const std::string& sdPath) {
    if (!cbdos::storage::fileExists(sdPath.c_str())) return false;

    std::string m3uContent = cbdos::storage::readFile(sdPath.c_str());
    if (m3uContent.empty()) return false;

    // Extraer nombre de archivo como nombre de lista
    std::string plName = "Importada M3U";
    size_t lastSlash = sdPath.find_last_of('/');
    if (lastSlash != std::string::npos) {
        plName = sdPath.substr(lastSlash + 1);
        size_t dot = plName.find_last_of('.');
        if (dot != std::string::npos) plName = plName.substr(0, dot);
    }

    RadioPlaylist newPl("pl_m3u_" + std::to_string(m_playlists.size() + 1), plName, false);

    std::istringstream stream(m3uContent);
    std::string line;
    std::string pendingTitle = "";

    while (std::getline(stream, line)) {
        // Limpiar retornos de carro
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) {
            line.pop_back();
        }
        if (line.empty()) continue;

        if (line.rfind("#EXTINF:", 0) == 0) {
            size_t comma = line.find(',');
            if (comma != std::string::npos && comma + 1 < line.size()) {
                pendingTitle = line.substr(comma + 1);
            }
        } else if (line[0] != '#') {
            if (line.rfind("http://", 0) == 0 || line.rfind("https://", 0) == 0) {
                std::string stName = pendingTitle.empty() ? "Radio Stream" : pendingTitle;
                newPl.stations.push_back(RadioStation(stName, line, "Global", "M3U", 128, false));
                pendingTitle.clear();
            }
        }
    }

    if (!newPl.stations.empty()) {
        m_playlists.push_back(newPl);
        savePlaylists();
        return true;
    }

    return false;
}

std::vector<std::string> RadioManager::scanPlaylistsOnSd() {
    std::vector<std::string> results;
    if (!cbdos::storage::isSdMounted()) return results;

    auto entries = cbdos::storage::listDir("/sdcard/playlists");
    for (const auto& entry : entries) {
        if (!entry.isDirectory) {
            if (entry.name.rfind(".msgpack") != std::string::npos || entry.name.rfind(".m3u") != std::string::npos) {
                results.push_back("/sdcard/playlists/" + entry.name);
            }
        }
    }
    return results;
}

// ────────────────────────────────────────────────────────────────
//  Búsqueda de Emisoras en Línea (radio-browser.info)
// ────────────────────────────────────────────────────────────────
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

    snprintf(url, sizeof(url), "http://de1.api.radio-browser.info/json/stations/byname/%s?order=votes&reverse=true&limit=%d&offset=%d", encodedQuery.c_str(), limit, offset);

    if (http.begin(client, url)) {
        int httpCode = http.GET();
        if (httpCode == HTTP_CODE_OK) {
            std::string payload = http.getString().c_str();
            size_t pos = 0;
            while ((pos = payload.find("\"name\":\"", pos)) != std::string::npos) {
                pos += 8;
                size_t endName = payload.find("\"", pos);
                if (endName == std::string::npos) break;
                std::string name = payload.substr(pos, endName - pos);

                std::string stUrl = "";
                size_t urlPos = payload.find("\"url_resolved\":\"", endName);
                if (urlPos != std::string::npos && urlPos < endName + 300) {
                    urlPos += 16;
                    size_t endUrl = payload.find("\"", urlPos);
                    if (endUrl != std::string::npos) {
                        stUrl = payload.substr(urlPos, endUrl - urlPos);
                    }
                }

                std::string genre = "General";
                size_t tagPos = payload.find("\"tags\":\"", endName);
                if (tagPos != std::string::npos && tagPos < endName + 500) {
                    tagPos += 8;
                    size_t endTag = payload.find("\"", tagPos);
                    if (endTag != std::string::npos) {
                        genre = payload.substr(tagPos, endTag - tagPos);
                    }
                }

                if (!stUrl.empty()) {
                    RadioStation st;
                    st.name = sanitizeString(name.c_str());
                    st.url = stUrl;
                    st.country = "Global";
                    st.genre = sanitizeString(genre.c_str());
                    st.bitrate = 128;
                    st.isFavorite = false;
                    list.push_back(st);
                }
                pos = endName;
            }
        }
        http.end();
    }

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
    const auto& favs = getFavorites();
    for (auto& st : list) {
        for (const auto& fav : favs) {
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
