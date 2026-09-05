#include "cbdos/ssh.hpp"
#include <libssh/libssh.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>
#include <vector>

static const char* TAG = "HAL_SSH_P4";

namespace cbdos {
namespace bsp {

class P4SshClient : public cbdos::ssh::ISshClient {
public:
    P4SshClient() : m_session(nullptr), m_shellChannel(nullptr), m_shellTaskHandle(nullptr), m_stopShell(false) {}

    ~P4SshClient() override {
        disconnect();
    }

    bool connect(const cbdos::ssh::SshConfig& config, cbdos::ssh::SshStateCallback onStateChanged) override {
        disconnect();

        if (onStateChanged) {
            onStateChanged(cbdos::ssh::SshSessionState::ConnectingTcp, "Iniciando conexión SSH...");
        }

        m_session = ssh_new();
        if (!m_session) {
            ESP_LOGE(TAG, "Fallo al asignar sesión SSH");
            if (onStateChanged) {
                onStateChanged(cbdos::ssh::SshSessionState::ErrorSocket, "Fallo al asignar memoria para sesión SSH");
            }
            return false;
        }

        ssh_options_set(m_session, SSH_OPTIONS_HOST, config.host.c_str());
        int port = config.port;
        ssh_options_set(m_session, SSH_OPTIONS_PORT, &port);
        ssh_options_set(m_session, SSH_OPTIONS_USER, config.username.c_str());
        long timeoutSec = (config.timeoutMs + 999) / 1000;
        ssh_options_set(m_session, SSH_OPTIONS_TIMEOUT, &timeoutSec);

        ESP_LOGI(TAG, "Conectando a %s:%d (usuario: %s)", config.host.c_str(), config.port, config.username.c_str());
        int rc = ssh_connect(m_session);
        if (rc != SSH_OK) {
            std::string err = ssh_get_error(m_session);
            ESP_LOGE(TAG, "Error en ssh_connect: %s", err.c_str());
            if (onStateChanged) {
                onStateChanged(cbdos::ssh::SshSessionState::ErrorSocket, err);
            }
            ssh_free(m_session);
            m_session = nullptr;
            return false;
        }

        if (onStateChanged) {
            onStateChanged(cbdos::ssh::SshSessionState::Authenticating, "Autenticando credenciales...");
        }

        if (config.authType == cbdos::ssh::SshAuthType::Password) {
            rc = ssh_userauth_password(m_session, nullptr, config.password.c_str());
            if (rc != SSH_AUTH_SUCCESS) {
                std::string err = ssh_get_error(m_session);
                ESP_LOGE(TAG, "Fallo autenticación por contraseña: %s", err.c_str());
                if (onStateChanged) {
                    onStateChanged(cbdos::ssh::SshSessionState::ErrorAuthFailed, err);
                }
                disconnect();
                return false;
            }
        } else if (config.authType == cbdos::ssh::SshAuthType::PublicKey) {
            ssh_key privkey = nullptr;
            rc = ssh_pki_import_privkey_file(config.privateKeyPath.c_str(),
                                             config.passphrase.empty() ? nullptr : config.passphrase.c_str(),
                                             nullptr, nullptr, &privkey);
            if (rc == SSH_OK && privkey) {
                rc = ssh_userauth_publickey(m_session, nullptr, privkey);
                ssh_key_free(privkey);
                if (rc != SSH_AUTH_SUCCESS) {
                    std::string err = ssh_get_error(m_session);
                    ESP_LOGE(TAG, "Fallo autenticación con clave pública: %s", err.c_str());
                    if (onStateChanged) {
                        onStateChanged(cbdos::ssh::SshSessionState::ErrorAuthFailed, err);
                    }
                    disconnect();
                    return false;
                }
            } else {
                std::string err = "No se pudo leer o descifrar clave privada desde MicroSD: " + config.privateKeyPath;
                ESP_LOGE(TAG, "%s", err.c_str());
                if (onStateChanged) {
                    onStateChanged(cbdos::ssh::SshSessionState::ErrorAuthFailed, err);
                }
                disconnect();
                return false;
            }
        }

        ESP_LOGI(TAG, "Sesión SSH autenticada con éxito con %s", config.host.c_str());
        if (onStateChanged) {
            onStateChanged(cbdos::ssh::SshSessionState::Ready, "Conexión SSH establecida y autenticada");
        }
        return true;
    }

    void disconnect() override {
        closeShell();
        if (m_session) {
            ESP_LOGI(TAG, "Cerrando sesión SSH");
            ssh_disconnect(m_session);
            ssh_free(m_session);
            m_session = nullptr;
        }
    }

    bool isConnected() const override {
        return (m_session != nullptr && ssh_is_connected(m_session));
    }

    cbdos::ssh::SshExecResult execute(const std::string& command, uint32_t timeoutMs) override {
        cbdos::ssh::SshExecResult result;
        if (!isConnected()) {
            result.success = false;
            result.exitCode = -1;
            result.errorMessage = "Sesión SSH no conectada";
            return result;
        }

        ssh_channel channel = ssh_channel_new(m_session);
        if (!channel) {
            result.success = false;
            result.errorMessage = "Error al crear canal SSH: " + std::string(ssh_get_error(m_session));
            return result;
        }

        if (ssh_channel_open_session(channel) != SSH_OK) {
            result.success = false;
            result.errorMessage = "Error al abrir sesión de canal: " + std::string(ssh_get_error(m_session));
            ssh_channel_free(channel);
            return result;
        }

        if (ssh_channel_request_exec(channel, command.c_str()) != SSH_OK) {
            result.success = false;
            result.errorMessage = "Error al ejecutar comando remoto: " + std::string(ssh_get_error(m_session));
            ssh_channel_close(channel);
            ssh_channel_free(channel);
            return result;
        }

        char buffer[256];
        int nbytes = 0;
        while ((nbytes = ssh_channel_read(channel, buffer, sizeof(buffer) - 1, 0)) > 0) {
            result.stdOut.append(buffer, nbytes);
        }

        while ((nbytes = ssh_channel_read(channel, buffer, sizeof(buffer) - 1, 1)) > 0) {
            result.stdErr.append(buffer, nbytes);
        }

        result.exitCode = ssh_channel_get_exit_status(channel);
        result.success = (nbytes >= 0);

        ssh_channel_send_eof(channel);
        ssh_channel_close(channel);
        ssh_channel_free(channel);

        return result;
    }

    bool openShell(cbdos::ssh::SshDataCallback onData, uint16_t cols, uint16_t rows) override {
        if (!isConnected()) {
            return false;
        }

        closeShell();

        m_shellChannel = ssh_channel_new(m_session);
        if (!m_shellChannel) {
            return false;
        }

        if (ssh_channel_open_session(m_shellChannel) != SSH_OK) {
            ssh_channel_free(m_shellChannel);
            m_shellChannel = nullptr;
            return false;
        }

        if (ssh_channel_request_pty_size(m_shellChannel, "xterm-256color", cols, rows) != SSH_OK) {
            ssh_channel_close(m_shellChannel);
            ssh_channel_free(m_shellChannel);
            m_shellChannel = nullptr;
            return false;
        }

        if (ssh_channel_request_shell(m_shellChannel) != SSH_OK) {
            ssh_channel_close(m_shellChannel);
            ssh_channel_free(m_shellChannel);
            m_shellChannel = nullptr;
            return false;
        }

        m_onData = onData;
        m_stopShell = false;

        xTaskCreatePinnedToCore([](void* arg) {
            P4SshClient* self = static_cast<P4SshClient*>(arg);
            char buf[256];
            while (!self->m_stopShell && self->m_shellChannel && ssh_channel_is_open(self->m_shellChannel)) {
                int nbytes = ssh_channel_read_nonblocking(self->m_shellChannel, buf, sizeof(buf), 0);
                if (nbytes > 0 && self->m_onData) {
                    self->m_onData(reinterpret_cast<const uint8_t*>(buf), nbytes);
                } else if (nbytes < 0) {
                    break;
                }
                vTaskDelay(pdMS_TO_TICKS(15));
            }
            self->m_shellTaskHandle = nullptr;
            vTaskDelete(nullptr);
        }, "ssh_shell_rx", 4096, this, 5, &m_shellTaskHandle, 1);

        return true;
    }

    bool sendInput(const uint8_t* buffer, size_t length) override {
        if (!m_shellChannel || !ssh_channel_is_open(m_shellChannel)) {
            return false;
        }
        int written = ssh_channel_write(m_shellChannel, buffer, length);
        return (written == static_cast<int>(length));
    }

    void resizePty(uint16_t cols, uint16_t rows) override {
        if (m_shellChannel && ssh_channel_is_open(m_shellChannel)) {
            ssh_channel_change_pty_size(m_shellChannel, cols, rows);
        }
    }

    void closeShell() override {
        m_stopShell = true;
        if (m_shellTaskHandle) {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        if (m_shellChannel) {
            ssh_channel_close(m_shellChannel);
            ssh_channel_free(m_shellChannel);
            m_shellChannel = nullptr;
        }
        m_onData = nullptr;
    }

private:
    ssh_session m_session;
    ssh_channel m_shellChannel;
    cbdos::ssh::SshDataCallback m_onData;
    TaskHandle_t m_shellTaskHandle;
    volatile bool m_stopShell;
};

static P4SshClient s_p4SshClient;

void initSshBackendP4() {
    cbdos::ssh::setSshClient(&s_p4SshClient);
    ESP_LOGI(TAG, "HAL SSH P4 inicializado");
}

} // namespace bsp
} // namespace cbdos
