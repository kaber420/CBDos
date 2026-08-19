#include "cbdos/input.hpp"
#include "TouchHAL.h"

namespace cbdos {
namespace input {

bool init() {
    return (TouchHAL::getInstance().init() == ESP_OK);
}

bool getTouch(TouchPoint& point) {
    uint16_t x = 0, y = 0;
    bool pressed = false;
    TouchHAL::getInstance().read(&x, &y, &pressed);
    point.isPressed = pressed;
    point.x = x;
    point.y = y;
    point.fingerCount = pressed ? 1 : 0;
    return pressed;
}

} // namespace input
} // namespace cbdos
