#pragma once
#include <lvgl.h>
#include "../components/HeaderBar.hpp"
#include "../../network/ConfigManager.h"

class WiFiConfigView {
public:
    static lv_obj_t* create();

private:
    static HeaderBar* headerBar;
    static WiFiConfig currentCfg;
    
    static lv_obj_t* taSsid;
    static lv_obj_t* taPass;
    static lv_obj_t* btnTogglePass;
    static lv_obj_t* lblTogglePass;
    static bool passVisible;
    static lv_obj_t* taIp;
    static lv_obj_t* taGw;
    static lv_obj_t* swStatic;
    static lv_obj_t* staticContainer;

    static void toggle_pass_event_cb(lv_event_t* e);
    static void save_event_cb(lv_event_t* e);
    static void switch_event_cb(lv_event_t* e);
    static void ta_event_cb(lv_event_t* e);
};

