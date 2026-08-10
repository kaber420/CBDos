#pragma once
#include <lvgl.h>
#include "../Components/HeaderBar.h"

class DashboardView {
public:
    typedef void (*CommandCallback)(int cmdId);
    static void setCommandCallback(CommandCallback cb) { commandCb = cb; }

    static lv_obj_t* create();
    static void refreshState();
    static HeaderBar* getHeaderBar() { return headerBar; }

private:
    static HeaderBar* headerBar;
    static CommandCallback commandCb;
    static lv_obj_t* ordersBtnLabel;
    static lv_obj_t* ordersBtnIcon;

    static void btn_event_cb(lv_event_t * e);
};
