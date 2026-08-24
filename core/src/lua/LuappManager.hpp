#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace cbdos {
namespace lua {

struct LuappInfo {
    std::string filePath;
    std::string name;
    std::string icon;
    std::string iconSymbol;  // UTF-8 LVGL symbol or text
    uint32_t accentColor;
    std::string author;
    std::string version;
    std::string description;
};

class LuappManager {
public:
    static LuappManager& getInstance();

    void scanApps(const char* dirPath = "/sdcard/apps");
    const std::vector<LuappInfo>& getDiscoveredApps() const { return m_discoveredApps; }
    void clear() { m_discoveredApps.clear(); }

    // Parser para extraer metadatos de un archivo .luapp
    static bool parseLuappMetadata(const std::string& filePath, LuappInfo& outInfo);

private:
    LuappManager() = default;
    ~LuappManager() = default;
    LuappManager(const LuappManager&) = delete;
    LuappManager& operator=(const LuappManager&) = delete;

    static std::string resolveIconSymbol(const std::string& iconName);

    std::vector<LuappInfo> m_discoveredApps;
};

} // namespace lua
} // namespace cbdos
