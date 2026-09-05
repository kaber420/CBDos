#pragma once
#include "cbdos/terminal_stream.hpp"
#include "cbdos/serial.hpp"

namespace cbdos {
namespace terminal {

class SerialStreamAdapter : public ITerminalStream {
public:
    SerialStreamAdapter() = default;
    ~SerialStreamAdapter() override {
        close();
    }

    StreamType getType() const override { return StreamType::Serial; }
    const char* getTypeName() const override { return "Serial / USB"; }

    size_t write(const uint8_t* data, size_t len) override;
    size_t read(uint8_t* buffer, size_t maxLen) override;
    size_t available() override;
    bool isConnected() const override;
    void close() override;

    bool open(const cbdos::serial::SerialConfig& config);
    bool setBaudrate(uint32_t baudrate);
    bool pulseControlPin(uint32_t durationMs = 100, bool enterBootloader = false);
};

} // namespace terminal
} // namespace cbdos
