#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <functional>

namespace cbdos {
namespace terminal {

enum class StreamType {
    Serial,
    Ssh
};

enum class StreamState {
    Disconnected,
    Connecting,
    Connected,
    Error
};

using DataAvailableCallback = std::function<void()>;
using StateChangedCallback = std::function<void(StreamState state, const std::string& message)>;

class ITerminalStream {
public:
    virtual ~ITerminalStream() = default;

    virtual StreamType getType() const = 0;
    virtual const char* getTypeName() const = 0;

    virtual size_t write(const uint8_t* data, size_t len) = 0;
    virtual size_t read(uint8_t* buffer, size_t maxLen) = 0;
    virtual size_t available() = 0;
    virtual bool isConnected() const = 0;
    virtual void close() = 0;

    virtual void setOnDataAvailable(DataAvailableCallback cb) {
        m_onDataAvailable = cb;
    }

    virtual void setOnStateChanged(StateChangedCallback cb) {
        m_onStateChanged = cb;
    }

protected:
    DataAvailableCallback m_onDataAvailable;
    StateChangedCallback m_onStateChanged;
};

} // namespace terminal
} // namespace cbdos
