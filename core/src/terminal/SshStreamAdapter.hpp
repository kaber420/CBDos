#pragma once
#include "cbdos/terminal_stream.hpp"
#include "cbdos/ssh.hpp"
#include <vector>
#include <mutex>

namespace cbdos {
namespace terminal {

class SshStreamAdapter : public ITerminalStream {
public:
    SshStreamAdapter();
    ~SshStreamAdapter() override;

    StreamType getType() const override { return StreamType::Ssh; }
    const char* getTypeName() const override { return "SSH Remoto"; }

    size_t write(const uint8_t* data, size_t len) override;
    size_t read(uint8_t* buffer, size_t maxLen) override;
    size_t available() override;
    bool isConnected() const override;
    void close() override;

    bool connect(const cbdos::ssh::SshConfig& config, uint16_t cols = 80, uint16_t rows = 24);
    void resizePty(uint16_t cols, uint16_t rows);

private:
    void handleIncomingData(const uint8_t* data, size_t len);

    static constexpr size_t MAX_FIFO_SIZE = 16384;
    std::vector<uint8_t> m_rxFifo;
    mutable std::mutex m_fifoMutex;
};

} // namespace terminal
} // namespace cbdos
