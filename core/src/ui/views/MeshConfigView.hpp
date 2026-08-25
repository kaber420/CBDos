#pragma once
#include "BaseView.hpp"
#include "cbdos/mesh/mesh_engine.hpp"
#include <lvgl.h>
#include <vector>

namespace cbdos {
namespace ui {

class MeshConfigView : public BaseView {
public:
    MeshConfigView();
    ~MeshConfigView() override;

    bool onCreate(lv_obj_t* parent) override;

private:
    static void mode_select_cb(lv_event_t* e);
    static void scan_btn_cb(lv_event_t* e);
    static void channel_select_cb(lv_event_t* e);
    static void connect_tower_cb(lv_event_t* e);
    static void refresh_towers_ui();

    static MeshConfigView* s_instance;
    static lv_obj_t* s_towersListContainer;
    static lv_obj_t* s_lblScanStatus;
    static lv_obj_t* s_lblCurrentMode;
    static lv_obj_t* s_lblMac;
    static lv_obj_t* s_lblChannel;
    static lv_obj_t* s_ddMode;
    static lv_obj_t* s_btnScan;
};

} // namespace ui
} // namespace cbdos
