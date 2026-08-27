#include "hal_flasher_p4.hpp"
#include "cbdos/flasher.hpp"
#include <esp_loader.h>
#include <esp_loader_io.h>
#include <esp32_port.h>

#include <driver/uart.h>
#include <driver/gpio.h>

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

static const char* TAG_FLASHER = "HAL_FLASHER_P4";

// Símbolos del binario SDIO C6 embebido
extern const uint8_t c6_slave_bin_start[] asm("_binary_c6_slave_bin_start");
extern const uint8_t c6_slave_bin_end[]   asm("_binary_c6_slave_bin_end");

#define UART_PORT_NUM       UART_NUM_1

namespace cbdos {
namespace system {

FlasherServiceP4::FlasherServiceP4() {
    // 1. Preset Coprocesador ESP32-C6 (Integrado en placa JC4880P443C)
    cbdos::flasher::FlasherPreset p1;
    p1.id = "p4_c6_internal";
    p1.name = "ESP32-C6 Coprocesador (JC4880)";
    p1.description = "Coprocesador WiFi 6 / BT 5 integrado en modulo P4.";
    p1.wiringInfo =
        "+---------------------------------------------+\n"
        "|  CONEXIONES EN CONECTOR JP1 (2x13)          |\n"
        "+---------------------------------------------+\n"
        "| [Jumper 1] Pin 19 (IO32) --> Pin 20 (C6_RX) |\n"
        "| [Jumper 2] Pin 21 (IO28) --> Pin 22 (C6_TX) |\n"
        "| [Cable 1]  Pin 17 (IO34) --> Pin 24 (C6_IO9)|\n"
        "+---------------------------------------------+\n"
        "| Energia (IO36) y Reset (IO54) son INTERNOS  |";
    p1.config.txPin = 32;
    p1.config.rxPin = 28;
    p1.config.bootPin = 34;
    p1.config.rstPin = 54;
    p1.config.baudRate = 115200;
    p1.config.flashOffset = 0x0;
    p1.config.binPath = ""; // Embebido o /sdcard/network_adapter.bin
    p1.config.presetName = p1.name;
    p1.useEmbeddedBin = true;
    m_presets.push_back(p1);

    // 2. Preset ESP Externo vía Header JP1 (ESP32-P4)
    cbdos::flasher::FlasherPreset p2;
    p2.id = "p4_jp1_external";
    p2.name = "ESP Externo (Header JP1 P4)";
    p2.description = "Flasheo de chips ESP externos conectados a JP1.";
    p2.wiringInfo =
        "+---------------------------------------------+\n"
        "|  CONEXIONES JP1 HACIA ESP EXTERNO           |\n"
        "+---------------------------------------------+\n"
        "| [UART TX]  JP1 Pin 19 (GPIO 32) -> Target RX|\n"
        "| [UART RX]  JP1 Pin 21 (GPIO 28) -> Target TX|\n"
        "| [BOOT IO0] JP1 Pin 17 (GPIO 34) -> Target IO0\n"
        "| [RESET]    JP1 Pin 54 / Manual  -> Target EN|\n"
        "| [POWER]    JP1 Pin 01 (3.3V/5V) -> Target VCC\n"
        "| [TIERRA]   JP1 Pin 05 (GND)     -> Target GND|\n"
        "+---------------------------------------------+";
    p2.config.txPin = 32;
    p2.config.rxPin = 28;
    p2.config.bootPin = 34;
    p2.config.rstPin = 54;
    p2.config.baudRate = 115200;
    p2.config.flashOffset = 0x0;
    p2.config.binPath = "/sdcard/firmware.bin";
    p2.config.presetName = p2.name;
    p2.useEmbeddedBin = false;
    m_presets.push_back(p2);

    // 3. Preset ESP Externo para ESP32-S3 (JC3248W535)
    cbdos::flasher::FlasherPreset p3;
    p3.id = "s3_external";
    p3.name = "ESP Externo (JC3248W535 S3)";
    p3.description = "Flasheo desde tarjeta S3 usando pines libres.";
    p3.wiringInfo =
        "+---------------------------------------------+\n"
        "|  CONEXIONES S3 HACIA ESP EXTERNO            |\n"
        "+---------------------------------------------+\n"
        "| [UART TX]  GPIO 15        -> Target RX      |\n"
        "| [UART RX]  GPIO 16        -> Target TX      |\n"
        "| [BOOT IO0] GPIO 0         -> Target GPIO 0  |\n"
        "| [RESET]    Boton Manual   -> Target EN / RST|\n"
        "| [FIRMWARE] MicroSD        -> /sdcard/*.bin  |\n"
        "+---------------------------------------------+";
    p3.config.txPin = 15;
    p3.config.rxPin = 16;
    p3.config.bootPin = 0;
    p3.config.rstPin = -1;
    p3.config.baudRate = 115200;
    p3.config.flashOffset = 0x0;
    p3.config.binPath = "/sdcard/firmware.bin";
    p3.config.presetName = p3.name;
    p3.useEmbeddedBin = false;
    m_presets.push_back(p3);

    // 4. Preset Personalizado / Manual
    cbdos::flasher::FlasherPreset p4;
    p4.id = "custom";
    p4.name = "Personalizado / Manual";
    p4.description = "Configuracion libre de GPIOs, baudrate y firmware.";
    p4.wiringInfo =
        "+---------------------------------------------+\n"
        "|  GUIA DE CONEXION PERSONALIZADA             |\n"
        "+---------------------------------------------+\n"
        "| 1. Conectar Host TX al pin RX del objetivo. |\n"
        "| 2. Conectar Host RX al pin TX del objetivo. |\n"
        "| 3. Conectar Host BOOT al pin GPIO0/IO9.     |\n"
        "| 4. Conectar Host RST al pin Reset/EN.       |\n"
        "| 5. Conectar masa GND comun entre placas.    |\n"
        "+---------------------------------------------+";
    p4.config.txPin = 32;
    p4.config.rxPin = 28;
    p4.config.bootPin = 34;
    p4.config.rstPin = 54;
    p4.config.baudRate = 115200;
    p4.config.flashOffset = 0x0;
    p4.config.binPath = "/sdcard/firmware.bin";
    p4.config.presetName = p4.name;
    p4.useEmbeddedBin = false;
    m_presets.push_back(p4);

    m_activeConfig = m_presets[0].config;
}

FlasherServiceP4& FlasherServiceP4::getInstance() {
    static FlasherServiceP4 instance;
    return instance;
}

const std::vector<cbdos::flasher::FlasherPreset>& FlasherServiceP4::getPresets() const {
    return m_presets;
}

cbdos::flasher::FlasherConfig FlasherServiceP4::getDefaultConfig() const {
    if (!m_presets.empty()) {
        return m_presets[0].config;
    }
    return cbdos::flasher::FlasherConfig();
}

bool FlasherServiceP4::startFlash(const cbdos::flasher::FlasherConfig& config, cbdos::flasher::FlasherProgressCb progressCb) {
    if (m_busy) {
        ESP_LOGW(TAG_FLASHER, "Flasheador ocupado actualmente");
        return false;
    }
    m_activeConfig = config;
    m_busy = true;
    m_cb = progressCb;
    m_status = cbdos::flasher::FlasherStatus::EnteringBootloader;
    m_progress = 0;
    m_message = "Iniciando modo bootloader...";

    if (m_cb) m_cb(m_status, m_progress, m_message.c_str());

    xTaskCreatePinnedToCore(flashTaskWrapper, "flasher_task", 8192, this, 5, nullptr, 0);
    return true;
}

bool FlasherServiceP4::startFlash(cbdos::flasher::FlasherProgressCb progressCb) {
    return startFlash(getDefaultConfig(), progressCb);
}

void FlasherServiceP4::flashTaskWrapper(void* arg) {
    auto* self = static_cast<FlasherServiceP4*>(arg);
    self->runFlashTask();
    vTaskDelete(nullptr);
}

void FlasherServiceP4::runFlashTask() {
    ESP_LOGI(TAG_FLASHER, "=== Iniciando Flasheo Universal [%s] ===", m_activeConfig.presetName.c_str());
    ESP_LOGI(TAG_FLASHER, "Pines: TX=%d, RX=%d, BOOT=%d, RST=%d, Baud=%lu, Offset=0x%lx",
             m_activeConfig.txPin, m_activeConfig.rxPin, m_activeConfig.bootPin,
             m_activeConfig.rstPin, (unsigned long)m_activeConfig.baudRate, (unsigned long)m_activeConfig.flashOffset);

    // 1. Determinar fuente del binario
    const uint8_t* binData = nullptr;
    size_t binSize = 0;
    uint8_t* sdBuf = nullptr;

    if (!m_activeConfig.binPath.empty()) {
        FILE* fSd = fopen(m_activeConfig.binPath.c_str(), "rb");
        if (fSd) {
            fseek(fSd, 0, SEEK_END);
            binSize = ftell(fSd);
            fseek(fSd, 0, SEEK_SET);
            ESP_LOGI(TAG_FLASHER, "Firmware cargado desde MicroSD (%s): %u bytes", m_activeConfig.binPath.c_str(), binSize);
            sdBuf = (uint8_t*)malloc(binSize);
            if (sdBuf) {
                fread(sdBuf, 1, binSize, fSd);
                binData = sdBuf;
            }
            fclose(fSd);
        } else {
            ESP_LOGW(TAG_FLASHER, "No se encontro archivo en MicroSD: %s", m_activeConfig.binPath.c_str());
        }
    }

    // Fallback para Coprocesador C6 si no hay archivo en SD: usar binario SDIO oficial embebido
    if (!binData && (m_activeConfig.binPath.empty() || m_activeConfig.binPath == "/sdcard/network_adapter.bin")) {
        binData = c6_slave_bin_start;
        binSize = c6_slave_bin_end - c6_slave_bin_start;
        ESP_LOGI(TAG_FLASHER, "Usando firmware SDIO oficial embebido en Flash: %u bytes", binSize);
    }

    if (!binData || binSize == 0) {
        m_status = cbdos::flasher::FlasherStatus::Failed;
        m_message = "Error: Binario de firmware vacío o no encontrado en SD";
        if (m_cb) m_cb(m_status, 0, m_message.c_str());
        m_busy = false;
        return;
    }

    // Asegurar que el carril ESP_3V3 (GPIO 36) este energizado
    gpio_config_t pwr_conf = {};
    pwr_conf.intr_type = GPIO_INTR_DISABLE;
    pwr_conf.mode = GPIO_MODE_OUTPUT;
    pwr_conf.pin_bit_mask = (1ULL << GPIO_NUM_36);
    pwr_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    pwr_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&pwr_conf);
    gpio_set_level(GPIO_NUM_36, 1);

    // 2. Configurar puertos y GPIOs para esp-serial-flasher
    loader_esp32_config_t config = {};
    config.baud_rate = m_activeConfig.baudRate > 0 ? m_activeConfig.baudRate : 115200;
    config.uart_port = UART_PORT_NUM;
    config.uart_rx_pin = static_cast<gpio_num_t>(m_activeConfig.rxPin);
    config.uart_tx_pin = static_cast<gpio_num_t>(m_activeConfig.txPin);
    config.reset_trigger_pin = m_activeConfig.rstPin >= 0 ? static_cast<gpio_num_t>(m_activeConfig.rstPin) : GPIO_NUM_NC;
    config.gpio0_trigger_pin = m_activeConfig.bootPin >= 0 ? static_cast<gpio_num_t>(m_activeConfig.bootPin) : GPIO_NUM_NC;

    m_status = cbdos::flasher::FlasherStatus::Connecting;
    m_progress = 5;
    m_message = "Sincronizando con ROM del microcontrolador objetivo...";
    if (m_cb) m_cb(m_status, m_progress, m_message.c_str());

    if (loader_port_esp32_init(&config) != ESP_LOADER_SUCCESS) {
        m_status = cbdos::flasher::FlasherStatus::Failed;
        m_message = "Fallo al inicializar puerto UART para flasheo";
        if (m_cb) m_cb(m_status, 0, m_message.c_str());
        if (sdBuf) free(sdBuf);
        m_busy = false;
        return;
    }

    // 3. Conectar e identificar chip
    esp_loader_connect_args_t connect_args = ESP_LOADER_CONNECT_DEFAULT();
    connect_args.trials = 15;
    connect_args.sync_timeout = 200;

    esp_loader_error_t err = esp_loader_connect(&connect_args);
    if (err != ESP_LOADER_SUCCESS) {
        ESP_LOGE(TAG_FLASHER, "Fallo esp_loader_connect (err=%d)", err);
        m_status = cbdos::flasher::FlasherStatus::Failed;
        m_message = "No responde el chip (Verifica conexiones TX/RX/BOOT/VCC)";
        if (m_cb) m_cb(m_status, 0, m_message.c_str());
        loader_port_esp32_deinit();
        if (sdBuf) free(sdBuf);
        m_busy = false;
        return;
    }

    target_chip_t target = esp_loader_get_target();
    ESP_LOGI(TAG_FLASHER, "Microcontrolador detectado exitosamente! Target chip ID: %d", target);

    // 4. Preparar memoria Flash
    m_status = cbdos::flasher::FlasherStatus::Erasing;
    m_progress = 15;
    m_message = "Borrando Flash del microcontrolador objetivo...";
    if (m_cb) m_cb(m_status, m_progress, m_message.c_str());

    err = esp_loader_flash_start(m_activeConfig.flashOffset, binSize, 4096);
    if (err != ESP_LOADER_SUCCESS) {
        ESP_LOGE(TAG_FLASHER, "Fallo esp_loader_flash_start (err=%d)", err);
        m_status = cbdos::flasher::FlasherStatus::Failed;
        m_message = "Fallo al inicializar borrado de memoria Flash";
        if (m_cb) m_cb(m_status, 0, m_message.c_str());
        loader_port_esp32_deinit();
        if (sdBuf) free(sdBuf);
        m_busy = false;
        return;
    }

    // 5. Escritura de bloques (4096 bytes por paquete)
    m_status = cbdos::flasher::FlasherStatus::Writing;
    size_t offset = 0;
    const size_t chunkSize = 4096;
    int lastNotifiedPct = -1;
    uint8_t chunkBuffer[4096];

    while (offset < binSize) {
        size_t toWrite = binSize - offset;
        if (toWrite > chunkSize) toWrite = chunkSize;

        memcpy(chunkBuffer, binData + offset, toWrite);

        err = esp_loader_flash_write(chunkBuffer, toWrite);
        if (err != ESP_LOADER_SUCCESS) {
            ESP_LOGE(TAG_FLASHER, "Fallo al escribir en offset 0x%x (err=%d)", offset, err);
            m_status = cbdos::flasher::FlasherStatus::Failed;
            m_message = "Error de escritura durante transmisión UART";
            if (m_cb) m_cb(m_status, 0, m_message.c_str());
            loader_port_esp32_deinit();
            if (sdBuf) free(sdBuf);
            m_busy = false;
            return;
        }

        offset += toWrite;
        int pct = 15 + (int)((offset * 80) / binSize);
        m_progress = pct;

        // Notificar a la UI solo si avanza el porcentaje o es el último bloque
        if (pct != lastNotifiedPct || offset >= binSize) {
            lastNotifiedPct = pct;
            char msgBuf[64];
            snprintf(msgBuf, sizeof(msgBuf), "Escribiendo Flash... %d KB / %d KB", (int)(offset / 1024), (int)(binSize / 1024));
            m_message = msgBuf;
            if (m_cb) m_cb(m_status, m_progress, m_message.c_str());
        }

        vTaskDelay(pdMS_TO_TICKS(2));
    }

    // 6. Finalizar y verificar
    m_status = cbdos::flasher::FlasherStatus::Verifying;
    m_progress = 98;
    m_message = "Verificando integridad y reiniciando chip...";
    if (m_cb) m_cb(m_status, m_progress, m_message.c_str());

    err = esp_loader_flash_finish(true);
    if (err != ESP_LOADER_SUCCESS) {
        ESP_LOGW(TAG_FLASHER, "Aviso esp_loader_flash_finish: %d", err);
    }

    // 7. Liberar UART y reiniciar el target en modo normal
    loader_port_esp32_deinit();

    if (m_activeConfig.bootPin >= 0) {
        gpio_set_direction(static_cast<gpio_num_t>(m_activeConfig.bootPin), GPIO_MODE_OUTPUT);
        gpio_set_level(static_cast<gpio_num_t>(m_activeConfig.bootPin), 1); // Salir de bootloader
    }

    if (m_activeConfig.rstPin >= 0) {
        gpio_set_direction(static_cast<gpio_num_t>(m_activeConfig.rstPin), GPIO_MODE_OUTPUT);
        gpio_set_level(static_cast<gpio_num_t>(m_activeConfig.rstPin), 0); // Pulso de Reset
        vTaskDelay(pdMS_TO_TICKS(100));
        gpio_set_level(static_cast<gpio_num_t>(m_activeConfig.rstPin), 1); // Liberar Reset
    }

    if (sdBuf) free(sdBuf);

    m_status = cbdos::flasher::FlasherStatus::Success;
    m_progress = 100;
    m_message = "¡Microcontrolador programado exitosamente al 100%!";
    if (m_cb) m_cb(m_status, m_progress, m_message.c_str());

    ESP_LOGI(TAG_FLASHER, "=== Flasheo Completado con Éxito ===");
    m_busy = false;
}

} // namespace system

namespace flasher {

bool isSupported() {
    return true;
}

bool isBusy() {
    return system::FlasherServiceP4::getInstance().isBusy();
}

const std::vector<FlasherPreset>& getPresets() {
    return system::FlasherServiceP4::getInstance().getPresets();
}

FlasherConfig getDefaultConfig() {
    return system::FlasherServiceP4::getInstance().getDefaultConfig();
}

bool startFlash(const FlasherConfig& config, FlasherProgressCb progressCb) {
    return system::FlasherServiceP4::getInstance().startFlash(config, progressCb);
}

bool startFlash(FlasherProgressCb progressCb) {
    return system::FlasherServiceP4::getInstance().startFlash(progressCb);
}

} // namespace flasher
} // namespace cbdos
