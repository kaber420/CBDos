#include "cbdos/system.hpp"
#include "cbdos/display.hpp"
#include "cbdos/input.hpp"
#include "cbdos/storage.hpp"
#include "cbdos/audio.hpp"
#include "cbdos/ui.hpp"
#include "LVGL_Port.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "CBDos_Main";

static void uiUpdateTask(void* pvParameters) {
    while (true) {
        if (LVGL_Port::getInstance().lock(pdMS_TO_TICKS(100))) {
            cbdos::ui::update();
            LVGL_Port::getInstance().unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

extern "C" void app_main(void) {
    cbdos::system::log(cbdos::system::LogLevel::Info, TAG, "=== Iniciando CyBerDeck OS (CBDos v0.2.0) ===");
    
    // 1. Inicializar Subsistema de Almacenamiento (MicroSD / Flash)
    if (!cbdos::storage::init()) {
        cbdos::system::log(cbdos::system::LogLevel::Warn, TAG, "Aviso: MicroSD no insertada o no montada al inicio");
    }

    // 2. Inicializar Subsistema de Pantalla a través de la API
    if (!cbdos::display::init()) {
        cbdos::system::log(cbdos::system::LogLevel::Error, TAG, "Error inicializando Display");
        return;
    }
    
    // 3. Inicializar Entrada Táctil a través de la API
    if (!cbdos::input::init()) {
        cbdos::system::log(cbdos::system::LogLevel::Warn, TAG, "Aviso: Touch no detectado o fallo inicializacion");
    }

    // 4. Inicializar Subsistema de Audio (I2S + Códec ES8311)
    if (!cbdos::audio::init()) {
        cbdos::system::log(cbdos::system::LogLevel::Warn, TAG, "Aviso: Fallo al inicializar subsistema de audio");
    }
    
    // 5. Inicializar Puerto LVGL 9.5
    if (LVGL_Port::getInstance().init() != ESP_OK) {
        cbdos::system::log(cbdos::system::LogLevel::Error, TAG, "Error inicializando LVGL 9.5 Port");
        return;
    }

    // 5. Inicializar Sistema de Interfaz Gráfica Universal (Fase 1)

    if (LVGL_Port::getInstance().lock()) {
        if (!cbdos::ui::init()) {
            cbdos::system::log(cbdos::system::LogLevel::Error, TAG, "Error inicializando UI Core");
            LVGL_Port::getInstance().unlock();
            return;
        }
        LVGL_Port::getInstance().unlock();
    }

    // 5. Tarea periódica para refresco de métricas en tiempo real (reloj, RAM, etc.)
    xTaskCreatePinnedToCore(uiUpdateTask, "ui_update_task", 4096, nullptr, 3, nullptr, 1);

    auto caps = cbdos::display::getCapabilities();
    cbdos::system::log(cbdos::system::LogLevel::Info, TAG, "CyBerDeck OS v0.2.0 iniciado y operando a %d FPS en %dx%d!", 
                       caps.targetFps, caps.width, caps.height);
}
