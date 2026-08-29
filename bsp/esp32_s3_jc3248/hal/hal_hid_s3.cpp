#include "cbdos/hid.hpp"
#include "cbdos/system.hpp"
#include <Arduino.h>

#if defined(CONFIG_TINYUSB_ENABLED) || defined(ARDUINO_USB_MODE)
#include <USB.h>
#include <USBHIDKeyboard.h>
#include <USBHIDMouse.h>

namespace {

class Esp32S3HidDriver : public cbdos::hid::IHidDriver {
public:
    Esp32S3HidDriver() : m_ledState(0), m_initialized(false) {}

    bool init() {
        if (m_initialized) return true;
        ensureInitialized();
        return true;
    }

    bool isConnected() override {
        return m_initialized && USB;
    }

    bool isReady() override {
        return m_initialized;
    }

    void ensureInitialized() {
        if (!m_initialized) {
            m_keyboard.begin();
            m_mouse.begin();
            USB.begin();
            m_initialized = true;
        }
    }

    void sendReport(uint8_t modifiers, const uint8_t keycodes[6]) override {
        ensureInitialized();
        if (keycodes[0] != 0) {
            m_keyboard.pressRaw(keycodes[0]);
        } else {
            m_keyboard.releaseAll();
        }
    }

    void sendMouseReport(uint8_t buttons, int8_t x, int8_t y, int8_t wheel) override {
        ensureInitialized();
        if (x != 0 || y != 0 || wheel != 0) {
            m_mouse.move(x, y, wheel);
        }
        if (buttons & cbdos::hid::MOUSE_BTN_LEFT) m_mouse.press(MOUSE_LEFT);
        else m_mouse.release(MOUSE_LEFT);

        if (buttons & cbdos::hid::MOUSE_BTN_RIGHT) m_mouse.press(MOUSE_RIGHT);
        else m_mouse.release(MOUSE_RIGHT);

        if (buttons & cbdos::hid::MOUSE_BTN_MIDDLE) m_mouse.press(MOUSE_MIDDLE);
        else m_mouse.release(MOUSE_MIDDLE);
    }

    uint8_t getHostLedState() override {
        return m_ledState;
    }

    void updateLedState(uint8_t state) {
        m_ledState = state;
        cbdos::hid::onHostLedStateChanged(state);
    }

private:
    USBHIDKeyboard m_keyboard;
    USBHIDMouse m_mouse;
    uint8_t m_ledState;
    bool m_initialized;
};

static Esp32S3HidDriver s_s3HidDriver;

} // namespace

#else

namespace {

class DummyS3HidDriver : public cbdos::hid::IHidDriver {
public:
    bool isConnected() override { return false; }
    bool isReady() override { return false; }
    void sendReport(uint8_t modifiers, const uint8_t keycodes[6]) override { (void)modifiers; (void)keycodes; }
    void sendMouseReport(uint8_t buttons, int8_t x, int8_t y, int8_t wheel) override { (void)buttons; (void)x; (void)y; (void)wheel; }
    uint8_t getHostLedState() override { return 0; }
};

static DummyS3HidDriver s_s3HidDriver;

} // namespace

#endif

namespace cbdos {
namespace bsp {

void initHidDriverS3() {
#if defined(CONFIG_TINYUSB_ENABLED) || defined(ARDUINO_USB_MODE)
    s_s3HidDriver.init();
#endif
    cbdos::hid::registerDriver(&s_s3HidDriver);
}

} // namespace bsp
} // namespace cbdos
