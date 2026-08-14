#pragma once
#include <lvgl.h>

class DoomView {
public:
    static lv_obj_t* create();

private:
    static void screenUnloadCb(lv_event_t* e);
    static void touchCb(lv_event_t* e);
};
