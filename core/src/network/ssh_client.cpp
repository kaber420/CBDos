#include "cbdos/ssh.hpp"

namespace cbdos {
namespace ssh {

static ISshClient* s_sshClient = nullptr;

void setSshClient(ISshClient* client) {
    s_sshClient = client;
}

ISshClient* getSshClient() {
    return s_sshClient;
}

bool connect(const SshConfig& config, SshStateCallback onStateChanged) {
    if (s_sshClient) {
        return s_sshClient->connect(config, onStateChanged);
    }
    if (onStateChanged) {
        onStateChanged(SshSessionState::ErrorSocket, "No SSH client backend registered");
    }
    return false;
}

void disconnect() {
    if (s_sshClient) {
        s_sshClient->disconnect();
    }
}

bool isConnected() {
    if (s_sshClient) {
        return s_sshClient->isConnected();
    }
    return false;
}

SshExecResult execute(const std::string& command, uint32_t timeoutMs) {
    if (s_sshClient) {
        return s_sshClient->execute(command, timeoutMs);
    }
    SshExecResult res;
    res.success = false;
    res.exitCode = -1;
    res.errorMessage = "No SSH client backend registered";
    return res;
}

bool openShell(SshDataCallback onData, uint16_t cols, uint16_t rows, const std::string& termType) {
    if (s_sshClient) {
        return s_sshClient->openShell(onData, cols, rows, termType);
    }
    return false;
}

bool sendInput(const uint8_t* buffer, size_t length) {
    if (s_sshClient) {
        return s_sshClient->sendInput(buffer, length);
    }
    return false;
}

void resizePty(uint16_t cols, uint16_t rows) {
    if (s_sshClient) {
        s_sshClient->resizePty(cols, rows);
    }
}

void closeShell() {
    if (s_sshClient) {
        s_sshClient->closeShell();
    }
}

} // namespace ssh
} // namespace cbdos
