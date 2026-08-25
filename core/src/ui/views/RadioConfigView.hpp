#pragma once

#include "BaseView.hpp"
#include "cbdos/radio.hpp"
#include <vector>

namespace cbdos {
namespace ui {

class RadioConfigView : public BaseView {
public:
    RadioConfigView();
    virtual ~RadioConfigView();

    bool onCreate(lv_obj_t* parent) override;
    void onDestroy() override;

private:
    static void radio_power_sw_cb(lv_event_t* e);
    static void mode_select_cb(lv_event_t* e);
    static void channel_slider_cb(lv_event_t* e);
    static void txpower_slider_cb(lv_event_t* e);

    // Wi-Fi Scan & Connect
    static void wifi_scan_btn_cb(lv_event_t* e);
    static void wifi_ap_click_cb(lv_event_t* e);
    static void wifi_connect_dialog_cb(lv_event_t* e);
    static void wifi_save_manual_cb(lv_event_t* e);

    // ESP-NOW / LR Channel Sweep
    static void sweep_btn_cb(lv_event_t* e);
    static void tune_node_channel_cb(lv_event_t* e);

    static void save_radio_config_cb(lv_event_t* e);

    static void update_mode_visibility();
    static void refresh_wifi_list_ui();
    static void refresh_sweep_list_ui();

    static RadioConfigView* s_instance;
    static cbdos::radio::RadioConfig s_cfg;

    // UI Widgets
    static lv_obj_t* s_swPower;
    static lv_obj_t* s_lblPowerStatus;
    static lv_obj_t* s_ddMode;
    static lv_obj_t* s_sliderChannel;
    static lv_obj_t* s_lblChannelVal;
    static lv_obj_t* s_sliderTx;
    static lv_obj_t* s_lblTxVal;

    static lv_obj_t* s_boxWifi;
    static lv_obj_t* s_btnScanWifi;
    static lv_obj_t* s_lblWifiScanStatus;
    static lv_obj_t* s_wifiListContainer;

    static lv_obj_t* s_boxMesh;
    static lv_obj_t* s_btnSweep;
    static lv_obj_t* s_barSweepProgress;
    static lv_obj_t* s_lblSweepStatus;
    static lv_obj_t* s_sweepListContainer;

    static std::vector<cbdos::radio::WifiApInfo> s_scannedAps;
    static std::vector<cbdos::radio::DiscoveredNode> s_sweptNodes;
};

} // namespace ui
} // namespace cbdos
