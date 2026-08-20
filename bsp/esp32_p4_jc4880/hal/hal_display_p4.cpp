#include "cbdos/display.hpp"
#include "DisplayHAL.h"
#include "LVGL_Port.h"

namespace cbdos {
namespace display {

static uint8_t s_brightness = 100;

bool init() {
    return (DisplayHAL::getInstance().init() == ESP_OK);
}

DisplayCaps getCapabilities() {
    DisplayCaps caps;
    caps.width = DisplayHAL::getInstance().getWidth();
    caps.height = DisplayHAL::getInstance().getHeight();
    caps.hasHardware2D = true;  // ESP32-P4 PPA / DMA2D
    caps.targetFps = 60;
    caps.isTouchSupported = true;
    return caps;
}

void setBrightness(uint8_t percent) {
    if (percent > 100) percent = 100;
    s_brightness = percent;
    DisplayHAL::getInstance().setBrightness(percent);
}

uint8_t getBrightness() {
    return DisplayHAL::getInstance().getBrightness();
}

void* getFramebuffer(int index) {
    return DisplayHAL::getInstance().getFrameBuffer(index);
}

bool lock(uint32_t timeout_ms) {
    return LVGL_Port::getInstance().lock(timeout_ms);
}

void unlock() {
    LVGL_Port::getInstance().unlock();
}

} // namespace display
} // namespace cbdos
