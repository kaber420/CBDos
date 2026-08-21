#include "LVGL_Port.h"
#include "DisplayHAL.h"
#include "TouchHAL.h"
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <esp_cache.h>
#include <esp_timer.h>

static const char *TAG = "LVGL_Port";

static uint32_t lvgl_tick_cb(void) {
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

LVGL_Port::LVGL_Port() {}
LVGL_Port::~LVGL_Port() {
    running = false;
    if (lvglMux) {
        vSemaphoreDelete(lvglMux);
        lvglMux = nullptr;
    }
}

bool LVGL_Port::lock(uint32_t timeout_ms) {
    if (!lvglMux) return true;
    TickType_t ticks = (timeout_ms == portMAX_DELAY) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTake(lvglMux, ticks) == pdTRUE;
}

void LVGL_Port::unlock() {
    if (lvglMux) {
        xSemaphoreGive(lvglMux);
    }
}

void LVGL_Port::flushCallback(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    esp_lcd_panel_handle_t panel_handle = DisplayHAL::getInstance().getPanelHandle();
    if (panel_handle) {
        int w = DisplayHAL::getInstance().getWidth();
        int h = DisplayHAL::getInstance().getHeight();
        size_t fb_bytes = (size_t)w * h * sizeof(uint16_t);

        static int flush_count = 0;
        if (flush_count < 10) {
            ESP_LOGI(TAG, "flushCallback #%d (area: %d,%d a %d,%d, buf: %p)", flush_count, area->x1, area->y1, area->x2, area->y2, px_map);
            flush_count++;
        }

        // Asegurar coherencia de memoria caché antes de que el motor DPI transmita
        esp_cache_msync(px_map, fb_bytes, ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);

        // Notificar al controlador DPI para intercambiar y escanear este fotograma
        esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, w, h, px_map);
    }
    lv_display_flush_ready(disp);
}

void LVGL_Port::touchReadCallback(lv_indev_t* indev, lv_indev_data_t* data) {
    uint16_t x = 0;
    uint16_t y = 0;
    bool pressed = false;

    if (TouchHAL::getInstance().read(&x, &y, &pressed)) {
        if (pressed) {
            data->point.x = x;
            data->point.y = y;
            data->state = LV_INDEV_STATE_PRESSED;
        } else {
            data->state = LV_INDEV_STATE_RELEASED;
        }
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void LVGL_Port::lvglTask(void* arg) {
    LVGL_Port* self = static_cast<LVGL_Port*>(arg);
    ESP_LOGI(TAG, "Tarea LVGL 9 iniciada en Core %d", xPortGetCoreID());

    while (self->running) {
        uint32_t task_delay_ms = 5;
        if (self->lock(10)) {
            task_delay_ms = lv_timer_handler();
            self->unlock();
        }
        if (task_delay_ms > 50) task_delay_ms = 50;
        if (task_delay_ms < 1) task_delay_ms = 1;
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
    vTaskDelete(NULL);
}

esp_err_t LVGL_Port::init(int h_res, int v_res) {
    width = h_res;
    height = v_res;

    ESP_LOGI(TAG, "=== Inicializando LVGL 9.5 Port para ESP32-P4 ===");

    // 1. Crear Mutex de sincronización
    lvglMux = xSemaphoreCreateRecursiveMutex();
    if (!lvglMux) {
        ESP_LOGE(TAG, "No se pudo crear el mutex de LVGL");
        return ESP_ERR_NO_MEM;
    }

    // 2. Inicializar el Core de LVGL 9 y Tick Timer
    lv_init();
    lv_tick_set_cb(lvgl_tick_cb);

    // 3. Vincular los búferes de fotogramas del controlador DPI
    buf1 = DisplayHAL::getInstance().getFrameBuffer(0);
    buf2 = DisplayHAL::getInstance().getFrameBuffer(1);
    size_t buffer_size_bytes = width * height * sizeof(lv_color16_t);

    if (!buf1 || !buf2) {
        ESP_LOGW(TAG, "Asignando búferes dedicados en PSRAM como respaldo...");
        buf1 = heap_caps_aligned_alloc(64, buffer_size_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        buf2 = heap_caps_aligned_alloc(64, buffer_size_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }

    if (!buf1 || !buf2) {
        ESP_LOGE(TAG, "Error: No hay suficiente PSRAM para buffers de pantalla");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Búferes de LVGL vinculados: buf1=%p, buf2=%p (%u KB c/u)",
             buf1, buf2, (unsigned)(buffer_size_bytes / 1024));

    // 4. Crear Display en LVGL 9 con renderizado FULL
    display = lv_display_create(width, height);
    if (!display) {
        ESP_LOGE(TAG, "Error creando display LVGL 9");
        return ESP_FAIL;
    }

    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, buf1, buf2, buffer_size_bytes, LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display, flushCallback);

    // 5. Crear e Inicializar Dispositivo de Entrada Táctil
    indev = lv_indev_create();
    if (indev) {
        lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(indev, touchReadCallback);
        lv_indev_set_display(indev, display);
    }

    // 6. Lanzar Tarea FreeRTOS para el loop de LVGL
    running = true;
    BaseType_t res = xTaskCreatePinnedToCore(
        lvglTask,
        "lvgl_task",
        32 * 1024,
        this,
        5,
        &taskHandle,
        1 // Pin to Core 1
    );

    if (res != pdPASS) {
        ESP_LOGE(TAG, "Fallo al crear tarea FreeRTOS de LVGL");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "LVGL 9.5 Port inicializado exitosamente!");
    return ESP_OK;
}
