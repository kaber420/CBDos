#include "SerialStreamAdapter.hpp"

namespace cbdos {
namespace terminal {

size_t SerialStreamAdapter::write(const uint8_t* data, size_t len) {
    if (!data || len == 0 || !isConnected()) return 0;
    return cbdos::serial::write(data, len);
}

size_t SerialStreamAdapter::read(uint8_t* buffer, size_t maxLen) {
    if (!buffer || maxLen == 0 || !isConnected()) return 0;
    return cbdos::serial::read(buffer, maxLen);
}

size_t SerialStreamAdapter::available() {
    if (!isConnected()) return 0;
    return cbdos::serial::available();
}

bool SerialStreamAdapter::isConnected() const {
    return cbdos::serial::isOpen();
}

void SerialStreamAdapter::close() {
    if (cbdos::serial::isOpen()) {
        cbdos::serial::close();
        if (m_onStateChanged) {
            m_onStateChanged(StreamState::Disconnected, "Puerto serial cerrado");
        }
    }
}

bool SerialStreamAdapter::open(const cbdos::serial::SerialConfig& config) {
    bool ok = cbdos::serial::open(config);
    if (ok) {
        if (m_onStateChanged) {
            m_onStateChanged(StreamState::Connected, "Puerto serial abierto con éxito");
        }
    } else {
        if (m_onStateChanged) {
            m_onStateChanged(StreamState::Error, "Fallo al abrir puerto serial");
        }
    }
    return ok;
}

bool SerialStreamAdapter::setBaudrate(uint32_t baudrate) {
    return cbdos::serial::setBaudrate(baudrate);
}

bool SerialStreamAdapter::pulseControlPin(uint32_t durationMs, bool enterBootloader) {
    return cbdos::serial::pulseControlPin(durationMs, enterBootloader);
}

} // namespace terminal
} // namespace cbdos
