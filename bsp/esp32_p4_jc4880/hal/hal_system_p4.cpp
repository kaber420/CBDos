#include "cbdos/system.hpp"
#include <driver/temperature_sensor.h>
#include <esp_timer.h>
#include <esp_rom_sys.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <esp_log.h>
#include <cstdarg>
#include <cstdio>

namespace cbdos {
namespace system {

uint32_t getTimeMs() {
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

uint64_t getTimeUs() {
    return (uint64_t)esp_timer_get_time();
}

void sleepMs(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

void sleepUs(uint32_t us) {
    esp_rom_delay_us(us);
}

void yieldTask() {
    taskYIELD();
}

size_t getFreeHeap() {
    return heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
}

size_t getTotalHeap() {
    return heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
}



static temperature_sensor_handle_t s_temp_sensor = nullptr;
static bool s_temp_sensor_init = false;

static void init_temp_sensor_if_needed() {
    if (!s_temp_sensor_init) {
        temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 80);
        if (temperature_sensor_install(&temp_sensor_config, &s_temp_sensor) == ESP_OK) {
            if (temperature_sensor_enable(s_temp_sensor) == ESP_OK) {
                s_temp_sensor_init = true;
            }
        }
    }
}

size_t getFreePsram() {
    return heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
}

size_t getTotalPsram() {
    return heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
}

float getCpuTemperature() {
    init_temp_sensor_if_needed();
    if (s_temp_sensor_init && s_temp_sensor) {
        float tsens_out = 0.0f;
        if (temperature_sensor_get_celsius(s_temp_sensor, &tsens_out) == ESP_OK) {
            return tsens_out;
        }
    }
    return 0.0f;
}

void restart() {
    esp_restart();
}

void log(LogLevel level, const char* tag, const char* format, ...) {
    esp_log_level_t esp_level = ESP_LOG_INFO;
    switch (level) {
        case LogLevel::Debug: esp_level = ESP_LOG_DEBUG; break;
        case LogLevel::Info:  esp_level = ESP_LOG_INFO;  break;
        case LogLevel::Warn:  esp_level = ESP_LOG_WARN;  break;
        case LogLevel::Error: esp_level = ESP_LOG_ERROR; break;
    }
    
    va_list args;
    va_start(args, format);
    esp_log_writev(esp_level, tag ? tag : "CBDos", format, args);
    va_end(args);
    printf("\n");
}

} // namespace system
} // namespace cbdos
