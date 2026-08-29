#include "cbdos/hid.hpp"
#include "cbdos/system.hpp"
#include <algorithm>
#include <cctype>
#include <map>

namespace cbdos {
namespace hid {

static IHidDriver* s_driver = nullptr;
static uint8_t s_hostLedState = 0;
static std::function<void(uint8_t)> s_ledCallback = nullptr;

void registerDriver(IHidDriver* driver) {
    s_driver = driver;
}

bool isConnected() {
    return s_driver ? s_driver->isConnected() : false;
}

bool isReady() {
    return s_driver ? s_driver->isReady() : false;
}

void sendKeyPress(uint8_t keycode, uint8_t modifiers) {
    if (!s_driver) return;
    uint8_t keys[6] = { keycode, 0, 0, 0, 0, 0 };
    s_driver->sendReport(modifiers, keys);
}

void sendKeyRelease() {
    if (!s_driver) return;
    uint8_t keys[6] = { 0, 0, 0, 0, 0, 0 };
    s_driver->sendReport(MOD_NONE, keys);
}

void sendCombo(const std::vector<uint8_t>& keycodes, uint8_t modifiers) {
    if (!s_driver) return;
    uint8_t keys[6] = { 0 };
    size_t count = std::min(keycodes.size(), static_cast<size_t>(6));
    for (size_t i = 0; i < count; ++i) {
        keys[i] = keycodes[i];
    }
    s_driver->sendReport(modifiers, keys);
    cbdos::system::sleepMs(20);
    sendKeyRelease();
}

void sendString(const std::string& text, uint32_t char_delay_ms) {
    if (!s_driver) return;

    for (char c : text) {
        uint8_t mod = MOD_NONE;
        uint8_t code = charToKeycode(c, mod);
        if (code != keycode::KEY_NONE) {
            sendKeyPress(code, mod);
            cbdos::system::sleepMs(char_delay_ms > 5 ? 5 : char_delay_ms);
            sendKeyRelease();
            cbdos::system::sleepMs(char_delay_ms);
        }
    }
}

void mouseMove(int8_t dx, int8_t dy, int8_t wheel) {
    if (!s_driver) return;
    s_driver->sendMouseReport(MOUSE_BTN_NONE, dx, dy, wheel);
}

void mouseButtonPress(uint8_t buttons) {
    if (!s_driver) return;
    s_driver->sendMouseReport(buttons, 0, 0, 0);
}

void mouseButtonRelease() {
    if (!s_driver) return;
    s_driver->sendMouseReport(MOUSE_BTN_NONE, 0, 0, 0);
}

void mouseClick(MouseButton btn) {
    if (!s_driver) return;
    mouseButtonPress(static_cast<uint8_t>(btn));
    cbdos::system::sleepMs(15);
    mouseButtonRelease();
}

uint8_t getLedState() {
    if (s_driver) {
        s_hostLedState = s_driver->getHostLedState();
    }
    return s_hostLedState;
}

void onHostLedStateChanged(uint8_t new_state) {
    s_hostLedState = new_state;
    if (s_ledCallback) {
        s_ledCallback(new_state);
    }
}

void setLedChangeCallback(std::function<void(uint8_t new_state)> cb) {
    s_ledCallback = cb;
}

bool waitForLedEvent(uint8_t expected_mask, uint32_t timeout_ms) {
    uint32_t start = cbdos::system::getTimeMs();
    uint8_t initial = getLedState();

    while ((cbdos::system::getTimeMs() - start) < timeout_ms) {
        uint8_t current = getLedState();
        if ((current & expected_mask) != (initial & expected_mask)) {
            return true;
        }
        cbdos::system::sleepMs(10);
    }
    return false;
}

uint8_t charToKeycode(char c, uint8_t& out_modifier) {
    out_modifier = MOD_NONE;

    if (c >= 'a' && c <= 'z') {
        return keycode::KEY_A + (c - 'a');
    }
    if (c >= 'A' && c <= 'Z') {
        out_modifier = MOD_LSHIFT;
        return keycode::KEY_A + (c - 'A');
    }
    if (c >= '1' && c <= '9') {
        return keycode::KEY_1 + (c - '1');
    }
    if (c == '0') return keycode::KEY_0;
    if (c == '\n') return keycode::KEY_ENTER;
    if (c == '\t') return keycode::KEY_TAB;
    if (c == ' ')  return keycode::KEY_SPACE;

    switch (c) {
        case '!': out_modifier = MOD_LSHIFT; return keycode::KEY_1;
        case '@': out_modifier = MOD_LSHIFT; return keycode::KEY_2;
        case '#': out_modifier = MOD_LSHIFT; return keycode::KEY_3;
        case '$': out_modifier = MOD_LSHIFT; return keycode::KEY_4;
        case '%': out_modifier = MOD_LSHIFT; return keycode::KEY_5;
        case '^': out_modifier = MOD_LSHIFT; return keycode::KEY_6;
        case '&': out_modifier = MOD_LSHIFT; return keycode::KEY_7;
        case '*': out_modifier = MOD_LSHIFT; return keycode::KEY_8;
        case '(': out_modifier = MOD_LSHIFT; return keycode::KEY_9;
        case ')': out_modifier = MOD_LSHIFT; return keycode::KEY_0;
        case '-': return keycode::KEY_MINUS;
        case '_': out_modifier = MOD_LSHIFT; return keycode::KEY_MINUS;
        case '=': return keycode::KEY_EQUAL;
        case '+': out_modifier = MOD_LSHIFT; return keycode::KEY_EQUAL;
        case '[': return keycode::KEY_LEFTBRACE;
        case '{': out_modifier = MOD_LSHIFT; return keycode::KEY_LEFTBRACE;
        case ']': return keycode::KEY_RIGHTBRACE;
        case '}': out_modifier = MOD_LSHIFT; return keycode::KEY_RIGHTBRACE;
        case '\\': return keycode::KEY_BACKSLASH;
        case '|': out_modifier = MOD_LSHIFT; return keycode::KEY_BACKSLASH;
        case ';': return keycode::KEY_SEMICOLON;
        case ':': out_modifier = MOD_LSHIFT; return keycode::KEY_SEMICOLON;
        case '\'': return keycode::KEY_QUOTE;
        case '"': out_modifier = MOD_LSHIFT; return keycode::KEY_QUOTE;
        case '`': return keycode::KEY_GRAVE;
        case '~': out_modifier = MOD_LSHIFT; return keycode::KEY_GRAVE;
        case ',': return keycode::KEY_COMMA;
        case '<': out_modifier = MOD_LSHIFT; return keycode::KEY_COMMA;
        case '.': return keycode::KEY_DOT;
        case '>': out_modifier = MOD_LSHIFT; return keycode::KEY_DOT;
        case '/': return keycode::KEY_SLASH;
        case '?': out_modifier = MOD_LSHIFT; return keycode::KEY_SLASH;
        default: return keycode::KEY_NONE;
    }
}

uint8_t nameToKeycode(const std::string& name) {
    std::string s = name;
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);

    if (s.length() == 1) {
        uint8_t mod = 0;
        return charToKeycode(s[0], mod);
    }

    static const std::map<std::string, uint8_t> keyMap = {
        {"ENTER", keycode::KEY_ENTER},
        {"RETURN", keycode::KEY_ENTER},
        {"ESC", keycode::KEY_ESCAPE},
        {"ESCAPE", keycode::KEY_ESCAPE},
        {"BACKSPACE", keycode::KEY_BACKSPACE},
        {"TAB", keycode::KEY_TAB},
        {"SPACE", keycode::KEY_SPACE},
        {"CAPSLOCK", keycode::KEY_CAPSLOCK},
        {"PRINTSCREEN", keycode::KEY_PRINTSCREEN},
        {"SCROLLLOCK", keycode::KEY_SCROLLLOCK},
        {"PAUSE", keycode::KEY_PAUSE},
        {"INSERT", keycode::KEY_INSERT},
        {"HOME", keycode::KEY_HOME},
        {"PAGEUP", keycode::KEY_PAGEUP},
        {"DELETE", keycode::KEY_DELETE},
        {"END", keycode::KEY_END},
        {"PAGEDOWN", keycode::KEY_PAGEDOWN},
        {"RIGHT", keycode::KEY_RIGHT},
        {"LEFT", keycode::KEY_LEFT},
        {"DOWN", keycode::KEY_DOWN},
        {"UP", keycode::KEY_UP},

        {"F1", keycode::KEY_F1},
        {"F2", keycode::KEY_F2},
        {"F3", keycode::KEY_F3},
        {"F4", keycode::KEY_F4},
        {"F5", keycode::KEY_F5},
        {"F6", keycode::KEY_F6},
        {"F7", keycode::KEY_F7},
        {"F8", keycode::KEY_F8},
        {"F9", keycode::KEY_F9},
        {"F10", keycode::KEY_F10},
        {"F11", keycode::KEY_F11},
        {"F12", keycode::KEY_F12},

        {"F13", keycode::KEY_F13},
        {"F14", keycode::KEY_F14},
        {"F15", keycode::KEY_F15},
        {"F16", keycode::KEY_F16},
        {"F17", keycode::KEY_F17},
        {"F18", keycode::KEY_F18},
        {"F19", keycode::KEY_F19},
        {"F20", keycode::KEY_F20},
        {"F21", keycode::KEY_F21},
        {"F22", keycode::KEY_F22},
        {"F23", keycode::KEY_F23},
        {"F24", keycode::KEY_F24}
    };

    auto it = keyMap.find(s);
    if (it != keyMap.end()) {
        return it->second;
    }
    return keycode::KEY_NONE;
}

uint8_t nameToModifier(const std::string& name) {
    std::string s = name;
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);

    if (s == "CTRL" || s == "CONTROL" || s == "LCTRL") return MOD_LCTRL;
    if (s == "RCTRL") return MOD_RCTRL;
    if (s == "SHIFT" || s == "LSHIFT") return MOD_LSHIFT;
    if (s == "RSHIFT") return MOD_RSHIFT;
    if (s == "ALT" || s == "LALT") return MOD_LALT;
    if (s == "RALT" || s == "ALTGR") return MOD_RALT;
    if (s == "GUI" || s == "WINDOWS" || s == "WIN" || s == "COMMAND" || s == "SUPER") return MOD_LGUI;
    if (s == "RGUI") return MOD_RGUI;

    return MOD_NONE;
}

} // namespace hid
} // namespace cbdos
