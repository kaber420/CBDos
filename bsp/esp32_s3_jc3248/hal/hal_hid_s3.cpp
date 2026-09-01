#include "cbdos/hid.hpp"
#include "cbdos/system.hpp"
#include "security/kerberos/KerberosManager.hpp"
#include <Arduino.h>

#if defined(CONFIG_TINYUSB_ENABLED) || defined(ARDUINO_USB_MODE)
#include <USB.h>
#include <USBHID.h>

namespace {

// Descriptor de reporte USB FIDO2 / CTAPHID oficial de Poseidon (Sin Report ID)
static const uint8_t s_fidoReportDesc[] = {
    0x06, 0xD0, 0xF1, // Usage Page (FIDO Alliance 0xF1D0)
    0x09, 0x01,       // Usage (CTAPHID)
    0xA1, 0x01,       // Collection (Application)
    0x09, 0x20,       //   Usage (Input Report Data)
    0x15, 0x00,       //   Logical Min 0
    0x26, 0xFF, 0x00, //   Logical Max 255
    0x75, 0x08,       //   Report Size 8
    0x95, 0x40,       //   Report Count 64
    0x81, 0x02,       //   Input (Data,Var,Abs)
    0x09, 0x21,       //   Usage (Output Report Data)
    0x15, 0x00, 0x26, 0xFF, 0x00,
    0x75, 0x08, 0x95, 0x40,
    0x91, 0x02,       //   Output (Data,Var,Abs)
    0xC0              // End Collection
};

#define KH_QN 16
static uint8_t s_q[KH_QN][64];
static volatile uint8_t s_qhead = 0, s_qtail = 0;

class USBHIDFido : public USBHIDDevice {
public:
    USBHIDFido() {
        static bool s_added = false;
        if (!s_added) {
            s_added = true;
            USBHID::addDevice(this, sizeof(s_fidoReportDesc));
        }
    }

    void begin() {
        m_hid.begin();
    }

    uint16_t _onGetDescriptor(uint8_t* dst) override {
        memcpy(dst, s_fidoReportDesc, sizeof(s_fidoReportDesc));
        return sizeof(s_fidoReportDesc);
    }

    void _onOutput(uint8_t report_id, const uint8_t* buffer, uint16_t len) override {
        if (buffer && len >= 64) {
            uint8_t next = (uint8_t)((s_qhead + 1) % KH_QN);
            if (next != s_qtail) {
                memcpy(s_q[s_qhead], buffer, 64);
                s_qhead = next;
            }
        }
    }

    void _onSetFeature(uint8_t report_id, const uint8_t* buffer, uint16_t len) override {
        if (buffer && len >= 64) {
            uint8_t next = (uint8_t)((s_qhead + 1) % KH_QN);
            if (next != s_qtail) {
                memcpy(s_q[s_qhead], buffer, 64);
                s_qhead = next;
            }
        }
    }

    bool sendReport(const uint8_t report[64]) {
        for (int i = 0; i < 200; i++) {
            if (tud_hid_n_ready(0) && tud_hid_n_report(0, 0, report, 64)) {
                return true;
            }
            delay(1);
        }
        return false;
    }

private:
    USBHID m_hid;
};

static USBHIDFido s_fidoDevice;
} // namespace

void cbdos_hid_s3_poll() {
    while (s_qtail != s_qhead) {
        uint8_t* pkt = s_q[s_qtail];
        s_qtail = (uint8_t)((s_qtail + 1) % KH_QN);
        cbdos::security::KerberosManager::instance().handleIncomingUsbReport(pkt);
    }
}

namespace {

class Esp32S3HidDriver : public cbdos::hid::IHidDriver {
public:
    Esp32S3HidDriver() : m_ledState(0), m_enabled(false) {}

    bool init() {
        s_fidoDevice.begin();

        // Conectar Kerberos con la interfaz USB FIDO2
        cbdos::security::KerberosManager::instance().init();
        cbdos::security::KerberosManager::instance().setUsbSendCallback([](const uint8_t packet[64]) {
            s_fidoDevice.sendReport(packet);
        });

        USB.productName("KERBEROS FIDO2 Security Key");
        USB.manufacturerName("CBDos / Poseidon");
        USB.begin();
        m_enabled = true;
        return true;
    }

    bool enable() override {
        m_enabled = true;
        return true;
    }

    bool disable() override {
        m_enabled = false;
        return true;
    }

    bool isEnabled() const override {
        return m_enabled;
    }

    bool isConnected() override {
        return m_enabled && (bool)USB;
    }

    bool isReady() override {
        return m_enabled;
    }

    void sendReport(uint8_t modifiers, const uint8_t keycodes[6]) override {
        (void)modifiers;
        (void)keycodes;
    }

    void sendMouseReport(uint8_t buttons, int8_t x, int8_t y, int8_t wheel) override {
        (void)buttons;
        (void)x;
        (void)y;
        (void)wheel;
    }

    uint8_t getHostLedState() override {
        return m_ledState;
    }

    void updateLedState(uint8_t state) {
        m_ledState = state;
        cbdos::hid::onHostLedStateChanged(state);
    }

private:
    uint8_t m_ledState;
    bool m_enabled;
};

static Esp32S3HidDriver s_s3HidDriver;

} // namespace

#else

namespace {

class DummyS3HidDriver : public cbdos::hid::IHidDriver {
public:
    bool enable() override { return false; }
    bool disable() override { return false; }
    bool isEnabled() const override { return false; }
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

