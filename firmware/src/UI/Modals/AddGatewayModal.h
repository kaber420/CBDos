#pragma once
#include <lvgl.h>
#include <vector>
#include <string>

class AddGatewayModal {
public:
    static void show(lv_obj_t* parent);

private:
    static lv_obj_t* maskObj;
    static lv_obj_t* taPin;
    static lv_obj_t* dropdownFiles;
    static std::vector<std::string> encFiles;

    static void cancel_cb(lv_event_t* e);
    static void import_cb(lv_event_t* e);
};
