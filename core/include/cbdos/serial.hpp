#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <functional>

namespace cbdos {
namespace serial {

enum class PortType {
    HardwareUart,
    UsbCdcAcm,
    ManualUart
};

enum class LineEnding {
    CRLF,   // \r\n (módems AT, ESP-AT)
    LF,     // \n   (Linux, BusyBox, Raspberry Pi)
    CR,     // \r   (consolas retro / REPLs)
    NONE    // texto plano sin terminador
};

struct SerialPortDescriptor {
    std::string id;            // ej: "jp1", "usb0", "ext_s3", "manual"
    std::string displayName;   // ej: "UART JP1 (TX:32 RX:28)", "USB CDC-ACM", "Manual"
    PortType type;
    int defaultTx;             // -1 si es USB
    int defaultRx;             // -1 si es USB
    int controlPin;            // 34 en JP1 P4, -1 si no aplica
    bool isAvailable;          // true para UART fija o si USB CDC está conectado
};

struct SerialConfig {
    std::string portId;
    uint32_t baudrate = 115200;
    int txPin = -1;
    int rxPin = -1;
    int controlPin = -1;
};

// ────────────────────────────────────────────────────────────────
// Contrato HAL C++ Puro para un Puerto Serie Activo
// ────────────────────────────────────────────────────────────────

class ISerialPort {
public:
    virtual ~ISerialPort() = default;

    virtual bool open(const SerialConfig& config) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    virtual size_t available() = 0;
    virtual size_t read(uint8_t* buffer, size_t maxLen) = 0;
    virtual std::string readString(size_t maxLen = 1024) = 0;
    virtual size_t write(const uint8_t* data, size_t len) = 0;
    virtual size_t writeString(const std::string& str) = 0;
    virtual void flush() = 0;
    virtual bool setBaudrate(uint32_t baudrate) = 0;
    
    // Control auxiliar de línea y reinicio dual (ej. GPIO 34/54 en JP1, DTR/RTS en USB)
    virtual bool setControlPin(bool level) = 0;
    virtual bool pulseControlPin(uint32_t durationMs = 100, bool enterBootloader = false) = 0;
};

// ────────────────────────────────────────────────────────────────
// Contrato HAL C++ Puro para el Backend Serial del Sistema
// ────────────────────────────────────────────────────────────────

class ISerialBackend {
public:
    virtual ~ISerialBackend() = default;

    virtual std::vector<SerialPortDescriptor> getAvailablePorts() = 0;
    virtual ISerialPort* getPort(const std::string& portId) = 0;
    virtual void setHotplugCallback(std::function<void(bool connected, const std::string& portId)> cb) = 0;
};

void setBackend(ISerialBackend* backend);
ISerialBackend* getBackend();

// ────────────────────────────────────────────────────────────────
// APIs Públicas de CBDos (Consumidas por Vistas, Apps y Lua)
// ────────────────────────────────────────────────────────────────

std::vector<SerialPortDescriptor> getAvailablePorts();
bool open(const SerialConfig& config);
void close();
bool isOpen();
size_t available();
size_t read(uint8_t* buffer, size_t maxLen);
std::string readString(size_t maxLen = 1024);
size_t write(const uint8_t* data, size_t len);
size_t writeString(const std::string& str);
void flush();
bool setBaudrate(uint32_t baudrate);
bool setControlPin(bool level);
bool pulseControlPin(uint32_t durationMs = 100, bool enterBootloader = false);

inline bool resetTarget() { return pulseControlPin(100, false); }
inline bool enterBootloader() { return pulseControlPin(100, true); }

void setHotplugCallback(std::function<void(bool connected, const std::string& portId)> cb);

} // namespace serial
} // namespace cbdos
