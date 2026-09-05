#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <functional>
#include <memory>

namespace cbdos {
namespace ssh {

enum class SshAuthType {
    Password,
    PublicKey
};

enum class SshSessionState {
    Disconnected,
    ConnectingTcp,
    KeyExchange,
    Authenticating,
    Ready,
    ErrorAuthFailed,
    ErrorTimeout,
    ErrorSocket
};

struct SshConfig {
    std::string host;
    uint16_t port{22};
    std::string username;
    SshAuthType authType{SshAuthType::Password};
    std::string password;              // Usado si authType == Password
    std::string privateKeyPath;        // Ruta en MicroSD (ej. "/sdcard/keys/id_ed25519")
    std::string passphrase;            // Opcional para clave privada protegida
    uint32_t timeoutMs{8000};
    std::string termType{"vt100"};     // "vt100" (máxima compatibilidad) o "xterm"
};

struct SshExecResult {
    bool success{false};
    int exitCode{-1};
    std::string stdOut;
    std::string stdErr;
    std::string errorMessage;
};

using SshDataCallback = std::function<void(const uint8_t* data, size_t len)>;
using SshStateCallback = std::function<void(SshSessionState state, const std::string& msg)>;

class ISshClient {
public:
    virtual ~ISshClient() = default;

    // Control del ciclo de vida de la conexión
    virtual bool connect(const SshConfig& config, SshStateCallback onStateChanged = nullptr) = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;

    // Modo 1: Automatización / Scripting (Comando único no interactivo)
    virtual SshExecResult execute(const std::string& command, uint32_t timeoutMs = 10000) = 0;

    // Modo 2: Consola Interactiva (Pseudo-terminal PTY / VT100 / xterm)
    virtual bool openShell(SshDataCallback onData, uint16_t cols = 80, uint16_t rows = 24, const std::string& termType = "vt100") = 0;
    virtual bool sendInput(const uint8_t* buffer, size_t length) = 0;
    virtual void resizePty(uint16_t cols, uint16_t rows) = 0;
    virtual void closeShell() = 0;
};

// Inyección de dependencias HAL
void setSshClient(ISshClient* client);
ISshClient* getSshClient();

// Funciones helpers de conveniencia global para Core y Apps
bool connect(const SshConfig& config, SshStateCallback onStateChanged = nullptr);
void disconnect();
bool isConnected();
SshExecResult execute(const std::string& command, uint32_t timeoutMs = 10000);
bool openShell(SshDataCallback onData, uint16_t cols = 80, uint16_t rows = 24, const std::string& termType = "vt100");
bool sendInput(const uint8_t* buffer, size_t length);
void resizePty(uint16_t cols, uint16_t rows);
void closeShell();

} // namespace ssh
} // namespace cbdos
