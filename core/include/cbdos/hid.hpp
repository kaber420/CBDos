#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>

namespace cbdos {
namespace hid {

// Modificadores de teclado estándar HID
enum Modifier : uint8_t {
    MOD_NONE       = 0x00,
    MOD_LCTRL      = 0x01,
    MOD_LSHIFT     = 0x02,
    MOD_LALT       = 0x04,
    MOD_LGUI       = 0x08,
    MOD_RCTRL      = 0x10,
    MOD_RSHIFT     = 0x20,
    MOD_RALT       = 0x40,
    MOD_RGUI       = 0x80,
};

// Máscaras de estado de LEDs recibidas desde el Host (SET_REPORT)
enum LedMask : uint8_t {
    LED_NONE       = 0x00,
    LED_NUMLOCK    = 0x01,
    LED_CAPSLOCK   = 0x02,
    LED_SCROLLLOCK = 0x04,
    LED_COMPOSE    = 0x08,
    LED_KANA       = 0x10
};

// Botones de ratón
enum MouseButton : uint8_t {
    MOUSE_BTN_NONE   = 0x00,
    MOUSE_BTN_LEFT   = 0x01,
    MOUSE_BTN_RIGHT  = 0x02,
    MOUSE_BTN_MIDDLE = 0x04
};

// Keycodes estándar USB HID Usage Page 0x07 (Keyboard/Keypad)
namespace keycode {
    constexpr uint8_t KEY_NONE        = 0x00;
    constexpr uint8_t KEY_A           = 0x04;
    constexpr uint8_t KEY_B           = 0x05;
    constexpr uint8_t KEY_C           = 0x06;
    constexpr uint8_t KEY_D           = 0x07;
    constexpr uint8_t KEY_E           = 0x08;
    constexpr uint8_t KEY_F           = 0x09;
    constexpr uint8_t KEY_G           = 0x0A;
    constexpr uint8_t KEY_H           = 0x0B;
    constexpr uint8_t KEY_I           = 0x0C;
    constexpr uint8_t KEY_J           = 0x0D;
    constexpr uint8_t KEY_K           = 0x0E;
    constexpr uint8_t KEY_L           = 0x0F;
    constexpr uint8_t KEY_M           = 0x10;
    constexpr uint8_t KEY_N           = 0x11;
    constexpr uint8_t KEY_O           = 0x12;
    constexpr uint8_t KEY_P           = 0x13;
    constexpr uint8_t KEY_Q           = 0x14;
    constexpr uint8_t KEY_R           = 0x15;
    constexpr uint8_t KEY_S           = 0x16;
    constexpr uint8_t KEY_T           = 0x17;
    constexpr uint8_t KEY_U           = 0x18;
    constexpr uint8_t KEY_V           = 0x19;
    constexpr uint8_t KEY_W           = 0x1A;
    constexpr uint8_t KEY_X           = 0x1B;
    constexpr uint8_t KEY_Y           = 0x1C;
    constexpr uint8_t KEY_Z           = 0x1D;

    constexpr uint8_t KEY_1           = 0x1E;
    constexpr uint8_t KEY_2           = 0x1F;
    constexpr uint8_t KEY_3           = 0x20;
    constexpr uint8_t KEY_4           = 0x21;
    constexpr uint8_t KEY_5           = 0x22;
    constexpr uint8_t KEY_6           = 0x23;
    constexpr uint8_t KEY_7           = 0x24;
    constexpr uint8_t KEY_8           = 0x25;
    constexpr uint8_t KEY_9           = 0x26;
    constexpr uint8_t KEY_0           = 0x27;

    constexpr uint8_t KEY_ENTER       = 0x28;
    constexpr uint8_t KEY_ESCAPE      = 0x29;
    constexpr uint8_t KEY_BACKSPACE   = 0x2A;
    constexpr uint8_t KEY_TAB         = 0x2B;
    constexpr uint8_t KEY_SPACE       = 0x2C;
    constexpr uint8_t KEY_MINUS       = 0x2D;
    constexpr uint8_t KEY_EQUAL       = 0x2E;
    constexpr uint8_t KEY_LEFTBRACE   = 0x2F;
    constexpr uint8_t KEY_RIGHTBRACE  = 0x30;
    constexpr uint8_t KEY_BACKSLASH   = 0x31;
    constexpr uint8_t KEY_SEMICOLON   = 0x33;
    constexpr uint8_t KEY_QUOTE       = 0x34;
    constexpr uint8_t KEY_GRAVE       = 0x35;
    constexpr uint8_t KEY_COMMA       = 0x36;
    constexpr uint8_t KEY_DOT         = 0x37;
    constexpr uint8_t KEY_SLASH       = 0x38;
    constexpr uint8_t KEY_CAPSLOCK    = 0x39;

    constexpr uint8_t KEY_F1          = 0x3A;
    constexpr uint8_t KEY_F2          = 0x3B;
    constexpr uint8_t KEY_F3          = 0x3C;
    constexpr uint8_t KEY_F4          = 0x3D;
    constexpr uint8_t KEY_F5          = 0x3E;
    constexpr uint8_t KEY_F6          = 0x3F;
    constexpr uint8_t KEY_F7          = 0x40;
    constexpr uint8_t KEY_F8          = 0x41;
    constexpr uint8_t KEY_F9          = 0x42;
    constexpr uint8_t KEY_F10         = 0x43;
    constexpr uint8_t KEY_F11         = 0x44;
    constexpr uint8_t KEY_F12         = 0x45;

    constexpr uint8_t KEY_PRINTSCREEN = 0x46;
    constexpr uint8_t KEY_SCROLLLOCK  = 0x47;
    constexpr uint8_t KEY_PAUSE       = 0x48;
    constexpr uint8_t KEY_INSERT      = 0x49;
    constexpr uint8_t KEY_HOME        = 0x4A;
    constexpr uint8_t KEY_PAGEUP      = 0x4B;
    constexpr uint8_t KEY_DELETE      = 0x4C;
    constexpr uint8_t KEY_END         = 0x4D;
    constexpr uint8_t KEY_PAGEDOWN    = 0x4E;
    constexpr uint8_t KEY_RIGHT       = 0x4F;
    constexpr uint8_t KEY_LEFT        = 0x50;
    constexpr uint8_t KEY_DOWN        = 0x51;
    constexpr uint8_t KEY_UP          = 0x52;

    // MacroPad / OBS Hotkeys extendidos
    constexpr uint8_t KEY_F13         = 0x68;
    constexpr uint8_t KEY_F14         = 0x69;
    constexpr uint8_t KEY_F15         = 0x6A;
    constexpr uint8_t KEY_F16         = 0x6B;
    constexpr uint8_t KEY_F17         = 0x6C;
    constexpr uint8_t KEY_F18         = 0x6D;
    constexpr uint8_t KEY_F19         = 0x6E;
    constexpr uint8_t KEY_F20         = 0x6F;
    constexpr uint8_t KEY_F21         = 0x70;
    constexpr uint8_t KEY_F22         = 0x71;
    constexpr uint8_t KEY_F23         = 0x72;
    constexpr uint8_t KEY_F24         = 0x73;
}

// Interfaz abstracta del Driver Hardware (HAL)
class IHidDriver {
public:
    virtual ~IHidDriver() = default;

    virtual bool isConnected() = 0;
    virtual bool isReady() = 0;

    virtual void sendReport(uint8_t modifiers, const uint8_t keycodes[6]) = 0;
    virtual void sendMouseReport(uint8_t buttons, int8_t x, int8_t y, int8_t wheel) = 0;
    virtual uint8_t getHostLedState() = 0;
};

// API Agnóstica accesible para aplicaciones y Lua
void registerDriver(IHidDriver* driver);
bool isConnected();
bool isReady();

void sendKeyPress(uint8_t keycode, uint8_t modifiers = MOD_NONE);
void sendKeyRelease();
void sendCombo(const std::vector<uint8_t>& keycodes, uint8_t modifiers = MOD_NONE);
void sendString(const std::string& text, uint32_t char_delay_ms = 10);

void mouseMove(int8_t dx, int8_t dy, int8_t wheel = 0);
void mouseButtonPress(uint8_t buttons);
void mouseButtonRelease();
void mouseClick(MouseButton btn = MOUSE_BTN_LEFT);

uint8_t getLedState();
void onHostLedStateChanged(uint8_t new_state);
void setLedChangeCallback(std::function<void(uint8_t new_state)> cb);
bool waitForLedEvent(uint8_t expected_mask, uint32_t timeout_ms);

uint8_t charToKeycode(char c, uint8_t& out_modifier);
uint8_t nameToKeycode(const std::string& name);
uint8_t nameToModifier(const std::string& name);

} // namespace hid
} // namespace cbdos
