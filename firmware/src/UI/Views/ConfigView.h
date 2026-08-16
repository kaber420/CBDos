#pragma once
#include <lvgl.h>
#include "../Components/HeaderBar.h"

class ConfigView {
public:
    static lv_obj_t* create();

private:
    static HeaderBar* headerBar;
    static void btn_event_cb(lv_event_t* e);
    static void nvs_btn_event_cb(lv_event_t* e);
    static void nvs_timer_cb(lv_timer_t* timer);
    static void cancel_nvs_reset();

    static lv_timer_t* s_nvsTimer;
    static uint32_t s_nvsStartTime;
    static lv_obj_t* s_nvsBar;
    static lv_obj_t* s_nvsSubLabel;
};
