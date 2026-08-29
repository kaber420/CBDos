#ifndef WALLPAPER_MANAGER_H
#define WALLPAPER_MANAGER_H

#include <lvgl.h>
#include <vector>
#include <string>

class WallpaperManager {
public:
    static WallpaperManager& getInstance() {
        static WallpaperManager instance;
        return instance;
    }

    void init();
    void applyWallpaper(lv_obj_t* parent);
    bool setWallpaper(const std::string& path);
    void restoreDefault();
    std::string getCurrentWallpaper() const { return currentPath; }
    bool isCustom() const { return hasCustomWallpaper; }
    bool isAnimated() const { return currentPath == "animated"; }
    void setAnimatedMode(lv_obj_t* parent);
    std::vector<std::string> getAvailableWallpapers();

private:
    WallpaperManager();
    ~WallpaperManager();

    bool ensureBufferAllocated();
    bool loadFromFlash();

    std::string currentPath;
    bool hasCustomWallpaper;
    uint8_t* customBuffer;
    lv_image_dsc_t customDsc;
};

#endif // WALLPAPER_MANAGER_H
