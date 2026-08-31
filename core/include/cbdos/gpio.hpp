#pragma once
#include <cstdint>
#include <cstddef>

namespace cbdos {
namespace gpio {

enum class PinMode {
    Input,
    Output,
    InputPullUp,
    InputPullDown
};

enum class PinLevel {
    Low = 0,
    High = 1
};

// ────────────────────────────────────────────────────────────────
// Contrato HAL C++ Puro para GPIOs
// ────────────────────────────────────────────────────────────────

class IGpioBackend {
public:
    virtual ~IGpioBackend() = default;

    virtual bool setPinMode(int pin, PinMode mode) = 0;
    virtual bool digitalWrite(int pin, PinLevel level) = 0;
    virtual PinLevel digitalRead(int pin) = 0;
    virtual bool isPinAvailable(int pin) const = 0;
};

void setBackend(IGpioBackend* backend);
IGpioBackend* getBackend();

// ────────────────────────────────────────────────────────────────
// APIs públicas de CBDos (Consumidas por Vistas, Apps y Lua)
// ────────────────────────────────────────────────────────────────

bool setPinMode(int pin, PinMode mode);
bool digitalWrite(int pin, PinLevel level);
PinLevel digitalRead(int pin);
bool isPinAvailable(int pin);

} // namespace gpio
} // namespace cbdos
