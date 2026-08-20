#pragma once
#include "BaseView.hpp"
#include "../../network/ConfigManager.h"
#include <lvgl.h>

namespace cbdos {
namespace ui {

class WiFiConfigView : public BaseView {
public:
    WiFiConfigView();
    ~WiFiConfigView() override = default;

    bool onCreate(lv_obj_t* parent) override;

private:
    static void toggle_pass_event_cb(lv_event_t* e);
    static void switch_event_cb(lv_event_t* e);
    static void enable_wifi_event_cb(lv_event_t* e);
    static void save_event_cb(lv_event_t* e);

    static WiFiConfig currentCfg;
    static lv_obj_t* swEnableWifi;
    static lv_obj_t* wifiSettingsBox;
    static lv_obj_t* lblWifiStatusText;
    static lv_obj_t* taSsid;
    static lv_obj_t* taPass;
    static lv_obj_t* btnTogglePass;
    static lv_obj_t* lblTogglePass;
    static bool passVisible;
    static lv_obj_t* taIp;
    static lv_obj_t* taGw;
    static lv_obj_t* swStatic;
    static lv_obj_t* staticContainer;
};


} // namespace ui
} // namespace cbdos
