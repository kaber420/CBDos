#pragma once

#include <cstdint>
#include <esp_err.h>
#include <esp_lcd_touch.h>

#include <driver/i2c_master.h>

#define BOARD_TOUCH_I2C_PORT    0
#define BOARD_TOUCH_SDA_GPIO    7
#define BOARD_TOUCH_SCL_GPIO    8
#define BOARD_TOUCH_RST_GPIO    3
#define BOARD_TOUCH_INT_GPIO    4

class TouchHAL {
public:
    static TouchHAL& getInstance() {
        static TouchHAL instance;
        return instance;
    }

    esp_err_t init(int h_res = 480, int v_res = 800);
    esp_lcd_touch_handle_t getTouchHandle() const { return touchHandle; }
    i2c_master_bus_handle_t getI2cBusHandle() const { return i2cBusHandle; }

    bool read(uint16_t* x, uint16_t* y, bool* pressed);

private:
    TouchHAL();
    ~TouchHAL();

    TouchHAL(const TouchHAL&) = delete;
    TouchHAL& operator=(const TouchHAL&) = delete;

    i2c_master_bus_handle_t i2cBusHandle = nullptr;
    esp_lcd_touch_handle_t touchHandle = nullptr;
    int width = 480;
    int height = 800;
    bool initialized = false;
};
