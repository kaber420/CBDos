#pragma once
#include <lvgl.h>
#include "../Components/HeaderBar.h"
#include "../../Network/ConfigManager.h"
#include <vector>

class GatewayConfigView {
public:
    static lv_obj_t* create();

private:
    static HeaderBar* headerBar;
    static std::vector<GatewayConfig> gatewaysList;
    static GatewayConfig activeGw;

    static void select_gw_cb(lv_event_t* e);
    static void add_gw_cb(lv_event_t* e);
    static void delete_gw_cb(lv_event_t* e);
};
