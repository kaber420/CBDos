#pragma once

#include <string>
#include <vector>

#ifdef ARDUINO
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

struct RadioStation {
    char name[64];
    char url[192];
    char country[32];
    char genre[32];
    int bitrate = 128;
    bool isFavorite = false;

    RadioStation() {
        name[0] = '\0';
        url[0] = '\0';
        country[0] = '\0';
        genre[0] = '\0';
        bitrate = 128;
        isFavorite = false;
    }
};

class RadioManager {
public:
    static RadioManager& getInstance() {
        static RadioManager instance;
        return instance;
    }

    void init();
    const std::vector<RadioStation>& getFavorites();
    void addFavorite(const RadioStation& station);
    void updateFavorite(size_t index, const RadioStation& station);
    void removeFavorite(size_t index);
    void saveFavorites();
    void loadFavorites();

    // Emisoras desde API radio-browser
    std::vector<RadioStation> searchStations(const std::string& query, int offset, int limit);

private:
    RadioManager() = default;
    std::vector<RadioStation> favorites;
};
