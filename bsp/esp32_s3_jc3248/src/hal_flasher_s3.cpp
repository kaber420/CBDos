#include "cbdos/flasher.hpp"

namespace cbdos {
namespace flasher {

static std::vector<FlasherPreset> s_s3Presets = {
    {
        "s3_external",
        "ESP Externo (JC3248W535 S3)",
        "Flasheo desde tarjeta S3 usando pines libres.",
        "Conexiones S3:\n"
        "• GPIO 15 (TX) ──> Target RX\n"
        "• GPIO 16 (RX) ──> Target TX\n"
        "• GPIO 0  (BOOT)─> Target GPIO0\n"
        "• MicroSD ──────> /sdcard/firmware.bin",
        {15, 16, 0, -1, 115200, 0x0, "/sdcard/firmware.bin", "ESP Externo (JC3248W535 S3)"},
        false
    },
    {
        "p4_c6_internal",
        "ESP32-C6 Coprocesador (JC4880)",
        "Coprocesador WiFi 6 / BT 5 (Solo placa P4).",
        "Preset no aplicable directamente en placa S3.",
        {32, 28, 34, 54, 115200, 0x0, "", "ESP32-C6 Coprocesador"},
        true
    },
    {
        "custom",
        "Personalizado / Manual",
        "Configuración libre de GPIOs, baudrate y firmware.",
        "Configura los pines GPIO asignados en la pantalla\n"
        "y conecta las líneas correspondientes a tu chip ESP.",
        {15, 16, 0, -1, 115200, 0x0, "/sdcard/firmware.bin", "Personalizado / Manual"},
        false
    }
};

bool isSupported() {
    return true;
}

bool isBusy() {
    return false;
}

const std::vector<FlasherPreset>& getPresets() {
    return s_s3Presets;
}

FlasherConfig getDefaultConfig() {
    if (!s_s3Presets.empty()) {
        return s_s3Presets[0].config;
    }
    return FlasherConfig();
}

bool startFlash(const FlasherConfig& config, FlasherProgressCb progressCb) {
    if (progressCb) {
        progressCb(FlasherStatus::Failed, 0, "Flasheador hardware UART en S3 en desarrollo");
    }
    return false;
}

bool startFlash(FlasherProgressCb progressCb) {
    return startFlash(getDefaultConfig(), progressCb);
}

} // namespace flasher
} // namespace cbdos

