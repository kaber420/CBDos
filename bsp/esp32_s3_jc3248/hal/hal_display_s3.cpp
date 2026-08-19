#include "cbdos/display.hpp"
#include <Arduino.h>
#include <JC3248W535.h>

static JC3248W535_Display s_display;
static uint8_t s_brightness = 100;

namespace cbdos {
namespace display {

bool init() {
    if (!s_display.begin()) {
        return false;
    }
    s_display.backlightOn();
    return true;
}

DisplayCaps getCapabilities() {
    DisplayCaps caps;
    caps.width = 320;
    caps.height = 480;
    caps.hasHardware2D = false;  // Render por software en S3
    caps.targetFps = 30;
    caps.isTouchSupported = true;
    return caps;
}

void setBrightness(uint8_t percent) {
    if (percent > 100) percent = 100;
    s_brightness = percent;
    if (percent == 0) {
        s_display.backlightOff();
    } else {
        s_display.backlightOn();
    }
}

uint8_t getBrightness() {
    return s_brightness;
}

void* getFramebuffer(int index) {
    (void)index;
    return s_display.getCanvas();
}

} // namespace display
} // namespace cbdos

JC3248W535_Display& get_s3_display_driver() {
    return s_display;
}
