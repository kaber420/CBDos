#pragma once
#include <string>
#include <lvgl.h>

namespace cbdos {
namespace ui {

class SystemIcons {
public:
    static void init();
    static const char* getSvgData(const std::string& appId);
    static lv_obj_t* createIcon(lv_obj_t* parent, const std::string& appId, int32_t size = 48);
};

} // namespace ui
} // namespace cbdos
