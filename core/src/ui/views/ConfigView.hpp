#pragma once
#include "BaseView.hpp"
#include <lvgl.h>

namespace cbdos {
namespace ui {

class ConfigView : public BaseView {
public:
    ConfigView();
    ~ConfigView() override = default;

    bool onCreate(lv_obj_t* parent) override;
    void onDestroy() override;

private:
    static void btn_event_cb(lv_event_t* e);
    static void nvs_btn_event_cb(lv_event_t* e);
    static void nvs_timer_cb(lv_timer_t* timer);
    static void cancel_nvs_reset();

    static lv_timer_t* s_nvsTimer;
    static uint32_t s_nvsStartTime;
    static lv_obj_t* s_nvsBar;
    static lv_obj_t* s_nvsSubLabel;
};

} // namespace ui
} // namespace cbdos
