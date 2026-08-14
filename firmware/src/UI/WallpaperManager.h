#ifndef WALLPAPER_MANAGER_H
#define WALLPAPER_MANAGER_H

#include <lvgl.h>
#include <vector>
#include <string>
#if defined(ARDUINO)
#include <Arduino.h>
#else
#include <string>
typedef std::string String;
#endif

class WallpaperManager {
public:
    static WallpaperManager& getInstance() {
        static WallpaperManager instance;
        return instance;
    }

    void init();
    void applyWallpaper(lv_obj_t* parent);
    void setWallpaper(const std::string& path);
    void restoreDefault();
    std::string getCurrentWallpaper() const { return currentPath; }
    bool isCustom() const { return !currentPath.empty() && currentPath != "default"; }
    std::vector<std::string> getAvailableWallpapers();

private:
    WallpaperManager() : currentPath("default") {}
    std::string currentPath;
};

#endif // WALLPAPER_MANAGER_H
