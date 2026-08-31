#include "cbdos/gpio.hpp"

namespace cbdos {
namespace gpio {

static IGpioBackend* s_backend = nullptr;

void setBackend(IGpioBackend* backend) {
    s_backend = backend;
}

IGpioBackend* getBackend() {
    return s_backend;
}

bool setPinMode(int pin, PinMode mode) {
    if (s_backend) {
        return s_backend->setPinMode(pin, mode);
    }
    return false;
}

bool digitalWrite(int pin, PinLevel level) {
    if (s_backend) {
        return s_backend->digitalWrite(pin, level);
    }
    return false;
}

PinLevel digitalRead(int pin) {
    if (s_backend) {
        return s_backend->digitalRead(pin);
    }
    return PinLevel::Low;
}

bool isPinAvailable(int pin) {
    if (s_backend) {
        return s_backend->isPinAvailable(pin);
    }
    return false;
}

} // namespace gpio
} // namespace cbdos
