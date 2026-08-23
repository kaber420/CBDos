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

struct RadioPlaylist {
    std::string id;         // Identificador único (ej: "fav", "pl_retro")
    std::string name;       // Nombre visible (ej: "Favoritos", "Synthwave")
    std::vector<RadioStation> stations;
    bool isDefault = false; // true para la lista protegida "Favoritos"

    RadioPlaylist() : id("fav"), name("Favoritos"), isDefault(true) {}
    RadioPlaylist(const std::string& i, const std::string& n, bool def = false)
        : id(i), name(n), isDefault(def) {}
};

class RadioManager {
public:
    static RadioManager& getInstance() {
        static RadioManager instance;
        return instance;
    }

    void init();

    // ────────────────────────────────────────────────────────────
    //  APIs de Listas de Reproducción Multi-Lista
    // ────────────────────────────────────────────────────────────
    const std::vector<RadioPlaylist>& getPlaylists() const { return m_playlists; }
    RadioPlaylist* getPlaylist(const std::string& id);
    bool createPlaylist(const std::string& name);
    bool deletePlaylist(const std::string& id);
    bool addStationToPlaylist(const std::string& playlistId, const RadioStation& station);
    bool removeStationFromPlaylist(const std::string& playlistId, size_t stationIdx);
    void savePlaylists();
    void loadPlaylists();

    // ────────────────────────────────────────────────────────────
    //  Operaciones Portables con MicroSD (Import / Export)
    // ────────────────────────────────────────────────────────────
    bool exportPlaylistToSd(const std::string& playlistId, const std::string& sdPath);
    bool importPlaylistFromSd(const std::string& sdPath);
    bool importM3uFromSd(const std::string& sdPath);
    std::vector<std::string> scanPlaylistsOnSd();

    // ────────────────────────────────────────────────────────────
    //  Compatibilidad con Playlist "Favoritos" por defecto
    // ────────────────────────────────────────────────────────────
    const std::vector<RadioStation>& getFavorites() const;
    void addFavorite(const RadioStation& station);
    void updateFavorite(size_t index, const RadioStation& station);
    void removeFavorite(size_t index);
    void saveFavorites() { savePlaylists(); }
    void loadFavorites() { loadPlaylists(); }

    // Búsqueda en línea contra la API de radio-browser.info (agnóstico)
    std::vector<RadioStation> searchStations(const std::string& query, int offset = 0, int limit = 10);

private:
    RadioManager() = default;
    ~RadioManager() = default;
    RadioManager(const RadioManager&) = delete;
    RadioManager& operator=(const RadioManager&) = delete;

    std::vector<RadioPlaylist> m_playlists;
    std::vector<RadioStation> m_cachedFavorites; // Buffer auxiliar para compatibilidad
    bool m_initialized = false;
};

} // namespace audio
} // namespace cbdos
