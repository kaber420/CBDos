#include "cbdos/uart.hpp"
#include <driver/uart.h>
#include <driver/gpio.h>
#include <esp_log.h>
#include <cstring>
#include <vector>

static const char* TAG = "HAL_UART_P4";
#define UART_HAL_PORT UART_NUM_1
#define UART_RX_BUF_SIZE 2048
#define UART_TX_BUF_SIZE 1024

namespace cbdos {
namespace uart {

static bool s_isInitialized = false;
static int s_currentTxPin = 32;
static int s_currentRxPin = 28;
static uint32_t s_currentBaud = 115200;

static const std::vector<UartPinPreset> s_presets = {
    {"JP1 (TX:32 RX:28)", 32, 28},
    {"JP1 Alt (TX:50 RX:49)", 50, 49},
    {"JP1 (TX:52 RX:51)", 52, 51}
};

bool init(int txPin, int rxPin, uint32_t baudrate) {
    if (s_isInitialized) {
        deinit();
    }

    s_currentTxPin = txPin;
    s_currentRxPin = rxPin;
    s_currentBaud = baudrate;

    uart_config_t uart_config = {
        .baud_rate = (int)baudrate,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t err = uart_param_config(UART_HAL_PORT, &uart_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_param_config fallo: %s", esp_err_to_name(err));
        return false;
    }

    err = uart_set_pin(UART_HAL_PORT, txPin, rxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_set_pin fallo: %s", esp_err_to_name(err));
        return false;
    }

    err = uart_driver_install(UART_HAL_PORT, UART_RX_BUF_SIZE, UART_TX_BUF_SIZE, 0, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_driver_install fallo: %s", esp_err_to_name(err));
        return false;
    }

    s_isInitialized = true;
    ESP_LOGI(TAG, "UART1 inicializada en TX:%d, RX:%d a %u bps", txPin, rxPin, (unsigned)baudrate);
    return true;
}

void deinit() {
    if (!s_isInitialized) return;
    uart_driver_delete(UART_HAL_PORT);
    s_isInitialized = false;
    ESP_LOGI(TAG, "UART1 liberada");
}

bool isInitialized() {
    return s_isInitialized;
}

size_t available() {
    if (!s_isInitialized) return 0;
    size_t length = 0;
    esp_err_t err = uart_get_buffered_data_len(UART_HAL_PORT, &length);
    if (err == ESP_OK) {
        return length;
    }
    return 0;
}

size_t read(uint8_t* buffer, size_t maxLen) {
    if (!s_isInitialized || !buffer || maxLen == 0) return 0;
    int len = uart_read_bytes(UART_HAL_PORT, buffer, maxLen, 0);
    return (len > 0) ? (size_t)len : 0;
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
    int written = uart_write_bytes(UART_HAL_PORT, (const char*)data, len);
    return (written > 0) ? (size_t)written : 0;
}

size_t writeString(const std::string& str) {
    return write((const uint8_t*)str.data(), str.size());
}

void flush() {
    if (!s_isInitialized) return;
    uart_flush(UART_HAL_PORT);
}

bool setBaudrate(uint32_t baudrate) {
    if (!s_isInitialized) return false;
    s_currentBaud = baudrate;
    esp_err_t err = uart_set_baudrate(UART_HAL_PORT, baudrate);
    return (err == ESP_OK);
}

int getDefaultTxPin() {
    return 32;
}

int getDefaultRxPin() {
    return 28;
}

uint32_t getDefaultBaudrate() {
    return 115200;
}

const std::vector<UartPinPreset>& getPinPresets() {
    return s_presets;
}

} // namespace uart
} // namespace cbdos
