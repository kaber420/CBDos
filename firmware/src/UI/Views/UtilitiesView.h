#pragma once

#include <lvgl.h>
#include "../Components/HeaderBar.h"
#include <string>
#include <vector>

class UtilitiesView {
public:
    static lv_obj_t* create();

private:
    static HeaderBar* headerBar;
    
    // Subvistas / Pestañas
    static void buildTodoTab(lv_obj_t* parent);
    static void buildCalcTab(lv_obj_t* parent);
    static void buildStopwatchTab(lv_obj_t* parent);

    // Callbacks de destrucción
    static void screen_delete_cb(lv_event_t* e);
};
