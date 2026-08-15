#include "RadioManager.h"
#include "../../../Core/LVFS_Driver.h"
#include <cstdio>
#include <vector>
#include <string>

#ifdef ARDUINO
#include <Arduino.h>
#include <SD.h>
#include <ArduinoJson.h>
#endif

static const GenreTrio kGenreTrios[] = {
    {"Top", "topclick", "Rock", "rock", "Pop", "pop"},
    {"Chill", "chillout", "Jazz", "jazz", "Reggae", "reggae"},
    {"Metal", "metal", "Electro", "electro", "News", "news"},
    {"Latin", "latin", "Ambient", "ambient", "Classical", "classical"}
};

size_t RadioManager::getTrioCount() {
    return sizeof(kGenreTrios) / sizeof(kGenreTrios[0]);
}

GenreTrio RadioManager::getTrio(size_t index) {
    if (index >= getTrioCount()) return kGenreTrios[0];
    return kGenreTrios[index];
}

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
                        st.name = sObj["name"].as<const char*>() ? sObj["name"].as<const char*>() : "Radio";
                        st.url = sObj["url"].as<const char*>() ? sObj["url"].as<const char*>() : "";
                        st.country = sObj["country"].as<const char*>() ? sObj["country"].as<const char*>() : "Global";
                        st.genre = sObj["genre"].as<const char*>() ? sObj["genre"].as<const char*>() : "Varios";
                        st.bitrate = sObj["bitrate"].as<int>() ? sObj["bitrate"].as<int>() : 128;
                        st.isFavorite = true;
                        if (!st.url.empty()) {
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
        favorites.push_back({
            "SomaFM Groove Salad",
            "http://ice1.somafm.com/groovesalad-128-mp3",
            "USA",
            "Ambient / Chill",
            128,
            true
        });
        favorites.push_back({
            "Ibiza Global Radio",
            "http://listento.ibizaglobalradio.com:8024/stream",
            "Espana",
            "Electronic",
            128,
            true
        });
        favorites.push_back({
            "Radio Paradise",
            "http://stream.radioparadise.com/mp3-128",
            "USA",
            "Rock / Eclectic",
            128,
            true
        });
        favorites.push_back({
            "SomaFM Secret Agent",
            "http://ice1.somafm.com/secretagent-128-mp3",
            "USA",
            "Spy / Lounge",
            128,
            true
        });
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
                sObj["name"] = st.name.c_str();
                sObj["url"] = st.url.c_str();
                sObj["country"] = st.country.c_str();
                sObj["genre"] = st.genre.c_str();
                sObj["bitrate"] = st.bitrate;
            }
            serializeJson(doc, file);
            file.close();
        }
        lv_fs_spi_unlock();
    }
#endif
}

std::vector<RadioStation> RadioManager::getCategoryStations(const std::string& category) {
    std::vector<RadioStation> list;

    if (category == "rock") {
        list.push_back({"Radio Paradise Rock", "http://stream.radioparadise.com/rock-128", "USA", "Rock", 128, false});
        list.push_back({"Rock Antenne Classic", "http://stream.rockantenne.de/classic-perlen/stream/mp3", "Alemania", "Classic Rock", 128, false});
        list.push_back({"100hitz 90s Rock", "http://ghbr.streamguys1.com/90srock-mp3", "USA", "90s Rock", 128, false});
        list.push_back({"SomaFM Indie Pop Rocks", "http://ice1.somafm.com/indiepop-128-mp3", "USA", "Indie Rock", 128, false});
    } else if (category == "pop") {
        list.push_back({"Capital FM UK", "http://media-the.musicradio.com/CapitalMP3", "UK", "Pop / Hits", 128, false});
        list.push_back({"Hit Radio FFH", "http://mp3.ffh.de/radioffh/hqlivestream.mp3", "Alemania", "Pop", 128, false});
        list.push_back({"100hitz Hot Hitz", "http://ghbr.streamguys1.com/hothitz-mp3", "USA", "Top 40 Pop", 128, false});
    } else if (category == "chillout") {
        list.push_back({"SomaFM Groove Salad", "http://ice1.somafm.com/groovesalad-128-mp3", "USA", "Ambient Chill", 128, false});
        list.push_back({"SomaFM Drone Zone", "http://ice1.somafm.com/dronezone-128-mp3", "USA", "Atmospheric", 128, false});
        list.push_back({"Chillsky Radio", "http://hyades.shoutca.st:8043/stream", "Global", "Lofi / Chill", 128, false});
    } else if (category == "jazz") {
        list.push_back({"Swiss Jazz", "http://stream.srg-ssr.ch/m/rjs/mp3_128", "Suiza", "Smooth Jazz", 128, false});
        list.push_back({"SomaFM Sonic Universe", "http://ice1.somafm.com/sonicuniverse-128-mp3", "USA", "Nu Jazz", 128, false});
    } else if (category == "reggae") {
        list.push_back({"1.FM ReggaeTrade", "http://sc-reggae.1.fm:7034/stream", "Suiza", "Reggae / Roots", 128, false});
        list.push_back({"Joint Radio Reggae", "http://reggae.jointil.net:8000/stream", "Israel", "Roots / Dub", 128, false});
        list.push_back({"SomaFM Heavyweight Reggae", "http://ice1.somafm.com/reggae-128-mp3", "USA", "Reggae / Dub", 128, false});
    } else if (category == "metal") {
        list.push_back({"Metal Rock FM", "http://stream.metalrock.fm:8000/stream", "Global", "Heavy Metal", 128, false});
        list.push_back({"Chroma Metal", "http://chromaradio.com:8006/stream", "Grecia", "Power / Thrash", 128, false});
    } else if (category == "electro") {
        list.push_back({"Ibiza Global Radio", "http://listento.ibizaglobalradio.com:8024/stream", "Espana", "Electronic", 128, false});
        list.push_back({"Defected Radio", "http://icecast.defected.com/defected", "UK", "House / Club", 128, false});
    } else if (category == "news") {
        list.push_back({"BBC World Service", "http://stream.live.vc.bbcmedia.co.uk/bbc_world_service", "UK", "News", 128, false});
        list.push_back({"RNE Radio Nacional", "http://rtve.stream.flumotion.com/rtve/radio1.mp3", "Espana", "Noticias", 128, false});
    } else if (category == "latin") {
        list.push_back({"Fiesta Latina", "http://stream.fiestalatina.be:8000/stream", "Global", "Salsa / Bachata", 128, false});
        list.push_back({"Radio Salsa", "http://stream.radiosalsa.cl:8000/stream", "Chile", "Salsa Clasica", 128, false});
    } else if (category == "ambient") {
        list.push_back({"SomaFM Deep Space One", "http://ice1.somafm.com/deepspaceone-128-mp3", "USA", "Deep Ambient", 128, false});
        list.push_back({"SomaFM Space Station", "http://ice1.somafm.com/spacestation-128-mp3", "USA", "Space Ambient", 128, false});
    } else if (category == "classical") {
        list.push_back({"Swiss Classic", "http://stream.srg-ssr.ch/m/rsc_de/mp3_128", "Suiza", "Classical", 128, false});
        list.push_back({"KUSC Classical", "http://kusc-live.streamguys1.com/kusc-128k-mp3", "USA", "Symphonic", 128, false});
    } else { // topclick
        list.push_back({"SomaFM Groove Salad", "http://ice1.somafm.com/groovesalad-128-mp3", "USA", "Chill / Beats", 128, false});
        list.push_back({"Ibiza Global Radio", "http://listento.ibizaglobalradio.com:8024/stream", "Espana", "Electronic", 128, false});
        list.push_back({"Radio Paradise", "http://stream.radioparadise.com/mp3-128", "USA", "Eclectic Rock", 128, false});
        list.push_back({"SomaFM Secret Agent", "http://ice1.somafm.com/secretagent-128-mp3", "USA", "Spy Lounge", 128, false});
    }

    // Marcar favoritas
    for (auto& st : list) {
        for (const auto& fav : favorites) {
            if (fav.url == st.url) {
                st.isFavorite = true;
                break;
            }
        }
    }

    return list;
}
