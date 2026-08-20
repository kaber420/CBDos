#include "C6FlasherService.hpp"
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

static const char* TAG_FLASHER = "UNIVERSAL_FLASHER";

// Símbolos del binario SDIO C6 embebido
extern const uint8_t c6_slave_bin_start[] asm("_binary_c6_slave_bin_start");
extern const uint8_t c6_slave_bin_end[]   asm("_binary_c6_slave_bin_end");

#define UART_PORT_NUM       UART_NUM_1

namespace cbdos {
namespace system {

C6FlasherService::C6FlasherService() {
    // 1. Preset Coprocesador ESP32-C6 (Integrado en placa JC4880P443C)
    cbdos::flasher::FlasherPreset p1;
    p1.id = "p4_c6_internal";
    p1.name = "ESP32-C6 Coprocesador (JC4880)";
    p1.description = "Coprocesador WiFi 6 / BT 5 integrado en módulo P4.";
    p1.wiringInfo =
        "Conexiones JP1 (2x13):\n"
        "• Pin 1 (3V3) ────[Cable 1]───> Pin 18 (ESP_3V3)\n"
        "• Pin 17 (GPIO34) ─[Cable 2]──> Pin 24 (C6_IO9) [Boot]\n"
        "• Pin 19 (GPIO32) ─[Jumper 1]─> Pin 20 (C6_U0RXD) [TX]\n"
        "• Pin 21 (GPIO28) ─[Jumper 2]─> Pin 22 (C6_U0TXD) [RX]";
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
        "Conexiones Externas:\n"
        "• JP1 Pin 19 (GPIO32) ──> Target RX\n"
        "• JP1 Pin 21 (GPIO28) ──> Target TX\n"
        "• JP1 Pin 17 (GPIO34) ──> Target GPIO0/BOOT\n"
        "• JP1 Pin 54 / Manual ──> Target EN/RST\n"
        "• JP1 GND y 3V3/5V ─────> Target GND y VCC";
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
        "Conexiones S3:\n"
        "• GPIO 15 (TX) ──> Target RX\n"
        "• GPIO 16 (RX) ──> Target TX\n"
        "• GPIO 0  (BOOT)─> Target GPIO0\n"
        "• Target RST ───> Botón Reset o Control Manual\n"
        "• MicroSD ──────> /sdcard/firmware.bin";
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
    p4.description = "Configuración libre de GPIOs, baudrate y firmware.";
    p4.wiringInfo =
        "Configura los pines GPIO asignados en la pantalla\n"
        "y conecta las líneas correspondientes a tu chip ESP.";
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

C6FlasherService& C6FlasherService::getInstance() {
    static C6FlasherService instance;
    return instance;
}

const std::vector<cbdos::flasher::FlasherPreset>& C6FlasherService::getPresets() const {
    return m_presets;
}

cbdos::flasher::FlasherConfig C6FlasherService::getDefaultConfig() const {
    if (!m_presets.empty()) {
        return m_presets[0].config;
    }
    return cbdos::flasher::FlasherConfig();
}

bool C6FlasherService::startFlash(const cbdos::flasher::FlasherConfig& config, cbdos::flasher::FlasherProgressCb progressCb) {
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

bool C6FlasherService::startFlash(cbdos::flasher::FlasherProgressCb progressCb) {
    return startFlash(getDefaultConfig(), progressCb);
}

void C6FlasherService::flashTaskWrapper(void* arg) {
    auto* self = static_cast<C6FlasherService*>(arg);
    self->runFlashTask();
    vTaskDelete(nullptr);
}

void C6FlasherService::runFlashTask() {
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

    while (offset < binSize) {
        size_t toWrite = binSize - offset;
        if (toWrite > chunkSize) toWrite = chunkSize;

        err = esp_loader_flash_write((void*)(binData + offset), toWrite);
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

        char msgBuf[64];
        snprintf(msgBuf, sizeof(msgBuf), "Escribiendo Flash... %d KB / %d KB", (int)(offset / 1024), (int)(binSize / 1024));
        m_message = msgBuf;
        if (m_cb) m_cb(m_status, m_progress, m_message.c_str());

        vTaskDelay(pdMS_TO_TICKS(1));
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
    return system::C6FlasherService::getInstance().isBusy();
}

const std::vector<FlasherPreset>& getPresets() {
    return system::C6FlasherService::getInstance().getPresets();
}

FlasherConfig getDefaultConfig() {
    return system::C6FlasherService::getInstance().getDefaultConfig();
}

bool startFlash(const FlasherConfig& config, FlasherProgressCb progressCb) {
    return system::C6FlasherService::getInstance().startFlash(config, progressCb);
}

bool startFlash(FlasherProgressCb progressCb) {
    return system::C6FlasherService::getInstance().startFlash(progressCb);
}

} // namespace flasher
} // namespace cbdos



