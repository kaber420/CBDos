#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace cbdos {
namespace flasher {

enum class FlasherStatus {
    Idle,
    EnteringBootloader,
    Connecting,
    Erasing,
    Writing,
    Verifying,
    Success,
    Failed
};

enum class FlasherTransport {
    UART_PINS,      // Conexión por pines GPIO (UART hardware)
    USB_CDC_NATIVE  // Conexión directa por cable USB-C (USB-Serial/JTAG CDC-ACM)
};

struct FlasherConfig {
    FlasherTransport transport = FlasherTransport::UART_PINS; // Modo de conexión
    int txPin = 32;                 // Host TX -> Target RX (Solo UART)
    int rxPin = 28;                 // Host RX -> Target TX (Solo UART)
    int bootPin = 34;               // Host GPIO -> Target Boot (IO9/IO0) (-1 si manual)
    int rstPin = 54;                // Host GPIO -> Target Reset/EN (-1 si manual)
    uint32_t baudRate = 115200;     // Velocidad UART / USB
    uint32_t flashOffset = 0x0;     // Offset de flasheo (0x0 o 0x10000)
    std::string binPath = "";       // Ruta en MicroSD ("" = firmware embebido si existe)
    std::string presetName = "Coprocesador C6";
};

struct FlasherPreset {
    std::string id;
    std::string name;
    std::string description;
    std::string wiringInfo;
    FlasherConfig config;
    bool useEmbeddedBin = false;
};

using FlasherProgressCb = std::function<void(FlasherStatus status, int percent, const char* message)>;

bool isSupported();
bool isBusy();
const std::vector<FlasherPreset>& getPresets();
FlasherConfig getDefaultConfig();
bool startFlash(const FlasherConfig& config, FlasherProgressCb progressCb = nullptr);
bool startFlash(FlasherProgressCb progressCb = nullptr);

} // namespace flasher
} // namespace cbdos

