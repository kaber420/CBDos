#pragma once

#include <string>
#include <vector>

#ifdef ARDUINO
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

struct RadioStation {
    std::string name;
    std::string url;
    std::string country;
    std::string genre;
    int bitrate = 128;
    bool isFavorite = false;
};

struct GenreTrio {
    const char* label1;
    const char* key1;
    const char* label2;
    const char* key2;
    const char* label3;
    const char* key3;
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

    // Emisoras por categoría desde memoria pura
    std::vector<RadioStation> getCategoryStations(const std::string& category);

    // Catálogo de tríos de géneros
    static size_t getTrioCount();
    static GenreTrio getTrio(size_t index);

private:
    RadioManager() = default;
    std::vector<RadioStation> favorites;
};
