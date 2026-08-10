#pragma once
#include <lvgl.h>

class QuickSettingsPanel {
public:
    static void toggle(lv_obj_t* parentScreen);
    static void hide();

private:
    static lv_obj_t* panelObj;
    static lv_obj_t* overlayMask;

    static void mask_click_cb(lv_event_t* e);
    static void full_config_cb(lv_event_t* e);
};
