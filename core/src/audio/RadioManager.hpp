#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <cstddef>

namespace cbdos {
namespace audio {

struct RadioStation {
    std::string name;
    std::string url;
    std::string country;
    std::string genre;
    int bitrate = 128;
    bool isFavorite = false;

    RadioStation()
        : name(""), url(""), country("Global"), genre("General"), bitrate(128), isFavorite(false) {}

    RadioStation(const std::string& n, const std::string& u, const std::string& c, const std::string& g, int b = 128, bool fav = false)
        : name(n), url(u), country(c), genre(g), bitrate(b), isFavorite(fav) {}
};

class RadioManager {
public:
    static RadioManager& getInstance() {
        static RadioManager instance;
        return instance;
    }

    void init();
    const std::vector<RadioStation>& getFavorites() const { return m_favorites; }
    void addFavorite(const RadioStation& station);
    void updateFavorite(size_t index, const RadioStation& station);
    void removeFavorite(size_t index);
    void saveFavorites();
    void loadFavorites();

    // Búsqueda en línea contra la API de radio-browser.info (agnóstico)
    std::vector<RadioStation> searchStations(const std::string& query, int offset = 0, int limit = 10);

private:
    RadioManager() = default;
    ~RadioManager() = default;
    RadioManager(const RadioManager&) = delete;
    RadioManager& operator=(const RadioManager&) = delete;

    std::vector<RadioStation> m_favorites;
    bool m_initialized = false;
};

} // namespace audio
} // namespace cbdos
