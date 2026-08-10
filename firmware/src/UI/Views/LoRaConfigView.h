#pragma once
#include <lvgl.h>
#include "../Components/HeaderBar.h"
#include "../../Network/ConfigManager.h"

class LoRaConfigView {
public:
    static lv_obj_t* create();

private:
    static HeaderBar* headerBar;
    static LoRaConfig currentCfg;

    static lv_obj_t* taFreq;
    static lv_obj_t* taPwr;
    static lv_obj_t* taBw;
    static lv_obj_t* taSf;

    static void save_event_cb(lv_event_t* e);
};
