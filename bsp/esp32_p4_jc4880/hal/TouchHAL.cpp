#include "TouchHAL.h"
#include <esp_log.h>
#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <esp_lcd_touch_gt911.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *TAG = "TouchHAL";

TouchHAL::TouchHAL() {}
TouchHAL::~TouchHAL() {}

esp_err_t TouchHAL::init(int h_res, int v_res) {
    if (initialized) return ESP_OK;

    width = h_res;
    height = v_res;

    ESP_LOGI(TAG, "=== Inicializando TouchHAL (Goodix GT911 I2C SDA=%d SCL=%d RST=%d INT=%d) ===",
             BOARD_TOUCH_SDA_GPIO, BOARD_TOUCH_SCL_GPIO, BOARD_TOUCH_RST_GPIO, BOARD_TOUCH_INT_GPIO);

    // 1. Configurar Bus I2C Maestro
    i2c_master_bus_config_t i2c_bus_config = {
        .i2c_port = (i2c_port_num_t)BOARD_TOUCH_I2C_PORT,
        .sda_io_num = (gpio_num_t)BOARD_TOUCH_SDA_GPIO,
        .scl_io_num = (gpio_num_t)BOARD_TOUCH_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags = {
            .enable_internal_pullup = 1
        }
    };
    i2c_master_bus_handle_t i2c_bus_handle = nullptr;
    esp_err_t ret = i2c_new_master_bus(&i2c_bus_config, &i2c_bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fallo al inicializar bus I2C maestro: %s", esp_err_to_name(ret));
        return ret;
    }

    // 2. Pulso de Reset por Hardware para despertar el GT911
    if (BOARD_TOUCH_RST_GPIO >= 0) {
        gpio_config_t rst_conf = {
            .pin_bit_mask = (1ULL << BOARD_TOUCH_RST_GPIO),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&rst_conf);
        gpio_set_level((gpio_num_t)BOARD_TOUCH_RST_GPIO, 0);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level((gpio_num_t)BOARD_TOUCH_RST_GPIO, 1);
        vTaskDelay(pdMS_TO_TICKS(50));
        ESP_LOGI(TAG, "Pulso de Reset GT911 completado en GPIO %d", BOARD_TOUCH_RST_GPIO);
    }

    // 3. Sondear dirección I2C del GT911 (0x5D o 0x14)
    uint8_t tp_addr = ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS; // 0x5D
    if (i2c_master_probe(i2c_bus_handle, ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS, 100) == ESP_OK) {
        tp_addr = ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS;
        ESP_LOGI(TAG, "GT911 detectado en dirección 0x5D");
    } else if (i2c_master_probe(i2c_bus_handle, ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP, 100) == ESP_OK) {
        tp_addr = ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP;
        ESP_LOGI(TAG, "GT911 detectado en dirección backup 0x14");
    } else {
        ESP_LOGW(TAG, "Aviso: No se recibió ACK del GT911 en 0x5D ni en 0x14 (intentando 0x5D)");
    }

    // 4. Configurar Panel IO I2C para GT911
    esp_lcd_panel_io_handle_t tp_io_handle = nullptr;
    esp_lcd_panel_io_i2c_config_t tp_io_config = {};
    tp_io_config.dev_addr = tp_addr;
    tp_io_config.scl_speed_hz = 400 * 1000;
    tp_io_config.control_phase_bytes = 1;
    tp_io_config.dc_bit_offset = 0;
    tp_io_config.lcd_cmd_bits = 16;
    tp_io_config.flags.disable_control_phase = 1;
    ret = esp_lcd_new_panel_io_i2c(i2c_bus_handle, &tp_io_config, &tp_io_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fallo al crear panel IO I2C: %s", esp_err_to_name(ret));
        return ret;
    }

    // 5. Inicializar controlador Goodix GT911
    esp_lcd_touch_config_t tp_cfg = {
        .x_max = (uint16_t)width,
        .y_max = (uint16_t)height,
        .rst_gpio_num = (gpio_num_t)BOARD_TOUCH_RST_GPIO,
        .int_gpio_num = (gpio_num_t)BOARD_TOUCH_INT_GPIO,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };

    ret = esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, &touchHandle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fallo al inicializar touch GT911: %s", esp_err_to_name(ret));
        return ret;
    }

    initialized = true;
    ESP_LOGI(TAG, "TouchHAL GT911 inicializado correctamente (%dx%d)", width, height);
    return ESP_OK;
}

bool TouchHAL::read(uint16_t* x, uint16_t* y, bool* pressed) {
    if (!touchHandle || !x || !y || !pressed) return false;

    esp_lcd_touch_read_data(touchHandle);

    uint16_t touch_x[1] = {0};
    uint16_t touch_y[1] = {0};
    uint16_t touch_strength[1] = {0};
    uint8_t touch_cnt = 0;

    bool is_pressed = esp_lcd_touch_get_coordinates(touchHandle, touch_x, touch_y, touch_strength, &touch_cnt, 1);
    *pressed = is_pressed && (touch_cnt > 0);
    if (*pressed) {
        *x = touch_x[0];
        *y = touch_y[0];
    }
    return true;
}
