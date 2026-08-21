#pragma once
#include <lvgl.h>
#include <string>

namespace cbdos {
namespace ui {

class CalculatorApp {
public:
    static void build(lv_obj_t* parent);
    static void cleanup();

private:
    static void updateDisplay();
    static std::string formatNumber(double val);
    static void btnEventCb(lv_event_t* e);
};

} // namespace ui
} // namespace cbdos
