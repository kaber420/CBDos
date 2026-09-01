#include "cbdos/radio.hpp"
#include "cbdos/network.hpp"
#include "cbdos/network_interface.hpp"
#include "cbdos/mesh/mesh_engine.hpp"
#include "cbdos/system.hpp"
#include "cbdos/config_manager.hpp"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <vector>
#include <cstring>

static const char* TAG_RADIO = "HAL_RADIO_S3";

namespace cbdos {
namespace bsp {

namespace {

struct RadioStateS3 {
    bool powered = true;
    radio::RadioMode mode = radio::RadioMode::EspNow;
    uint8_t channel = 13;
    int8_t txPower = 20;

    bool wifiScanning = false;
    radio::WifiScanCallback wifiScanCb = nullptr;

    bool channelSweeping = false;
    uint8_t sweepCurrentChannel = 1;
    radio::ChannelSweepCallback sweepCb = nullptr;
    std::vector<radio::DiscoveredNode> sweepNodes;
    uint32_t sweepStepStartTime = 0;
};

static RadioStateS3 s_radio;
static TaskHandle_t s_scanTaskHandle = nullptr;

void ensureWifiStarted() {
    if (WiFi.getMode() == WIFI_OFF) {
        WiFi.mode(WIFI_STA);
    }
}

void applyRadioModeConfig() {
    if (!s_radio.powered || s_radio.mode == radio::RadioMode::Off) {
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        esp_wifi_stop();
        system::log(system::LogLevel::Info, TAG_RADIO, "Radio 2.4 GHz apagada (OFF)");
        return;
    }

    ensureWifiStarted();
    esp_wifi_start();

    // Configurar potencia TX (rango en esp_wifi es de 8 a 84 en cuartos de dBm: 20dBm = 80-84)
    int8_t mappedPower = s_radio.txPower * 4;
    if (mappedPower > 84) mappedPower = 84;
    if (mappedPower < 8) mappedPower = 8;
    esp_wifi_set_max_tx_power(mappedPower);

    // Ajustar canal
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(s_radio.channel, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);

    if (s_radio.mode == radio::RadioMode::WifiSta) {
        esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
        mesh::MeshEngine::getInstance().setRadioMode(mesh::RadioMode::WifiIp);
        system::log(system::LogLevel::Info, TAG_RADIO, "Radio en Modo Wi-Fi STA (Canal %u, TX %d dBm)", s_radio.channel, s_radio.txPower);
    } else if (s_radio.mode == radio::RadioMode::EspNowLR) {
        esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR);
        mesh::MeshEngine::getInstance().setRadioMode(mesh::RadioMode::EspNowLR);
        mesh::MeshEngine::getInstance().init(s_radio.channel);
        system::log(system::LogLevel::Info, TAG_RADIO, "Radio en Modo ESP-NOW Long Range (LR) (Canal %u, TX +20 dBm)", s_radio.channel);
    } else if (s_radio.mode == radio::RadioMode::EspNow) {
        esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
        mesh::MeshEngine::getInstance().setRadioMode(mesh::RadioMode::EspNowNormal);
        mesh::MeshEngine::getInstance().init(s_radio.channel);
        system::log(system::LogLevel::Info, TAG_RADIO, "Radio en Modo ESP-NOW Normal (Canal %u, TX %d dBm)", s_radio.channel, s_radio.txPower);
    } else if (s_radio.mode == radio::RadioMode::Hybrid) {
        esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR);
        mesh::MeshEngine::getInstance().setRadioMode(mesh::RadioMode::EspNowNormal);
        mesh::MeshEngine::getInstance().init(s_radio.channel);
        system::log(system::LogLevel::Info, TAG_RADIO, "Radio en Modo Hibrido Wi-Fi + ESP-NOW (Canal %u)", s_radio.channel);
    }
}

// ─── Tarea de Escaneo Asíncrono en FreeRTOS ───
static void radioScanWorkerTask(void* param) {
    while (true) {
        // 1. Escaneo Wi-Fi
        if (s_radio.wifiScanning) {
            ensureWifiStarted();
            int16_t n = WiFi.scanNetworks(false, true); // show_hidden = false, passive = false
            std::vector<radio::WifiApInfo> apList;
            if (n >= 0) {
                for (int16_t i = 0; i < n; ++i) {
                    radio::WifiApInfo ap;
                    ap.ssid = WiFi.SSID(i).c_str();
                    ap.rssi = static_cast<int8_t>(WiFi.RSSI(i));
                    ap.channel = static_cast<uint8_t>(WiFi.channel(i));
                    ap.isEncrypted = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
                    ap.bssid = WiFi.BSSIDstr(i).c_str();
                    apList.push_back(ap);
                }
                WiFi.scanDelete();
            }
            s_radio.wifiScanning = false;
            if (s_radio.wifiScanCb) {
                s_radio.wifiScanCb(apList, (n >= 0));
            }
        }

        // 2. Barrido Multicanal ESP-NOW / LR (Channel Hopping 1..13)
        if (s_radio.channelSweeping) {
            ensureWifiStarted();
            s_radio.sweepNodes.clear();
            mesh::MeshEngine::getInstance().clearDiscoveredTowers();
            uint8_t originalCh = s_radio.channel;

            for (uint8_t ch = 1; ch <= 13 && s_radio.channelSweeping; ch++) {
                s_radio.sweepCurrentChannel = ch;
                
                // Ajustar canal temporal
                s_radio.channel = ch;
                if (s_radio.powered && s_radio.mode != radio::RadioMode::Off) {
                    esp_wifi_set_promiscuous(true);
                    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
                    esp_wifi_set_promiscuous(false);
                    mesh::MeshEngine::getInstance().setChannel(ch);
                }

                // Enviar sondeo broadcast en este canal
                mesh::MeshEngine::getInstance().sendTowerProbe();

                // Dwell time: escuchar durante 75 ms
                vTaskDelay(pdMS_TO_TICKS(75));

                // Recopilar nodos detectados en este canal
                const auto& found = mesh::MeshEngine::getInstance().getDiscoveredTowers();
                for (const auto& t : found) {
                    radio::DiscoveredNode n;
                    memcpy(n.mac, t.mac, 6);
                    n.short_id = t.short_id;
                    n.channel = (t.channel >= 1 && t.channel <= 13) ? t.channel : ch;
                    n.rssi = t.rssi;
                    n.supported_modes = t.supported_modes;
                    strncpy(n.name, t.name, sizeof(n.name) - 1);

                    // Evitar duplicados por MAC
                    bool exists = false;
                    for (auto& ex : s_radio.sweepNodes) {
                        if (memcmp(ex.mac, n.mac, 6) == 0) {
                            ex = n;
                            exists = true;
                            break;
                        }
                    }
                    if (!exists) {
                        s_radio.sweepNodes.push_back(n);
                    }
                }

                // Notificar progreso del barrido
                if (s_radio.sweepCb) {
                    s_radio.sweepCb(ch, 13, s_radio.sweepNodes, (ch == 13));
                }
            }

            // Restaurar canal original si terminó normalmente
            s_radio.channel = originalCh;
            if (s_radio.powered && s_radio.mode != radio::RadioMode::Off) {
                esp_wifi_set_promiscuous(true);
                esp_wifi_set_channel(originalCh, WIFI_SECOND_CHAN_NONE);
                esp_wifi_set_promiscuous(false);
                mesh::MeshEngine::getInstance().setChannel(originalCh);
            }
            s_radio.channelSweeping = false;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

} // anonymous namespace

class S3RadioBackend : public radio::IRadioBackend {
public:
    ~S3RadioBackend() override = default;

    bool init(const radio::RadioConfig& cfg) override {
        s_radio.powered = cfg.enabled;
        s_radio.mode = cfg.mode;
        s_radio.channel = (cfg.channel >= 1 && cfg.channel <= 13) ? cfg.channel : 1;
        s_radio.txPower = (cfg.txPower >= 2 && cfg.txPower <= 20) ? cfg.txPower : 20;

        system::log(system::LogLevel::Info, TAG_RADIO,
                    "Iniciando Radio S3: Enabled=%s, Mode=%d, Canal=%u, TX=%d dBm",
                    s_radio.powered ? "SI" : "NO", (int)s_radio.mode, s_radio.channel, s_radio.txPower);

        applyRadioModeConfig();
        return true;
    }

    bool setPower(bool on) override {
        s_radio.powered = on;
        if (!on) {
            s_radio.mode = radio::RadioMode::Off;
        } else if (s_radio.mode == radio::RadioMode::Off) {
            s_radio.mode = radio::RadioMode::EspNow;
        }
        applyRadioModeConfig();
        return true;
    }

    bool isPowered() const override {
        return s_radio.powered && (s_radio.mode != radio::RadioMode::Off);
    }

    bool setMode(radio::RadioMode mode) override {
        s_radio.mode = mode;
        s_radio.powered = (mode != radio::RadioMode::Off);
        applyRadioModeConfig();
        return true;
    }

    radio::RadioMode getMode() const override {
        return s_radio.mode;
    }

    bool setChannel(uint8_t channel) override {
        if (channel < 1 || channel > 13) return false;
        s_radio.channel = channel;
        if (s_radio.powered && s_radio.mode != radio::RadioMode::Off) {
            esp_wifi_set_promiscuous(true);
            esp_err_t err = esp_wifi_set_channel(s_radio.channel, WIFI_SECOND_CHAN_NONE);
            esp_wifi_set_promiscuous(false);
            mesh::MeshEngine::getInstance().setChannel(channel);
            return (err == ESP_OK);
        }
        return true;
    }

    uint8_t getChannel() const override {
        return s_radio.channel;
    }

    bool setTxPower(int8_t dbm) override {
        if (dbm < 2 || dbm > 20) return false;
        s_radio.txPower = dbm;
        if (s_radio.powered && s_radio.mode != radio::RadioMode::Off) {
            esp_wifi_set_max_tx_power(dbm * 4);
        }
        return true;
    }

    int8_t getTxPower() const override {
        return s_radio.txPower;
    }

    bool startWifiScan(radio::WifiScanCallback cb) override {
        if (!s_radio.powered || s_radio.mode == radio::RadioMode::Off) {
            setPower(true);
        }
        s_radio.wifiScanCb = cb;
        s_radio.wifiScanning = true;

        if (!s_scanTaskHandle) {
            xTaskCreatePinnedToCore(radioScanWorkerTask, "RadioScanTask", 4096, nullptr, 1, &s_scanTaskHandle, 0);
        }
        return true;
    }

    bool startChannelSweep(radio::ChannelSweepCallback cb) override {
        if (!s_radio.powered || s_radio.mode == radio::RadioMode::Off) {
            setPower(true);
        }
        s_radio.sweepCb = cb;
        s_radio.channelSweeping = true;

        if (!s_scanTaskHandle) {
            xTaskCreatePinnedToCore(radioScanWorkerTask, "RadioScanTask", 4096, nullptr, 1, &s_scanTaskHandle, 0);
        }
        return true;
    }

    void stopScan() override {
        s_radio.wifiScanning = false;
        s_radio.channelSweeping = false;
    }
};

class S3NetworkInterface : public network::INetworkInterface {
public:
    const char* getName() const override {
        return "S3-Radio (ESP-NOW/Wi-Fi)";
    }

    network::InterfaceType getType() const override {
        if (m_mode == network::InterfaceMode::WifiStation || m_mode == network::InterfaceMode::WifiAccessPoint) {
            return network::InterfaceType::IpNetwork;
        } else if (m_mode == network::InterfaceMode::BleGattServer) {
            return network::InterfaceType::BluetoothLe;
        }
        return network::InterfaceType::RadioPacket;
    }

    network::InterfaceMode getMode() const override {
        return m_mode;
    }

    bool setMode(network::InterfaceMode mode) override {
        m_mode = mode;
        if (mode == network::InterfaceMode::Off) {
            s_radio.powered = false;
            s_radio.mode = radio::RadioMode::Off;
        } else if (mode == network::InterfaceMode::EspNow) {
            s_radio.powered = true;
            s_radio.mode = radio::RadioMode::EspNow;
        } else if (mode == network::InterfaceMode::EspNowLR) {
            s_radio.powered = true;
            s_radio.mode = radio::RadioMode::EspNowLR;
        } else if (mode == network::InterfaceMode::WifiStation) {
            s_radio.powered = true;
            s_radio.mode = radio::RadioMode::WifiSta;
        }
        applyRadioModeConfig();
        return true;
    }

    bool isReady() const override {
        return s_radio.powered && (m_mode != network::InterfaceMode::Off);
    }

    int sendPacket(const uint8_t* buffer, size_t len) override {
        if (!isReady() || !buffer || len == 0) return -1;
        uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        if (!esp_now_is_peer_exist(broadcastMac)) {
            esp_now_peer_info_t peerInfo = {};
            memset(&peerInfo, 0, sizeof(peerInfo));
            memcpy(peerInfo.peer_addr, broadcastMac, 6);
            peerInfo.channel = 0; // 0 = Sigue dinámicamente el canal Wi-Fi activo
            peerInfo.ifidx = WIFI_IF_STA;
            peerInfo.encrypt = false;
            esp_now_add_peer(&peerInfo);
        }
        esp_err_t err = esp_now_send(broadcastMac, buffer, len);
        return (err == ESP_OK) ? static_cast<int>(len) : -1;
    }

    void setPacketRecvCallback(network::PacketRecvCallback cb, void* userCtx) override {
        m_recvCb = cb;
        m_recvCtx = userCtx;
        if (cb) {
            s_activeNetInterface = this;
            esp_now_register_recv_cb(S3NetworkInterface::onEspNowRawRecv);
        }
    }

    uint8_t getChannel() const override {
        return s_radio.channel;
    }

    bool setChannel(uint8_t channel) override {
        if (channel < 1 || channel > 13) return false;
        s_radio.channel = channel;
        if (s_radio.powered && s_radio.mode != radio::RadioMode::Off) {
            esp_wifi_set_promiscuous(true);
            esp_wifi_set_channel(s_radio.channel, WIFI_SECOND_CHAN_NONE);
            esp_wifi_set_promiscuous(false);
        }
        return true;
    }

    bool getMacAddress(uint8_t out_mac[6]) override {
        if (!out_mac) return false;
        return (esp_wifi_get_mac(WIFI_IF_STA, out_mac) == ESP_OK);
    }

    static void onEspNowRawRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
        if (!s_activeNetInterface || !s_activeNetInterface->m_recvCb || !data || len <= 0) return;
        int8_t rssi = -50;
        if (info && info->rx_ctrl) {
            rssi = info->rx_ctrl->rssi;
        }
        s_activeNetInterface->m_recvCb(data, static_cast<size_t>(len), rssi, 0, s_activeNetInterface->m_recvCtx);
    }

private:
    network::InterfaceMode m_mode = network::InterfaceMode::EspNow;
    network::PacketRecvCallback m_recvCb = nullptr;
    void* m_recvCtx = nullptr;
    static S3NetworkInterface* s_activeNetInterface;
};

S3NetworkInterface* S3NetworkInterface::s_activeNetInterface = nullptr;
static S3RadioBackend s_s3RadioBackend;
static S3NetworkInterface s_s3NetInterface;

void initRadioBackendS3() {
    radio::setRadioBackend(&s_s3RadioBackend);
    network::NetworkInterfaceManager::getInstance().registerInterface(0, &s_s3NetInterface);
    Serial.println("[RADIO_S3] S3 Radio Backend e INetworkInterface (Slot 0) inicializados e inyectados.");
}

} // namespace bsp
} // namespace cbdos


