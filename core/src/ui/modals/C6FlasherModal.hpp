#pragma once
#include <lvgl.h>

namespace cbdos {
namespace ui {

class C6FlasherModal {
public:
    static void show(lv_obj_t* parent = nullptr);
    static void hide();
    static bool isVisible();

private:
    static lv_obj_t* s_modalMask;
    static lv_obj_t* s_barProgress;
    static lv_obj_t* s_lblStatus;
    static lv_obj_t* s_btnStart;
    static lv_obj_t* s_lblBtn;

    static void close_btn_cb(lv_event_t* e);
    static void start_flash_cb(lv_event_t* e);
};

} // namespace ui
} // namespace cbdos
