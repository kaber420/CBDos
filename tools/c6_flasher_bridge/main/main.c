#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "driver/usb_serial_jtag.h"

#define P4_TX_PIN 32 // Conectado a C6_U0RXD
#define P4_RX_PIN 28 // Conectado a C6_U0TXD
#define C6_RST_PIN 54 // Reset C6 (Active LOW)
#define UART_PORT UART_NUM_1
#define BUF_SIZE 2048
#define BUF_SIZE 16384

static void reset_c6(void) {
    gpio_set_direction((gpio_num_t)C6_RST_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)C6_RST_PIN, 0); // Reset LOW
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level((gpio_num_t)C6_RST_PIN, 1); // Release HIGH
    vTaskDelay(pdMS_TO_TICKS(100));
}

static void usb_to_uart_task(void* pvParameters) {
    uint8_t *buf = (uint8_t *)malloc(BUF_SIZE);
    while (1) {
        int len = usb_serial_jtag_read_bytes(buf, BUF_SIZE, portMAX_DELAY);
        if (len > 0) {
            uart_write_bytes(UART_PORT, (const char*)buf, len);
        }
    }
}


static void uart_to_usb_task(void* pvParameters) {
    uint8_t *buf = (uint8_t *)malloc(BUF_SIZE);
    while (1) {
        int len = uart_read_bytes(UART_PORT, buf, BUF_SIZE, pdMS_TO_TICKS(5));
        if (len > 0) {
            usb_serial_jtag_write_bytes(buf, len, portMAX_DELAY);
        }
    }
}


void app_main(void) {
    // 1. Instalar driver USB-Serial-JTAG con búferes ampliados a 16KB
    usb_serial_jtag_driver_config_t usb_cfg = {
        .tx_buffer_size = 16384,
        .rx_buffer_size = 16384,
    };
    usb_serial_jtag_driver_install(&usb_cfg);

    // 2. Configurar GPIO 54 (Reset C6)
    gpio_set_direction((gpio_num_t)C6_RST_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)C6_RST_PIN, 1);

    // 3. Configurar UART1 hacia el ESP32-C6 a 115200 baudios con 16KB de búfer
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_param_config(UART_PORT, &uart_config);
    uart_set_pin(UART_PORT, P4_TX_PIN, P4_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    gpio_set_pull_mode((gpio_num_t)P4_RX_PIN, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode((gpio_num_t)P4_TX_PIN, GPIO_PULLUP_ONLY);
    uart_driver_install(UART_PORT, 16384, 16384, 0, NULL, 0);

    // 4. Reset inicial del C6
    reset_c6();

    // 5. Crear tareas dedicadas en Core 0 y Core 1
    xTaskCreatePinnedToCore(usb_to_uart_task, "usb2uart", 8192, NULL, 15, NULL, 0);
    xTaskCreatePinnedToCore(uart_to_usb_task, "uart2usb", 8192, NULL, 15, NULL, 1);
}
