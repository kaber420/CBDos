#include "cbdos/mesh/mesh_transport.hpp"
#include "cbdos/mesh/mesh_engine.hpp"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <cstring>

namespace {

class EspNowS3Transport : public cbdos::mesh::IMeshTransport {
public:
    EspNowS3Transport() = default;
    virtual ~EspNowS3Transport() {
        stop();
    }

    bool init(uint8_t channel = 1) override {
        m_channel = channel;

        // Asegurar que WiFi esté activo en modo STA sin conectar a router
        if (WiFi.getMode() == WIFI_OFF) {
            WiFi.mode(WIFI_STA);
        }
        WiFi.disconnect();

        // Forzar canal Wi-Fi
        esp_wifi_set_promiscuous(true);
        esp_wifi_set_channel(m_channel, WIFI_SECOND_CHAN_NONE);
        esp_wifi_set_promiscuous(false);

        // Inicializar ESP-NOW
        if (esp_now_init() != ESP_OK) {
            return false;
        }

        s_activeInstance = this;

        // Registrar callback de recepción
        esp_now_register_recv_cb(EspNowS3Transport::onDataRecv);

        // Añadir peer de Broadcast
        esp_now_peer_info_t broadcastPeer = {};
        memset(broadcastPeer.peer_addr, 0xFF, 6);
        broadcastPeer.channel = 0; // 0 = Sigue dinámicamente el canal Wi-Fi activo
        broadcastPeer.ifidx = WIFI_IF_STA;
        broadcastPeer.encrypt = false;
        if (!esp_now_is_peer_exist(broadcastPeer.peer_addr)) {
            esp_now_add_peer(&broadcastPeer);
        }

        m_ready = true;
        return true;
    }

    void stop() override {
        if (m_ready) {
            esp_now_unregister_recv_cb();
            esp_now_deinit();
            m_ready = false;
        }
        if (s_activeInstance == this) {
            s_activeInstance = nullptr;
        }
    }

    bool isReady() const override {
        return m_ready;
    }

    bool sendRaw(const uint8_t* dest_mac, const uint8_t* data, size_t len) override {
        if (!m_ready || !data || len == 0 || len > cbdos::mesh::ESPNOW_MAX_FRAME_SIZE) {
            return false;
        }

        uint8_t target_mac[6];
        if (!dest_mac) {
            memset(target_mac, 0xFF, 6);
        } else {
            memcpy(target_mac, dest_mac, 6);
        }

        // Si es unicast y no está registrado como peer, registrarlo
        if (dest_mac && !esp_now_is_peer_exist(target_mac)) {
            esp_now_peer_info_t peer = {};
            memcpy(peer.peer_addr, target_mac, 6);
            peer.channel = 0; // 0 = Sigue dinámicamente el canal Wi-Fi activo
            peer.ifidx = WIFI_IF_STA;
            peer.encrypt = false;
            esp_now_add_peer(&peer);
        }

        esp_err_t res = esp_now_send(target_mac, data, len);
        return (res == ESP_OK);
    }

    void setRecvCallback(cbdos::mesh::MeshRawRecvCallback cb) override {
        m_recvCb = cb;
    }

    bool getMacAddress(uint8_t out_mac[6]) override {
        if (!out_mac) return false;
        return (esp_wifi_get_mac(WIFI_IF_STA, out_mac) == ESP_OK);
    }

    uint8_t getChannel() override {
        return m_channel;
    }

    bool setChannel(uint8_t channel) override {
        m_channel = channel;
        if (m_ready) {
            esp_wifi_set_promiscuous(true);
            esp_err_t err = esp_wifi_set_channel(m_channel, WIFI_SECOND_CHAN_NONE);
            esp_wifi_set_promiscuous(false);
            return (err == ESP_OK);
        }
        return true;
    }

    bool setRadioMode(cbdos::mesh::RadioMode mode) override {
        m_mode = mode;
        if (!m_ready) {
            init(m_channel);
        }

        if (mode == cbdos::mesh::RadioMode::EspNowLR) {
            esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR);
            esp_wifi_set_max_tx_power(84); // +20dBm
        } else if (mode == cbdos::mesh::RadioMode::EspNowNormal) {
            esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
            esp_wifi_set_max_tx_power(84);
        } else {
            // Auto / Híbrido
            esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR);
            esp_wifi_set_max_tx_power(84);
        }
        return true;
    }

    cbdos::mesh::RadioMode getRadioMode() const override {
        return m_mode;
    }

private:
    static void onDataRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
        if (!s_activeInstance || !s_activeInstance->m_recvCb || !info || !data || len <= 0) return;
        int8_t rssi = -50;
        if (info->rx_ctrl) {
            rssi = info->rx_ctrl->rssi;
        }
        s_activeInstance->m_recvCb(info->src_addr, data, static_cast<size_t>(len), rssi);
    }

    static EspNowS3Transport* s_activeInstance;
    cbdos::mesh::MeshRawRecvCallback m_recvCb = nullptr;
    cbdos::mesh::RadioMode m_mode = cbdos::mesh::RadioMode::Auto;
    uint8_t m_channel = 1;
    bool m_ready = false;
};

EspNowS3Transport* EspNowS3Transport::s_activeInstance = nullptr;
static EspNowS3Transport s_s3Transport;

} // anonymous namespace

// Inicializador estático para conectar el HAL S3 al MeshEngine
namespace cbdos {
namespace bsp {

void initMeshTransportS3() {
    mesh::MeshEngine::getInstance().setTransport(&s_s3Transport);
}

} // namespace bsp
} // namespace cbdos
