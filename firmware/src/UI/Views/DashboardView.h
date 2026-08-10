#pragma once
#include <lvgl.h>
#include <vector>
#include "../Components/HeaderBar.h"

struct AppDescriptor {
    int id;
    const char* title;
    const char* icon;
    uint32_t colorHex;
    void (*action)();
};

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

    static void btn_event_cb(lv_event_t * e);
    static void renderAppGrid(lv_obj_t* parent);
};
