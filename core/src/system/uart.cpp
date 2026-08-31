#include "cbdos/uart.hpp"

namespace cbdos {
namespace uart {

static IUartBackend* s_backend = nullptr;

void setBackend(IUartBackend* backend) {
    s_backend = backend;
}

IUartBackend* getBackend() {
    return s_backend;
}

bool init(int txPin, int rxPin, uint32_t baudrate) {
    if (s_backend) {
        return s_backend->init(txPin, rxPin, baudrate);
    }
    return false;
}

void deinit() {
    if (s_backend) {
        s_backend->deinit();
    }
}

bool isInitialized() {
    if (s_backend) {
        return s_backend->isInitialized();
    }
    return false;
}

size_t available() {
    if (s_backend) {
        return s_backend->available();
    }
    return 0;
}

size_t read(uint8_t* buffer, size_t maxLen) {
    if (s_backend) {
        return s_backend->read(buffer, maxLen);
    }
    return 0;
}

std::string readString(size_t maxLen) {
    if (s_backend) {
        return s_backend->readString(maxLen);
    }
    return "";
}

size_t write(const uint8_t* data, size_t len) {
    if (s_backend) {
        return s_backend->write(data, len);
    }
    return 0;
}

size_t writeString(const std::string& str) {
    if (s_backend) {
        return s_backend->writeString(str);
    }
    return 0;
}

void flush() {
    if (s_backend) {
        s_backend->flush();
    }
}

bool setBaudrate(uint32_t baudrate) {
    if (s_backend) {
        return s_backend->setBaudrate(baudrate);
    }
    return false;
}

int getDefaultTxPin() {
    if (s_backend) {
        return s_backend->getDefaultTxPin();
    }
    return -1;
}

int getDefaultRxPin() {
    if (s_backend) {
        return s_backend->getDefaultRxPin();
    }
    return -1;
}

uint32_t getDefaultBaudrate() {
    if (s_backend) {
        return s_backend->getDefaultBaudrate();
    }
    return 115200;
}

static const std::vector<UartPinPreset> s_emptyPresets;

const std::vector<UartPinPreset>& getPinPresets() {
    if (s_backend) {
        return s_backend->getPinPresets();
    }
    return s_emptyPresets;
}

} // namespace uart
} // namespace cbdos
