#include "WallpaperManager.h"
#include "Assets/default_wallpaper.h"
#include <cstdio>

#if defined(ARDUINO)
#include <Preferences.h>
#include <SD.h>
#endif

void WallpaperManager::init() {
#if defined(ARDUINO)
    Preferences prefs;
    if (prefs.begin("wallpaper", true)) {
        String path = prefs.getString("path", "default");
        currentPath = path.c_str();
        prefs.end();
    } else {
        currentPath = "default";
    }
#else
    currentPath = "default";
#endif
}

void WallpaperManager::applyWallpaper(lv_obj_t* parent) {
    if (!parent) return;

    if (currentPath == "default" || currentPath.empty()) {
        lv_obj_set_style_bg_image_src(parent, &default_wallpaper, 0);
    } else {
        // Formato LVFS (S:/... para MicroSD)
        lv_obj_set_style_bg_image_src(parent, currentPath.c_str(), 0);
    }
    lv_obj_set_style_bg_image_opa(parent, LV_OPA_COVER, 0);
}

void WallpaperManager::setWallpaper(const std::string& path) {
    currentPath = path;
#if defined(ARDUINO)
    Preferences prefs;
    if (prefs.begin("wallpaper", false)) {
        prefs.putString("path", currentPath.c_str());
        prefs.end();
    }
#endif
}

void WallpaperManager::restoreDefault() {
    setWallpaper("default");
}

std::vector<std::string> WallpaperManager::getAvailableWallpapers() {
    std::vector<std::string> list;
#if defined(ARDUINO)
    File dir = SD.open("/wallpapers");
    if (dir && dir.isDirectory()) {
        File file = dir.openNextFile();
        while (file) {
            if (!file.isDirectory()) {
                String fname = file.name();
                String lower = fname;
                lower.toLowerCase();
                if (lower.endsWith(".jpg") || lower.endsWith(".jpeg") || lower.endsWith(".png") || lower.endsWith(".bmp")) {
                    String fullPath = "S:/wallpapers/" + fname;
                    list.push_back(fullPath.c_str());
                }
            }
            file = dir.openNextFile();
        }
        dir.close();
    }
#endif
    return list;
}
