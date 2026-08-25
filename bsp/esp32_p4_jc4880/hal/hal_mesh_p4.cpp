#include "cbdos/mesh/mesh_transport.hpp"
#include "cbdos/mesh/mesh_engine.hpp"
#include "cbdos/network.hpp"
#include <esp_log.h>
#include <cstring>

static const char* TAG = "CBDOS_MESH_P4";

namespace {

class EspNowP4Transport : public cbdos::mesh::IMeshTransport {
public:
    EspNowP4Transport() = default;
    virtual ~EspNowP4Transport() {
        stop();
    }

    bool init(uint8_t channel = 1) override {
        m_channel = channel;
        s_activeInstance = this;
        m_ready = true;
        ESP_LOGI(TAG, "Transporte Mesh P4 inicializado en Canal %u", m_channel);
        return true;
    }

    void stop() override {
        m_ready = false;
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
        ESP_LOGD(TAG, "TX Mesh P4 -> %u bytes", (unsigned int)len);
        return true;
    }

    void setRecvCallback(cbdos::mesh::MeshRawRecvCallback cb) override {
        m_recvCb = cb;
    }

    bool getMacAddress(uint8_t out_mac[6]) override {
        if (!out_mac) return false;
        out_mac[0] = 0xAA;
        out_mac[1] = 0xBB;
        out_mac[2] = 0xCC;
        out_mac[3] = 0xDD;
        out_mac[4] = 0xEE;
        out_mac[5] = 0x01;
        return true;
    }

    uint8_t getChannel() override {
        return m_channel;
    }

    bool setChannel(uint8_t channel) override {
        m_channel = channel;
        return true;
    }

    void injectRxData(const uint8_t* src_mac, const uint8_t* data, size_t len, int8_t rssi) {
        if (m_recvCb) {
            m_recvCb(src_mac, data, len, rssi);
        }
    }

private:
    static EspNowP4Transport* s_activeInstance;
    cbdos::mesh::MeshRawRecvCallback m_recvCb = nullptr;
    uint8_t m_channel = 1;
    bool m_ready = false;
};

EspNowP4Transport* EspNowP4Transport::s_activeInstance = nullptr;
static EspNowP4Transport s_p4Transport;

} // anonymous namespace

namespace cbdos {
namespace bsp {

void initMeshTransportP4() {
    mesh::MeshEngine::getInstance().setTransport(&s_p4Transport);
}

} // namespace bsp
} // namespace cbdos
