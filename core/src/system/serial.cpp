#include "cbdos/serial.hpp"

namespace cbdos {
namespace serial {

static ISerialBackend* s_backend = nullptr;
static ISerialPort* s_activePort = nullptr;

void setBackend(ISerialBackend* backend) {
    s_backend = backend;
}

ISerialBackend* getBackend() {
    return s_backend;
}

std::vector<SerialPortDescriptor> getAvailablePorts() {
    if (s_backend) {
        return s_backend->getAvailablePorts();
    }
    return {};
}

bool open(const SerialConfig& config) {
    if (!s_backend) return false;
    
    if (s_activePort && s_activePort->isOpen()) {
        s_activePort->close();
    }

    s_activePort = s_backend->getPort(config.portId);
    if (!s_activePort) return false;

    return s_activePort->open(config);
}

void close() {
    if (s_activePort) {
        s_activePort->close();
        s_activePort = nullptr;
    }
}

bool isOpen() {
    return s_activePort != nullptr && s_activePort->isOpen();
}

size_t available() {
    if (s_activePort && s_activePort->isOpen()) {
        return s_activePort->available();
    }
    return 0;
}

size_t read(uint8_t* buffer, size_t maxLen) {
    if (s_activePort && s_activePort->isOpen()) {
        return s_activePort->read(buffer, maxLen);
    }
    return 0;
}

std::string readString(size_t maxLen) {
    if (s_activePort && s_activePort->isOpen()) {
        return s_activePort->readString(maxLen);
    }
    return "";
}

size_t write(const uint8_t* data, size_t len) {
    if (s_activePort && s_activePort->isOpen()) {
        return s_activePort->write(data, len);
    }
    return 0;
}

size_t writeString(const std::string& str) {
    if (s_activePort && s_activePort->isOpen()) {
        return s_activePort->writeString(str);
    }
    return 0;
}

void flush() {
    if (s_activePort && s_activePort->isOpen()) {
        s_activePort->flush();
    }
}

bool setBaudrate(uint32_t baudrate) {
    if (s_activePort && s_activePort->isOpen()) {
        return s_activePort->setBaudrate(baudrate);
    }
    return false;
}

bool setControlPin(bool level) {
    if (s_activePort && s_activePort->isOpen()) {
        return s_activePort->setControlPin(level);
    }
    return false;
}

bool pulseControlPin(uint32_t durationMs, bool enterBootloader) {
    if (s_activePort && s_activePort->isOpen()) {
        return s_activePort->pulseControlPin(durationMs, enterBootloader);
    }
    return false;
}

void setHotplugCallback(std::function<void(bool connected, const std::string& portId)> cb) {
    if (s_backend) {
        s_backend->setHotplugCallback(cb);
    }
}

} // namespace serial
} // namespace cbdos
