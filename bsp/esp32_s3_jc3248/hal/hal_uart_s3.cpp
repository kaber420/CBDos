#include "cbdos/uart.hpp"
#include <Arduino.h>
#include <HardwareSerial.h>
#include <vector>

namespace cbdos {
namespace uart {

static HardwareSerial s_uart(1);
static bool s_isInitialized = false;
static int s_currentTxPin = 15;
static int s_currentRxPin = 16;
static uint32_t s_currentBaud = 115200;

static const std::vector<UartPinPreset> s_presets = {
    {"S3 Ext (TX:15 RX:16)", 15, 16},
    {"S3 Alt (TX:17 RX:18)", 17, 18}
};

bool init(int txPin, int rxPin, uint32_t baudrate) {
    if (s_isInitialized) {
        deinit();
    }

    s_currentTxPin = txPin;
    s_currentRxPin = rxPin;
    s_currentBaud = baudrate;

    s_uart.begin(baudrate, SERIAL_8N1, rxPin, txPin);
    s_isInitialized = true;
    return true;
}

void deinit() {
    if (!s_isInitialized) return;
    s_uart.end();
    s_isInitialized = false;
}

bool isInitialized() {
    return s_isInitialized;
}

size_t available() {
    if (!s_isInitialized) return 0;
    int avail = s_uart.available();
    return (avail > 0) ? (size_t)avail : 0;
}

size_t read(uint8_t* buffer, size_t maxLen) {
    if (!s_isInitialized || !buffer || maxLen == 0) return 0;
    return s_uart.readBytes((char*)buffer, maxLen);
}

std::string readString(size_t maxLen) {
    std::string res;
    if (!s_isInitialized || maxLen == 0) return res;
    size_t avail = available();
    if (avail == 0) return res;

    size_t toRead = (avail < maxLen) ? avail : maxLen;
    res.resize(toRead);
    size_t actual = read((uint8_t*)&res[0], toRead);
    res.resize(actual);
    return res;
}

size_t write(const uint8_t* data, size_t len) {
    if (!s_isInitialized || !data || len == 0) return 0;
    return s_uart.write(data, len);
}

size_t writeString(const std::string& str) {
    if (!s_isInitialized || str.empty()) return 0;
    return s_uart.print(str.c_str());
}

void flush() {
    if (!s_isInitialized) return;
    s_uart.flush();
}

bool setBaudrate(uint32_t baudrate) {
    if (!s_isInitialized) return false;
    return init(s_currentTxPin, s_currentRxPin, baudrate);
}

int getDefaultTxPin() {
    return 15;
}

int getDefaultRxPin() {
    return 16;
}

uint32_t getDefaultBaudrate() {
    return 115200;
}

const std::vector<UartPinPreset>& getPinPresets() {
    return s_presets;
}

} // namespace uart
} // namespace cbdos
