#include "LuappManager.hpp"
#include "cbdos/storage.hpp"
#include <lvgl.h>
#include <sstream>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace cbdos {
namespace lua {

LuappManager& LuappManager::getInstance() {
    static LuappManager instance;
    return instance;
}

std::string LuappManager::resolveIconSymbol(const std::string& iconName) {
    if (iconName.empty()) return LV_SYMBOL_FILE;

    // Si ya empieza por el byte UTF-8 de LVGL symbol (0xEF)
    if ((unsigned char)iconName[0] >= 0x80) {
        return iconName;
    }

    if (iconName == "LV_SYMBOL_AUDIO" || iconName == "audio" || iconName == "music") return LV_SYMBOL_AUDIO;
    if (iconName == "LV_SYMBOL_FILE" || iconName == "file") return LV_SYMBOL_FILE;
    if (iconName == "LV_SYMBOL_IMAGE" || iconName == "image" || iconName == "gallery") return LV_SYMBOL_IMAGE;
    if (iconName == "LV_SYMBOL_SETTINGS" || iconName == "settings" || iconName == "config") return LV_SYMBOL_SETTINGS;
    if (iconName == "LV_SYMBOL_WIFI" || iconName == "wifi" || iconName == "radio") return LV_SYMBOL_WIFI;
    if (iconName == "LV_SYMBOL_KEYBOARD" || iconName == "keyboard" || iconName == "terminal") return LV_SYMBOL_KEYBOARD;
    if (iconName == "LV_SYMBOL_DIRECTORY" || iconName == "directory" || iconName == "folder") return LV_SYMBOL_DIRECTORY;
    if (iconName == "LV_SYMBOL_LIST" || iconName == "list") return LV_SYMBOL_LIST;
    if (iconName == "LV_SYMBOL_EDIT" || iconName == "edit" || iconName == "editor") return LV_SYMBOL_EDIT;
    if (iconName == "LV_SYMBOL_DOWNLOAD" || iconName == "download") return LV_SYMBOL_DOWNLOAD;
    if (iconName == "LV_SYMBOL_EYE_OPEN" || iconName == "eye" || iconName == "browser") return LV_SYMBOL_EYE_OPEN;
    if (iconName == "LV_SYMBOL_PLAY" || iconName == "play") return LV_SYMBOL_PLAY;
    if (iconName == "LV_SYMBOL_PAUSE" || iconName == "pause") return LV_SYMBOL_PAUSE;
    if (iconName == "LV_SYMBOL_STOP" || iconName == "stop") return LV_SYMBOL_STOP;
    if (iconName == "LV_SYMBOL_POWER" || iconName == "power") return LV_SYMBOL_POWER;
    if (iconName == "LV_SYMBOL_BELL" || iconName == "bell") return LV_SYMBOL_BELL;
    if (iconName == "LV_SYMBOL_BATTERY_FULL" || iconName == "battery") return LV_SYMBOL_BATTERY_FULL;
    if (iconName == "LV_SYMBOL_DRIVE" || iconName == "drive" || iconName == "sd") return LV_SYMBOL_DRIVE;
    if (iconName == "LV_SYMBOL_REFRESH" || iconName == "refresh") return LV_SYMBOL_REFRESH;
    if (iconName == "LV_SYMBOL_TRASH" || iconName == "trash") return LV_SYMBOL_TRASH;
    if (iconName == "LV_SYMBOL_VOLUME_MAX" || iconName == "volume") return LV_SYMBOL_VOLUME_MAX;

    return LV_SYMBOL_FILE;
}

static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

bool LuappManager::parseLuappMetadata(const std::string& filePath, LuappInfo& outInfo) {
    outInfo.filePath = filePath;
    
    // Extraer nombre base por defecto
    std::string baseName = filePath;
    size_t slashPos = baseName.find_last_of("/\\");
    if (slashPos != std::string::npos) {
        baseName = baseName.substr(slashPos + 1);
    }
    size_t dotPos = baseName.rfind(".luapp");
    if (dotPos != std::string::npos) {
        baseName = baseName.substr(0, dotPos);
    }

    outInfo.name = baseName;
    outInfo.icon = "LV_SYMBOL_FILE";
    outInfo.iconSymbol = LV_SYMBOL_FILE;
    outInfo.accentColor = 0x00F5D4;
    outInfo.author = "Desconocido";
    outInfo.version = "1.0";
    outInfo.description = "";

    // Intentar leer primeros 1024 bytes
    FILE* f = fopen(filePath.c_str(), "rb");
    if (!f) {
        // Probar prefijo /sdcard/ si no estaba
        if (filePath.rfind("/sdcard/", 0) != 0) {
            std::string alt = std::string("/sdcard/") + (filePath[0] == '/' ? filePath.substr(1) : filePath);
            f = fopen(alt.c_str(), "rb");
        }
    }

    if (!f) {
        return false;
    }

    char buffer[1024];
    size_t bytesRead = fread(buffer, 1, sizeof(buffer) - 1, f);
    fclose(f);

    if (bytesRead == 0) {
        return true;
    }
    buffer[bytesRead] = '\0';

    std::istringstream stream(buffer);
    std::string line;

    while (std::getline(stream, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty()) continue;

        // Solo procesamos comentarios de cabecera que comiencen con -- @
        if (trimmed.rfind("-- @", 0) == 0 || trimmed.rfind("--@", 0) == 0) {
            size_t tagStart = trimmed.find('@');
            size_t colon = trimmed.find(':', tagStart);
            if (colon != std::string::npos) {
                std::string key = trim(trimmed.substr(tagStart + 1, colon - tagStart - 1));
                std::string val = trim(trimmed.substr(colon + 1));

                std::transform(key.begin(), key.end(), key.begin(), ::tolower);

                if (key == "name") {
                    outInfo.name = val;
                } else if (key == "icon") {
                    outInfo.icon = val;
                    outInfo.iconSymbol = resolveIconSymbol(val);
                } else if (key == "accent" || key == "color") {
                    if (val.rfind("#", 0) == 0) {
                        outInfo.accentColor = std::strtoul(val.c_str() + 1, nullptr, 16);
                    } else if (val.rfind("0x", 0) == 0 || val.rfind("0X", 0) == 0) {
                        outInfo.accentColor = std::strtoul(val.c_str() + 2, nullptr, 16);
                    } else {
                        outInfo.accentColor = std::strtoul(val.c_str(), nullptr, 16);
                    }
                } else if (key == "author") {
                    outInfo.author = val;
                } else if (key == "version") {
                    outInfo.version = val;
                } else if (key == "description") {
                    outInfo.description = val;
                }
            }
        } else if (trimmed.rfind("--", 0) != 0) {
            // Si encontramos código real fuera de comentarios, dejamos de parsear metadatos
            break;
        }
    }

    return true;
}

void LuappManager::scanApps(const char* dirPath) {
    m_discoveredApps.clear();

    const char* path = dirPath ? dirPath : "/sdcard/apps";
    auto entries = cbdos::storage::listDir(path);

    if (entries.empty() && strcmp(path, "/sdcard/apps") == 0) {
        // Intentar ruta alternativa /apps
        entries = cbdos::storage::listDir("/apps");
        if (!entries.empty()) {
            path = "/apps";
        }
    }

    for (const auto& entry : entries) {
        if (entry.isDirectory) continue;

        size_t len = entry.name.length();
        if (len > 6 && entry.name.substr(len - 6) == ".luapp") {
            std::string fullPath = std::string(path);
            if (fullPath.back() != '/') fullPath += '/';
            fullPath += entry.name;

            LuappInfo info;
            if (parseLuappMetadata(fullPath, info)) {
                m_discoveredApps.push_back(info);
            }
        }
    }
}

} // namespace lua
} // namespace cbdos
