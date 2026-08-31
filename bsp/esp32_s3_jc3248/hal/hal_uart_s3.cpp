#include "cbdos/uart.hpp"
#include "cbdos/gpio.hpp"
#include <Arduino.h>
#include <HardwareSerial.h>
#include <vector>

namespace cbdos {
namespace bsp {

class S3UartBackend : public cbdos::uart::IUartBackend {
public:
    S3UartBackend() : m_uart(1) {
        m_presets = {
            {"S3 Ext (TX:15 RX:16)", 15, 16},
            {"S3 Alt (TX:17 RX:18)", 17, 18}
        };
    }

    bool init(int txPin, int rxPin, uint32_t baudrate) override {
        if (m_isInitialized) {
            deinit();
        }

        m_currentTxPin = txPin;
        m_currentRxPin = rxPin;
        m_currentBaud = baudrate;

        m_uart.begin(baudrate, SERIAL_8N1, rxPin, txPin);
        m_isInitialized = true;
        return true;
    }

    void deinit() override {
        if (!m_isInitialized) return;
        m_uart.end();
        m_isInitialized = false;
    }

    bool isInitialized() const override {
        return m_isInitialized;
    }

    size_t available() override {
        if (!m_isInitialized) return 0;
        int avail = m_uart.available();
        return (avail > 0) ? (size_t)avail : 0;
    }

    size_t read(uint8_t* buffer, size_t maxLen) override {
        if (!m_isInitialized || !buffer || maxLen == 0) return 0;
        return m_uart.readBytes((char*)buffer, maxLen);
    }

    std::string readString(size_t maxLen) override {
        std::string res;
        if (!m_isInitialized || maxLen == 0) return res;
        size_t avail = available();
        if (avail == 0) return res;

        size_t toRead = (avail < maxLen) ? avail : maxLen;
        res.resize(toRead);
        size_t actual = read((uint8_t*)&res[0], toRead);
        res.resize(actual);
        return res;
    }

    size_t write(const uint8_t* data, size_t len) override {
        if (!m_isInitialized || !data || len == 0) return 0;
        return m_uart.write(data, len);
    }

    size_t writeString(const std::string& str) override {
        if (!m_isInitialized || str.empty()) return 0;
        return m_uart.print(str.c_str());
    }

    void flush() override {
        if (!m_isInitialized) return;
        m_uart.flush();
    }

    bool setBaudrate(uint32_t baudrate) override {
        if (!m_isInitialized) return false;
        return init(m_currentTxPin, m_currentRxPin, baudrate);
    }

    int getDefaultTxPin() const override { return 15; }
    int getDefaultRxPin() const override { return 16; }
    uint32_t getDefaultBaudrate() const override { return 115200; }
    const std::vector<cbdos::uart::UartPinPreset>& getPinPresets() const override { return m_presets; }

private:
    HardwareSerial m_uart;
    bool m_isInitialized = false;
    int m_currentTxPin = 15;
    int m_currentRxPin = 16;
    uint32_t m_currentBaud = 115200;
    std::vector<cbdos::uart::UartPinPreset> m_presets;
};

class S3GpioBackend : public cbdos::gpio::IGpioBackend {
public:
    bool setPinMode(int pin, cbdos::gpio::PinMode mode) override {
        if (!isPinAvailable(pin)) return false;

        switch (mode) {
            case cbdos::gpio::PinMode::Input:
                pinMode(pin, INPUT);
                break;
            case cbdos::gpio::PinMode::Output:
                pinMode(pin, OUTPUT);
                break;
            case cbdos::gpio::PinMode::InputPullUp:
                pinMode(pin, INPUT_PULLUP);
                break;
            case cbdos::gpio::PinMode::InputPullDown:
                pinMode(pin, INPUT_PULLDOWN);
                break;
        }
        return true;
    }

    bool digitalWrite(int pin, cbdos::gpio::PinLevel level) override {
        if (!isPinAvailable(pin)) return false;
        ::digitalWrite(pin, (level == cbdos::gpio::PinLevel::High) ? HIGH : LOW);
        return true;
    }

    cbdos::gpio::PinLevel digitalRead(int pin) override {
        if (!isPinAvailable(pin)) return cbdos::gpio::PinLevel::Low;
        int val = ::digitalRead(pin);
        return (val == HIGH) ? cbdos::gpio::PinLevel::High : cbdos::gpio::PinLevel::Low;
    }

    bool isPinAvailable(int pin) const override {
        if (pin < 0 || pin > 48) return false;
        // Reservados para LCD QSPI (45, 47, 21, 48, 40, 39, 4, 1), Touch (8, 4, 3), Audio (42, 2, 41)
        if (pin == 45 || pin == 47 || pin == 21 || pin == 48 || pin == 40 || pin == 39 || pin == 4 || pin == 1) return false;
        if (pin == 8 || pin == 3 || pin == 42 || pin == 2 || pin == 41) return false;
        return true;
    }
};

static S3UartBackend s_s3UartBackend;
static S3GpioBackend s_s3GpioBackend;

void initUartBackendS3() {
    cbdos::uart::setBackend(&s_s3UartBackend);
}

void initGpioBackendS3() {
    cbdos::gpio::setBackend(&s_s3GpioBackend);
}

} // namespace bsp
} // namespace cbdos
