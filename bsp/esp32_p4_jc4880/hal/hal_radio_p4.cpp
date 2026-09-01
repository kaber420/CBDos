#include "cbdos/radio.hpp"
#include "cbdos/network.hpp"
#include "cbdos/network_interface.hpp"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>
#include <vector>

static const char* TAG_RADIO_P4 = "HAL_RADIO_P4";

namespace cbdos {
namespace bsp {

namespace {

struct RadioStateP4 {
    bool powered = true;
    radio::RadioMode mode = radio::RadioMode::EspNow;
    uint8_t channel = 1;
    int8_t txPower = 20;

    bool wifiScanning = false;
    radio::WifiScanCallback wifiScanCb = nullptr;

    bool channelSweeping = false;
    radio::ChannelSweepCallback sweepCb = nullptr;
};

static RadioStateP4 s_radioP4;
static TaskHandle_t s_p4ScanTask = nullptr;

static void radioScanWorkerP4(void* param) {
    while (true) {
        if (s_radioP4.wifiScanning) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            std::vector<radio::WifiApInfo> aps;
            s_radioP4.wifiScanning = false;
            if (s_radioP4.wifiScanCb) {
                s_radioP4.wifiScanCb(aps, true);
            }
        }

        if (s_radioP4.channelSweeping) {
            std::vector<radio::DiscoveredNode> nodes;
            for (uint8_t ch = 1; ch <= 13 && s_radioP4.channelSweeping; ch++) {
                vTaskDelay(pdMS_TO_TICKS(40));
                if (s_radioP4.sweepCb) {
                    s_radioP4.sweepCb(ch, 13, nodes, (ch == 13));
                }
            }
            s_radioP4.channelSweeping = false;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

} // anonymous namespace

class P4RadioBackend : public radio::IRadioBackend {
public:
    ~P4RadioBackend() override = default;

    bool init(const radio::RadioConfig& cfg) override {
        s_radioP4.powered = cfg.enabled;
        s_radioP4.mode = cfg.mode;
        s_radioP4.channel = (cfg.channel >= 1 && cfg.channel <= 13) ? cfg.channel : 1;
        s_radioP4.txPower = (cfg.txPower >= 2 && cfg.txPower <= 20) ? cfg.txPower : 20;

        ESP_LOGI(TAG_RADIO_P4, "Inicializando Radio P4 (Modo: %d, Canal: %u, TX: %d dBm)",
                 (int)s_radioP4.mode, s_radioP4.channel, s_radioP4.txPower);
        return true;
    }

    bool setPower(bool on) override {
        s_radioP4.powered = on;
        if (!on) {
            s_radioP4.mode = radio::RadioMode::Off;
            network::disconnectWifi();
        } else if (s_radioP4.mode == radio::RadioMode::Off) {
            s_radioP4.mode = radio::RadioMode::EspNow;
        }
        ESP_LOGI(TAG_RADIO_P4, "Radio P4 Power: %s", on ? "ON" : "OFF");
        return true;
    }

    bool isPowered() const override {
        return s_radioP4.powered && (s_radioP4.mode != radio::RadioMode::Off);
    }

    bool setMode(radio::RadioMode mode) override {
        s_radioP4.mode = mode;
        s_radioP4.powered = (mode != radio::RadioMode::Off);
        ESP_LOGI(TAG_RADIO_P4, "Radio P4 Mode: %d", (int)mode);
        return true;
    }

    radio::RadioMode getMode() const override {
        return s_radioP4.mode;
    }

    bool setChannel(uint8_t channel) override {
        if (channel < 1 || channel > 13) return false;
        s_radioP4.channel = channel;
        return true;
    }

    uint8_t getChannel() const override {
        return s_radioP4.channel;
    }

    bool setTxPower(int8_t dbm) override {
        if (dbm < 2 || dbm > 20) return false;
        s_radioP4.txPower = dbm;
        return true;
    }

    int8_t getTxPower() const override {
        return s_radioP4.txPower;
    }

    bool startWifiScan(radio::WifiScanCallback cb) override {
        s_radioP4.wifiScanCb = cb;
        s_radioP4.wifiScanning = true;
        if (!s_p4ScanTask) {
            xTaskCreate(radioScanWorkerP4, "RadioScanP4", 4096, nullptr, 1, &s_p4ScanTask);
        }
        return true;
    }

    bool startChannelSweep(radio::ChannelSweepCallback cb) override {
        s_radioP4.sweepCb = cb;
        s_radioP4.channelSweeping = true;
        if (!s_p4ScanTask) {
            xTaskCreate(radioScanWorkerP4, "RadioScanP4", 4096, nullptr, 1, &s_p4ScanTask);
        }
        return true;
    }

    void stopScan() override {
        s_radioP4.wifiScanning = false;
        s_radioP4.channelSweeping = false;
    }
};

class P4NetworkInterface : public network::INetworkInterface {
public:
    const char* getName() const override {
        return "C6-Radio (ESP-NOW/Wi-Fi)";
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
            s_radioP4.powered = false;
            s_radioP4.mode = radio::RadioMode::Off;
        } else if (mode == network::InterfaceMode::EspNow) {
            s_radioP4.powered = true;
            s_radioP4.mode = radio::RadioMode::EspNow;
        } else if (mode == network::InterfaceMode::EspNowLR) {
            s_radioP4.powered = true;
            s_radioP4.mode = radio::RadioMode::EspNowLR;
        } else if (mode == network::InterfaceMode::WifiStation) {
            s_radioP4.powered = true;
            s_radioP4.mode = radio::RadioMode::WifiSta;
        }
        ESP_LOGI(TAG_RADIO_P4, "P4NetworkInterface modo establecido a: %d", (int)mode);
        return true;
    }

    bool isReady() const override {
        return s_radioP4.powered && (m_mode != network::InterfaceMode::Off);
    }

    int sendPacket(const uint8_t* buffer, size_t len) override {
        if (!isReady() || !buffer || len == 0) return -1;
        ESP_LOGD(TAG_RADIO_P4, "P4NetworkInterface transmitiendo %u bytes", (unsigned)len);
        return static_cast<int>(len);
    }

    void setPacketRecvCallback(network::PacketRecvCallback cb, void* userCtx) override {
        m_recvCb = cb;
        m_recvCtx = userCtx;
    }

    uint8_t getChannel() const override {
        return s_radioP4.channel;
    }

    bool setChannel(uint8_t channel) override {
        if (channel < 1 || channel > 13) return false;
        s_radioP4.channel = channel;
        return true;
    }

    bool getMacAddress(uint8_t out_mac[6]) override {
        if (!out_mac) return false;
        // MAC base simulada o leída de eFuse
        out_mac[0] = 0x24; out_mac[1] = 0xDC; out_mac[2] = 0xC3;
        out_mac[3] = 0x4A; out_mac[4] = 0x12; out_mac[5] = 0xF0;
        return true;
    }

private:
    network::InterfaceMode m_mode = network::InterfaceMode::EspNow;
    network::PacketRecvCallback m_recvCb = nullptr;
    void* m_recvCtx = nullptr;
};

static P4RadioBackend s_p4RadioBackend;
static P4NetworkInterface s_p4NetInterface;

void initRadioBackendP4() {
    radio::setRadioBackend(&s_p4RadioBackend);
    network::NetworkInterfaceManager::getInstance().registerInterface(0, &s_p4NetInterface);
    ESP_LOGI(TAG_RADIO_P4, "P4 Radio Backend e INetworkInterface (Slot 0) inicializados e inyectados.");
}

} // namespace bsp
} // namespace cbdos
