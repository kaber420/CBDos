#pragma once

#include "lvgl.h"

namespace cbdos {
namespace ui {

class AboutModal {
public:
    static void show(lv_obj_t* parent = nullptr);
    static void hide();

private:
    static lv_obj_t* s_modalMask;
    static void close_btn_cb(lv_event_t* e);
};

} // namespace ui
} // namespace cbdos
