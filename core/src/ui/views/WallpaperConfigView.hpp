#pragma once
#include "BaseView.hpp"
#include <lvgl.h>

namespace cbdos {
namespace ui {

class WallpaperConfigView : public BaseView {
public:
    WallpaperConfigView();
    ~WallpaperConfigView() override = default;

    bool onCreate(lv_obj_t* parent) override;

private:
    static void default_btn_cb(lv_event_t* e);
    static void animated_btn_cb(lv_event_t* e);
    static void wallpaper_select_cb(lv_event_t* e);
};

} // namespace ui
} // namespace cbdos
