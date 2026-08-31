#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace cbdos {
namespace uart {

struct UartPinPreset {
    std::string name;
    int txPin;
    int rxPin;
};

// ────────────────────────────────────────────────────────────────
// Contrato HAL C++ Puro para UART / Comunicación Serie
// ────────────────────────────────────────────────────────────────

class IUartBackend {
public:
    virtual ~IUartBackend() = default;

    virtual bool init(int txPin, int rxPin, uint32_t baudrate) = 0;
    virtual void deinit() = 0;
    virtual bool isInitialized() const = 0;
    virtual size_t available() = 0;
    virtual size_t read(uint8_t* buffer, size_t maxLen) = 0;
    virtual std::string readString(size_t maxLen = 1024) = 0;
    virtual size_t write(const uint8_t* data, size_t len) = 0;
    virtual size_t writeString(const std::string& str) = 0;
    virtual void flush() = 0;
    virtual bool setBaudrate(uint32_t baudrate) = 0;
    virtual int getDefaultTxPin() const = 0;
    virtual int getDefaultRxPin() const = 0;
    virtual uint32_t getDefaultBaudrate() const = 0;
    virtual const std::vector<UartPinPreset>& getPinPresets() const = 0;
};

void setBackend(IUartBackend* backend);
IUartBackend* getBackend();

// ────────────────────────────────────────────────────────────────
// APIs públicas de CBDos (Consumidas por Vistas, Apps y Lua)
// ────────────────────────────────────────────────────────────────

bool init(int txPin, int rxPin, uint32_t baudrate);
void deinit();
bool isInitialized();
size_t available();
size_t read(uint8_t* buffer, size_t maxLen);
std::string readString(size_t maxLen = 1024);
size_t write(const uint8_t* data, size_t len);
size_t writeString(const std::string& str);
void flush();
bool setBaudrate(uint32_t baudrate);

int getDefaultTxPin();
int getDefaultRxPin();
uint32_t getDefaultBaudrate();
const std::vector<UartPinPreset>& getPinPresets();

} // namespace uart
} // namespace cbdos

