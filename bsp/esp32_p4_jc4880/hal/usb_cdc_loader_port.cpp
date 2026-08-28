#include "usb_cdc_loader_port.hpp"
#include <esp_loader_io.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <usb/usb_host.h>
#include <usb/cdc_acm_host.h>
#include <cstring>

static const char* TAG = "USB_LOADER_PORT";

static cdc_acm_dev_hdl_t s_cdc_dev = NULL;
static SemaphoreHandle_t s_rx_sem = NULL;
static uint8_t* s_rx_buf = NULL;
static size_t s_rx_data_len = 0;
static bool s_usb_host_installed = false;
static bool s_cdc_driver_installed = false;
static TaskHandle_t s_usb_host_task_hdl = NULL;
static bool s_usb_active = false;

static void usb_host_lib_task(void* arg) {
    while (s_usb_host_installed) {
        uint32_t event_flags;
        usb_host_lib_handle_events(pdMS_TO_TICKS(10), &event_flags);
    }
    vTaskDelete(NULL);
}

static bool cdc_rx_callback(const uint8_t *data, size_t data_len, void *user_arg) {
    if (s_rx_buf && data && data_len > 0) {
        size_t to_copy = (data_len > 1024) ? 1024 : data_len;
        memcpy(s_rx_buf, data, to_copy);
        s_rx_data_len = to_copy;
        if (s_rx_sem) {
            xSemaphoreGive(s_rx_sem);
        }
    }
    return true;
}

static void cdc_event_callback(const cdc_acm_host_dev_event_data_t *event, void *user_arg) {
    switch (event->type) {
        case CDC_ACM_HOST_ERROR:
            ESP_LOGE(TAG, "Error en dispositivo CDC-ACM (err=%d)", event->data.error);
            break;
        case CDC_ACM_HOST_DEVICE_DISCONNECTED:
            ESP_LOGW(TAG, "Dispositivo CDC-ACM desconectado");
            break;
        default:
            break;
    }
}

esp_loader_error_t loader_port_usb_cdc_init(uint32_t timeout_ms) {
    s_rx_data_len = 0;
    if (!s_rx_sem) {
        s_rx_sem = xSemaphoreCreateBinary();
    }
    if (!s_rx_buf) {
        s_rx_buf = (uint8_t*)malloc(1024);
    }

    if (!s_usb_host_installed) {
        const usb_host_config_t host_config = {
            .skip_phy_setup = false,
            .intr_flags = ESP_INTR_FLAG_LEVEL1,
        };
        esp_err_t err = usb_host_install(&host_config);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "Fallo al instalar USB Host: %s", esp_err_to_name(err));
            return ESP_LOADER_ERROR_FAIL;
        }
        s_usb_host_installed = true;
        xTaskCreatePinnedToCore(usb_host_lib_task, "usb_host_task", 4096, NULL, 5, &s_usb_host_task_hdl, 0);
    }

    if (!s_cdc_driver_installed) {
        const cdc_acm_host_driver_config_t driver_config = {
            .driver_task_stack_size = 4096,
            .driver_task_priority = 5,
            .xCoreID = 0,
            .new_dev_cb = NULL,
        };
        esp_err_t err = cdc_acm_host_install(&driver_config);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "Fallo al instalar CDC-ACM Host: %s", esp_err_to_name(err));
            return ESP_LOADER_ERROR_FAIL;
        }
        s_cdc_driver_installed = true;
    }

    ESP_LOGI(TAG, "Buscando dispositivo USB-Serial/JTAG en puerto OTG...");

    const cdc_acm_host_device_config_t dev_config = {
        .connection_timeout_ms = timeout_ms,
        .out_buffer_size = 2048,
        .in_buffer_size = 2048,
        .event_cb = cdc_event_callback,
        .data_cb = cdc_rx_callback,
        .user_arg = NULL,
    };

    esp_err_t err = cdc_acm_host_open(0x303A, 0x1001, 0, &dev_config, &s_cdc_dev);
    if (err != ESP_OK) {
        err = cdc_acm_host_open_vendor_specific(0x303A, 0x1001, 0, &dev_config, &s_cdc_dev);
    }

    if (err != ESP_OK || s_cdc_dev == NULL) {
        ESP_LOGE(TAG, "No se detectó el ESP32 conectado por USB (err: %s)", esp_err_to_name(err));
        return ESP_LOADER_ERROR_TIMEOUT;
    }

    s_usb_active = true;
    ESP_LOGI(TAG, "¡Dispositivo ESP32 USB-Serial/JTAG conectado exitosamente!");
    return ESP_LOADER_SUCCESS;
}

void loader_port_usb_cdc_deinit(void) {
    s_usb_active = false;
    if (s_cdc_dev) {
        cdc_acm_host_close(s_cdc_dev);
        s_cdc_dev = NULL;
    }
}

esp_loader_error_t loader_port_usb_cdc_reset_target(void) {
    if (!s_cdc_dev) return ESP_LOADER_ERROR_FAIL;

    ESP_LOGI(TAG, "Reiniciando ESP32 conectado por USB...");
    s_rx_data_len = 0;
    // DTR=false, RTS=true (Reset activo)
    cdc_acm_host_set_control_line_state(s_cdc_dev, false, true);
    vTaskDelay(pdMS_TO_TICKS(100));
    // DTR=false, RTS=false (Reset liberado)
    cdc_acm_host_set_control_line_state(s_cdc_dev, false, false);
    vTaskDelay(pdMS_TO_TICKS(100));

    return ESP_LOADER_SUCCESS;
}

// Sobrescritura de los callbacks para esp-serial-flasher
extern "C" {

esp_loader_error_t loader_port_write(const uint8_t *data, uint16_t size, uint32_t timeout) {
    if (s_usb_active && s_cdc_dev) {
        esp_err_t err = cdc_acm_host_data_tx_blocking(s_cdc_dev, data, size, timeout);
        if (err == ESP_ERR_TIMEOUT) return ESP_LOADER_ERROR_TIMEOUT;
        if (err != ESP_OK) return ESP_LOADER_ERROR_FAIL;
        return ESP_LOADER_SUCCESS;
    }
    return ESP_LOADER_ERROR_FAIL;
}

esp_loader_error_t loader_port_read(uint8_t *data, uint16_t size, uint32_t timeout) {
    if (s_usb_active && s_cdc_dev) {
        uint32_t bytes_read = 0;
        int64_t start_time = esp_timer_get_time();
        int64_t timeout_us = (int64_t)timeout * 1000;

        while (bytes_read < size) {
            if (s_rx_data_len > 0) {
                size_t available = s_rx_data_len;
                size_t needed = size - bytes_read;
                size_t to_copy = (available > needed) ? needed : available;

                memcpy(data + bytes_read, s_rx_buf, to_copy);
                bytes_read += to_copy;

                if (to_copy < available) {
                    memmove(s_rx_buf, s_rx_buf + to_copy, available - to_copy);
                    s_rx_data_len = available - to_copy;
                } else {
                    s_rx_data_len = 0;
                }
            }

            if (bytes_read >= size) break;

            int64_t elapsed = esp_timer_get_time() - start_time;
            if (elapsed >= timeout_us) {
                return ESP_LOADER_ERROR_TIMEOUT;
            }

            uint32_t remaining_ms = (uint32_t)((timeout_us - elapsed) / 1000);
            if (remaining_ms < 1) remaining_ms = 1;

            if (s_rx_sem) {
                xSemaphoreTake(s_rx_sem, pdMS_TO_TICKS(remaining_ms));
            } else {
                vTaskDelay(pdMS_TO_TICKS(2));
            }
        }
        return ESP_LOADER_SUCCESS;
    }
    return ESP_LOADER_ERROR_FAIL;
}

void loader_port_enter_bootloader(void) {
    if (s_usb_active && s_cdc_dev) {
        ESP_LOGI(TAG, "Enviando secuencia DTR/RTS por USB para entrar en ROM Bootloader...");
        s_rx_data_len = 0;

        // Secuencia oficial de Espressif para USB-Serial/JTAG:
        // 1. DTR=true, RTS=false (Preparar BOOT)
        cdc_acm_host_set_control_line_state(s_cdc_dev, true, false);
        vTaskDelay(pdMS_TO_TICKS(50));

        // 2. DTR=true, RTS=true (Activar Reset manteniendo BOOT)
        cdc_acm_host_set_control_line_state(s_cdc_dev, true, true);
        vTaskDelay(pdMS_TO_TICKS(100));

        // 3. DTR=false, RTS=false (Liberar Reset y BOOT para que el chip arranque en ROM Bootloader)
        cdc_acm_host_set_control_line_state(s_cdc_dev, false, false);
        vTaskDelay(pdMS_TO_TICKS(100));

        s_rx_data_len = 0;
    }
}

void loader_port_reset_target(void) {
    if (s_usb_active && s_cdc_dev) {
        loader_port_usb_cdc_reset_target();
    }
}

esp_loader_error_t loader_port_change_transmission_rate(uint32_t baudrate) {
    return ESP_LOADER_SUCCESS;
}

} // extern "C"
