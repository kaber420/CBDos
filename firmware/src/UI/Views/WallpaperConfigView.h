#ifndef WALLPAPER_CONFIG_VIEW_H
#define WALLPAPER_CONFIG_VIEW_H

#include <lvgl.h>
#include "../Components/HeaderBar.h"

class WallpaperConfigView {
public:
    static lv_obj_t* create();

private:
    static HeaderBar* headerBar;
    static void default_btn_cb(lv_event_t* e);
    static void wallpaper_select_cb(lv_event_t* e);
};

#endif // WALLPAPER_CONFIG_VIEW_H
