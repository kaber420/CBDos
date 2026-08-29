#include "cbdos/radio.hpp"
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

namespace {

struct RadioStateS3 {
    bool powered = true;
    cbdos::radio::RadioMode mode = cbdos::radio::RadioMode::EspNow;
    uint8_t channel = 1;
    int8_t txPower = 20;

    bool wifiScanning = false;
    cbdos::radio::WifiScanCallback wifiScanCb = nullptr;

    bool channelSweeping = false;
    uint8_t sweepCurrentChannel = 1;
    cbdos::radio::ChannelSweepCallback sweepCb = nullptr;
    std::vector<cbdos::radio::DiscoveredNode> sweepNodes;
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
    if (!s_radio.powered || s_radio.mode == cbdos::radio::RadioMode::Off) {
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        esp_wifi_stop();
        cbdos::system::log(cbdos::system::LogLevel::Info, TAG_RADIO, "Radio 2.4 GHz apagada (OFF)");
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

    if (s_radio.mode == cbdos::radio::RadioMode::WifiSta) {
        esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
        cbdos::mesh::MeshEngine::getInstance().setRadioMode(cbdos::mesh::RadioMode::WifiIp);
        cbdos::system::log(cbdos::system::LogLevel::Info, TAG_RADIO, "Radio en Modo Wi-Fi STA (Canal %u, TX %d dBm)", s_radio.channel, s_radio.txPower);
    } else if (s_radio.mode == cbdos::radio::RadioMode::EspNowLR) {
        esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR);
        cbdos::mesh::MeshEngine::getInstance().setRadioMode(cbdos::mesh::RadioMode::EspNowLR);
        cbdos::mesh::MeshEngine::getInstance().init(s_radio.channel);
        cbdos::system::log(cbdos::system::LogLevel::Info, TAG_RADIO, "Radio en Modo ESP-NOW Long Range (LR) (Canal %u, TX +20 dBm)", s_radio.channel);
    } else if (s_radio.mode == cbdos::radio::RadioMode::EspNow) {
        esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
        cbdos::mesh::MeshEngine::getInstance().setRadioMode(cbdos::mesh::RadioMode::EspNowNormal);
        cbdos::mesh::MeshEngine::getInstance().init(s_radio.channel);
        cbdos::system::log(cbdos::system::LogLevel::Info, TAG_RADIO, "Radio en Modo ESP-NOW Normal (Canal %u, TX %d dBm)", s_radio.channel, s_radio.txPower);
    } else if (s_radio.mode == cbdos::radio::RadioMode::Hybrid) {
        esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR);
        cbdos::mesh::MeshEngine::getInstance().setRadioMode(cbdos::mesh::RadioMode::EspNowNormal);
        cbdos::mesh::MeshEngine::getInstance().init(s_radio.channel);
        cbdos::system::log(cbdos::system::LogLevel::Info, TAG_RADIO, "Radio en Modo Hibrido Wi-Fi + ESP-NOW (Canal %u)", s_radio.channel);
    }
}

} // anonymous namespace

namespace cbdos {
namespace radio {

bool init() {
    RadioConfig cfg;
    ConfigManager::getInstance().loadRadio(cfg);
    s_radio.powered = cfg.enabled;
    s_radio.mode = cfg.mode;
    s_radio.channel = (cfg.channel >= 1 && cfg.channel <= 13) ? cfg.channel : 1;
    s_radio.txPower = (cfg.txPower >= 2 && cfg.txPower <= 20) ? cfg.txPower : 20;

    cbdos::system::log(cbdos::system::LogLevel::Info, TAG_RADIO,
                       "Iniciando Radio desde NVS: Enabled=%s, Mode=%s, Canal=%u, TX=%d dBm",
                       s_radio.powered ? "SI" : "NO", getModeName(s_radio.mode), s_radio.channel, s_radio.txPower);

    applyRadioModeConfig();
    return true;
}

bool isRadioPowered() {
    return s_radio.powered && (s_radio.mode != RadioMode::Off);
}

void setRadioPower(bool on) {
    s_radio.powered = on;
    if (!on) {
        s_radio.mode = RadioMode::Off;
    } else if (s_radio.mode == RadioMode::Off) {
        s_radio.mode = RadioMode::EspNow;
    }
    applyRadioModeConfig();
}

bool setMode(RadioMode mode) {
    s_radio.mode = mode;
    s_radio.powered = (mode != RadioMode::Off);
    applyRadioModeConfig();
    return true;
}

RadioMode getMode() {
    return s_radio.mode;
}

const char* getModeName(RadioMode mode) {
    switch (mode) {
        case RadioMode::Off: return "Apagada (OFF)";
        case RadioMode::WifiSta: return "Wi-Fi";
        case RadioMode::EspNow: return "ESP-NOW Normal (1 Mbps)";
        case RadioMode::EspNowLR: return "ESP-NOW LR (Long Range)";
        case RadioMode::Hybrid: return "Hibrido (Wi-Fi + ESP-NOW)";
        default: return "Desconocido";
    }
}

uint8_t getChannel() {
    return s_radio.channel;
}

bool setChannel(uint8_t channel) {
    if (channel < 1 || channel > 13) return false;
    s_radio.channel = channel;
    if (s_radio.powered && s_radio.mode != RadioMode::Off) {
        esp_wifi_set_promiscuous(true);
        esp_err_t err = esp_wifi_set_channel(s_radio.channel, WIFI_SECOND_CHAN_NONE);
        esp_wifi_set_promiscuous(false);
        cbdos::mesh::MeshEngine::getInstance().setChannel(channel);
        return (err == ESP_OK);
    }
    return true;
}

int8_t getTxPower() {
    return s_radio.txPower;
}

bool setTxPower(int8_t dbm) {
    if (dbm < 2 || dbm > 20) return false;
    s_radio.txPower = dbm;
    if (s_radio.powered && s_radio.mode != RadioMode::Off) {
        esp_wifi_set_max_tx_power(dbm * 4);
    }
    return true;
}

// ─── Tarea de Escaneo Asíncrono en FreeRTOS ───
static void radioScanWorkerTask(void* param) {
    while (true) {
        // 1. Escaneo Wi-Fi
        if (s_radio.wifiScanning) {
            ensureWifiStarted();
            int16_t n = WiFi.scanNetworks(false, true); // show_hidden = false, passive = false
            std::vector<WifiApInfo> apList;
            if (n >= 0) {
                for (int16_t i = 0; i < n; ++i) {
                    WifiApInfo ap;
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
            cbdos::mesh::MeshEngine::getInstance().clearDiscoveredTowers();
            uint8_t originalCh = s_radio.channel;

            for (uint8_t ch = 1; ch <= 13 && s_radio.channelSweeping; ch++) {
                s_radio.sweepCurrentChannel = ch;
                setChannel(ch);

                // Enviar sondeo broadcast en este canal
                cbdos::mesh::MeshEngine::getInstance().sendTowerProbe();

                // Dwell time: escuchar durante 75 ms
                vTaskDelay(pdMS_TO_TICKS(75));

                // Recopilar nodos detectados en este canal
                const auto& found = cbdos::mesh::MeshEngine::getInstance().getDiscoveredTowers();
                for (const auto& t : found) {
                    DiscoveredNode n;
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
            setChannel(originalCh);
            s_radio.channelSweeping = false;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

bool startWifiScan(WifiScanCallback cb) {
    if (!s_radio.powered || s_radio.mode == RadioMode::Off) {
        setRadioPower(true);
    }
    s_radio.wifiScanCb = cb;
    s_radio.wifiScanning = true;

    if (!s_scanTaskHandle) {
        xTaskCreatePinnedToCore(radioScanWorkerTask, "RadioScanTask", 4096, nullptr, 1, &s_scanTaskHandle, 0);
    }
    return true;
}

bool startChannelSweep(ChannelSweepCallback cb) {
    if (!s_radio.powered || s_radio.mode == RadioMode::Off) {
        setRadioPower(true);
    }
    s_radio.sweepCb = cb;
    s_radio.channelSweeping = true;

    if (!s_scanTaskHandle) {
        xTaskCreatePinnedToCore(radioScanWorkerTask, "RadioScanTask", 4096, nullptr, 1, &s_scanTaskHandle, 0);
    }
    return true;
}

void stopScan() {
    s_radio.wifiScanning = false;
    s_radio.channelSweeping = false;
}

} // namespace radio
} // namespace cbdos
