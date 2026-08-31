#include "cbdos/uart.hpp"
#include "cbdos/gpio.hpp"
#include <driver/uart.h>
#include <driver/gpio.h>
#include <esp_log.h>
#include <cstring>
#include <vector>

static const char* TAG_UART = "HAL_UART_P4";
static const char* TAG_GPIO = "HAL_GPIO_P4";

#define UART_HAL_PORT UART_NUM_1
#define UART_RX_BUF_SIZE 2048
#define UART_TX_BUF_SIZE 1024

namespace cbdos {
namespace bsp {

class P4UartBackend : public cbdos::uart::IUartBackend {
public:
    P4UartBackend() {
        m_presets = {
            {"JP1 (TX:32 RX:28)", 32, 28},
            {"JP1 Alt 1 (TX:50 RX:49)", 50, 49},
            {"JP1 Alt 2 (TX:52 RX:51)", 52, 51},
            {"MX 1.25 UART0 (TX:38 RX:37)", 38, 37}
        };
    }

    bool init(int txPin, int rxPin, uint32_t baudrate) override {
        if (m_isInitialized) {
            deinit();
        }

        m_currentTxPin = txPin;
        m_currentRxPin = rxPin;
        m_currentBaud = baudrate;

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
            ESP_LOGE(TAG_UART, "uart_param_config fallo: %s", esp_err_to_name(err));
            return false;
        }

        err = uart_set_pin(UART_HAL_PORT, txPin, rxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
        if (err != ESP_OK) {
            ESP_LOGE(TAG_UART, "uart_set_pin fallo: %s", esp_err_to_name(err));
            return false;
        }

        err = uart_driver_install(UART_HAL_PORT, UART_RX_BUF_SIZE, UART_TX_BUF_SIZE, 0, NULL, 0);
        if (err != ESP_OK) {
            ESP_LOGE(TAG_UART, "uart_driver_install fallo: %s", esp_err_to_name(err));
            return false;
        }

        m_isInitialized = true;
        ESP_LOGI(TAG_UART, "UART1 inicializada en TX:%d, RX:%d a %u bps", txPin, rxPin, (unsigned)baudrate);
        return true;
    }

    void deinit() override {
        if (!m_isInitialized) return;
        uart_driver_delete(UART_HAL_PORT);
        m_isInitialized = false;
        ESP_LOGI(TAG_UART, "UART1 liberada (Pines disponibles para GPIO/Mochilas)");
    }

    bool isInitialized() const override {
        return m_isInitialized;
    }

    size_t available() override {
        if (!m_isInitialized) return 0;
        size_t length = 0;
        esp_err_t err = uart_get_buffered_data_len(UART_HAL_PORT, &length);
        return (err == ESP_OK) ? length : 0;
    }

    size_t read(uint8_t* buffer, size_t maxLen) override {
        if (!m_isInitialized || !buffer || maxLen == 0) return 0;
        int len = uart_read_bytes(UART_HAL_PORT, buffer, maxLen, 0);
        return (len > 0) ? (size_t)len : 0;
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
        int written = uart_write_bytes(UART_HAL_PORT, (const char*)data, len);
        return (written > 0) ? (size_t)written : 0;
    }

    size_t writeString(const std::string& str) override {
        return write((const uint8_t*)str.data(), str.size());
    }

    void flush() override {
        if (!m_isInitialized) return;
        uart_flush(UART_HAL_PORT);
    }

    bool setBaudrate(uint32_t baudrate) override {
        if (!m_isInitialized) return false;
        m_currentBaud = baudrate;
        esp_err_t err = uart_set_baudrate(UART_HAL_PORT, baudrate);
        return (err == ESP_OK);
    }

    int getDefaultTxPin() const override { return 32; }
    int getDefaultRxPin() const override { return 28; }
    uint32_t getDefaultBaudrate() const override { return 115200; }
    const std::vector<cbdos::uart::UartPinPreset>& getPinPresets() const override { return m_presets; }

private:
    bool m_isInitialized = false;
    int m_currentTxPin = 32;
    int m_currentRxPin = 28;
    uint32_t m_currentBaud = 115200;
    std::vector<cbdos::uart::UartPinPreset> m_presets;
};

class P4GpioBackend : public cbdos::gpio::IGpioBackend {
public:
    bool setPinMode(int pin, cbdos::gpio::PinMode mode) override {
        if (!isPinAvailable(pin)) {
            ESP_LOGW(TAG_GPIO, "GPIO %d protegido o reservado por el sistema", pin);
            return false;
        }

        gpio_config_t io_conf = {};
        io_conf.pin_bit_mask = (1ULL << pin);
        io_conf.intr_type = GPIO_INTR_DISABLE;

        switch (mode) {
            case cbdos::gpio::PinMode::Input:
                io_conf.mode = GPIO_MODE_INPUT;
                io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
                io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
                break;
            case cbdos::gpio::PinMode::Output:
                io_conf.mode = GPIO_MODE_OUTPUT;
                io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
                io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
                break;
            case cbdos::gpio::PinMode::InputPullUp:
                io_conf.mode = GPIO_MODE_INPUT;
                io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
                io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
                break;
            case cbdos::gpio::PinMode::InputPullDown:
                io_conf.mode = GPIO_MODE_INPUT;
                io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
                io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
                break;
        }

        return gpio_config(&io_conf) == ESP_OK;
    }

    bool digitalWrite(int pin, cbdos::gpio::PinLevel level) override {
        if (!isPinAvailable(pin)) return false;
        return gpio_set_level((gpio_num_t)pin, (uint32_t)level) == ESP_OK;
    }

    cbdos::gpio::PinLevel digitalRead(int pin) override {
        if (!isPinAvailable(pin)) return cbdos::gpio::PinLevel::Low;
        int val = gpio_get_level((gpio_num_t)pin);
        return (val > 0) ? cbdos::gpio::PinLevel::High : cbdos::gpio::PinLevel::Low;
    }

    bool isPinAvailable(int pin) const override {
        if (pin < 0 || pin > 54) return false;
        // Pines reservados para periféricos clave del sistema (Pantalla, SDMMC, I2C interno, Audio I2S)
        // 5: LCD RST, 23: Backlight, 7: I2C SDA, 8: I2C SCL, 3: Touch RST, 4: Touch INT
        // 13, 12, 10, 9, 48, 11: Audio I2S / ES8311
        // 39, 40, 41, 42, 43, 44: MicroSD Slot 0 SDMMC
        // 18, 19, 14, 15, 16, 17, 54: SDIO C6
        if (pin == 5 || pin == 23 || pin == 7 || pin == 8 || pin == 3 || pin == 4) return false;
        if (pin == 13 || pin == 12 || pin == 10 || pin == 9 || pin == 48 || pin == 11) return false;
        if (pin >= 39 && pin <= 44) return false;
        if (pin == 18 || pin == 19 || (pin >= 14 && pin <= 17) || pin == 54) return false;

        // Todos los pines de cabecera JP1 y conectores MX son permitidos:
        // 52, 51, 50, 49, 35, 34, 32, 28, 38, 37, 33, 31, 30, 29, etc.
        return true;
    }
};

static P4UartBackend s_p4UartBackend;
static P4GpioBackend s_p4GpioBackend;

void initUartBackendP4() {
    cbdos::uart::setBackend(&s_p4UartBackend);
    ESP_LOGI(TAG_UART, "Backend UART para ESP32-P4 registrado e inyectado");
}

void initGpioBackendP4() {
    cbdos::gpio::setBackend(&s_p4GpioBackend);
    ESP_LOGI(TAG_GPIO, "Backend GPIO para ESP32-P4 registrado e inyectado");
}

} // namespace bsp
} // namespace cbdos
