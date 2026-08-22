#include "cbdos/display.hpp"
#include <Arduino.h>
#include <JC3248W535.h>

static JC3248W535_Display s_display;
static uint8_t s_brightness = 70;

namespace cbdos {
namespace display {

bool init() {
    if (!s_display.begin()) {
        return false;
    }
    pinMode(1, OUTPUT);
    analogWrite(1, (s_brightness * 255) / 100);
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
    analogWrite(1, (percent * 255) / 100);
}

uint8_t getBrightness() {
    return s_brightness;
}

void* getFramebuffer(int index) {
    (void)index;
    if (s_display.getCanvas()) {
        return (void*)s_display.getCanvas()->getFramebuffer();
    }
    return nullptr;
}

void flush() {
    s_display.flush();
}

bool lock(uint32_t timeout_ms) {
    (void)timeout_ms;
    return true;
}

void unlock() {
}

} // namespace display
} // namespace cbdos

JC3248W535_Display& get_s3_display_driver() {
    return s_display;
}
