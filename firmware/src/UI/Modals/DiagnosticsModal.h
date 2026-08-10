#pragma once
#include <lvgl.h>

struct SystemDiagnostics {
    bool sdMounted;
    bool sdHasConfigFile;
    bool wifiConnected;
    char wifiSsid[32];
    char ipAddress[32];
    char macAddress[32];
    int rssi;
    char hubIp[32];
    bool mqttConnected;
    uint32_t freeHeap;
    uint32_t freePsram;
    uint32_t uptimeSeconds;
};

class DiagnosticsModal {
public:
    static void show(lv_obj_t* parent, const SystemDiagnostics& diag);
    static void hide();

private:
    static lv_obj_t* modalMask;
    static void close_btn_cb(lv_event_t* e);
};
