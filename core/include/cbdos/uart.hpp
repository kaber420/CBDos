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

// Funciones del subsistema HAL UART
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
