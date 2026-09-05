#include "SshStreamAdapter.hpp"
#include <algorithm>

namespace cbdos {
namespace terminal {

SshStreamAdapter::SshStreamAdapter() {
    m_rxFifo.reserve(2048);
}

SshStreamAdapter::~SshStreamAdapter() {
    close();
}

size_t SshStreamAdapter::write(const uint8_t* data, size_t len) {
    if (!data || len == 0 || !isConnected()) return 0;
    bool ok = cbdos::ssh::sendInput(data, len);
    return ok ? len : 0;
}

size_t SshStreamAdapter::read(uint8_t* buffer, size_t maxLen) {
    if (!buffer || maxLen == 0) return 0;

    std::lock_guard<std::mutex> lock(m_fifoMutex);
    if (m_rxFifo.empty()) return 0;

    size_t toCopy = std::min(maxLen, m_rxFifo.size());
    std::copy(m_rxFifo.begin(), m_rxFifo.begin() + toCopy, buffer);
    m_rxFifo.erase(m_rxFifo.begin(), m_rxFifo.begin() + toCopy);
    return toCopy;
}

size_t SshStreamAdapter::available() {
    std::lock_guard<std::mutex> lock(m_fifoMutex);
    return m_rxFifo.size();
}

bool SshStreamAdapter::isConnected() const {
    return cbdos::ssh::isConnected();
}

void SshStreamAdapter::close() {
    if (cbdos::ssh::isConnected()) {
        cbdos::ssh::closeShell();
        cbdos::ssh::disconnect();
    }
    {
        std::lock_guard<std::mutex> lock(m_fifoMutex);
        m_rxFifo.clear();
    }
    if (m_onStateChanged) {
        m_onStateChanged(StreamState::Disconnected, "Sesión SSH finalizada");
    }
}

bool SshStreamAdapter::connect(const cbdos::ssh::SshConfig& config, uint16_t cols, uint16_t rows) {
    close();

    if (m_onStateChanged) {
        m_onStateChanged(StreamState::Connecting, "Conectando al servidor SSH...");
    }

    bool ok = cbdos::ssh::connect(config, [this](cbdos::ssh::SshSessionState state, const std::string& msg) {
        if (!m_onStateChanged) return;
        switch (state) {
            case cbdos::ssh::SshSessionState::ConnectingTcp:
            case cbdos::ssh::SshSessionState::KeyExchange:
            case cbdos::ssh::SshSessionState::Authenticating:
                m_onStateChanged(StreamState::Connecting, msg);
                break;
            case cbdos::ssh::SshSessionState::Ready:
                m_onStateChanged(StreamState::Connected, msg);
                break;
            case cbdos::ssh::SshSessionState::Disconnected:
                m_onStateChanged(StreamState::Disconnected, msg);
                break;
            default:
                m_onStateChanged(StreamState::Error, msg);
                break;
        }
    });

    if (!ok) {
        return false;
    }

    // Abrir shell interactivo enganchando el callback asíncrono
    bool shellOk = cbdos::ssh::openShell([this](const uint8_t* data, size_t len) {
        this->handleIncomingData(data, len);
    }, cols, rows, config.termType);

    if (!shellOk) {
        close();
        if (m_onStateChanged) {
            m_onStateChanged(StreamState::Error, "Fallo al inicializar PTY interactivo SSH");
        }
        return false;
    }

    return true;
}

void SshStreamAdapter::handleIncomingData(const uint8_t* data, size_t len) {
    if (!data || len == 0) return;

    {
        std::lock_guard<std::mutex> lock(m_fifoMutex);
        if (m_rxFifo.size() + len > MAX_FIFO_SIZE) {
            size_t drop = (m_rxFifo.size() + len) - MAX_FIFO_SIZE;
            m_rxFifo.erase(m_rxFifo.begin(), m_rxFifo.begin() + drop);
        }
        m_rxFifo.insert(m_rxFifo.end(), data, data + len);
    }

    if (m_onDataAvailable) {
        m_onDataAvailable();
    }
}

void SshStreamAdapter::resizePty(uint16_t cols, uint16_t rows) {
    cbdos::ssh::resizePty(cols, rows);
}

} // namespace terminal
} // namespace cbdos
