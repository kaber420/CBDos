#pragma once
#include <lvgl.h>
#include "../Components/HeaderBar.h"

class ConfigView {
public:
    static lv_obj_t* create();

private:
    static HeaderBar* headerBar;
    static void btn_event_cb(lv_event_t* e);
};
