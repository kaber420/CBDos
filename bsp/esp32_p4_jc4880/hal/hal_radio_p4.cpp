#include "cbdos/radio.hpp"
#include "cbdos/network.hpp"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>
#include <vector>

static const char* TAG_RADIO_P4 = "HAL_RADIO_P4";

namespace {

struct RadioStateP4 {
    bool powered = true;
    cbdos::radio::RadioMode mode = cbdos::radio::RadioMode::EspNow;
    uint8_t channel = 1;
    int8_t txPower = 20;

    bool wifiScanning = false;
    cbdos::radio::WifiScanCallback wifiScanCb = nullptr;

    bool channelSweeping = false;
    cbdos::radio::ChannelSweepCallback sweepCb = nullptr;
};

static RadioStateP4 s_radioP4;
static TaskHandle_t s_p4ScanTask = nullptr;

} // anonymous namespace

namespace cbdos {
namespace radio {

bool init() {
    ESP_LOGI(TAG_RADIO_P4, "Inicializando Radio P4 (Modo: %s, Canal: %u)", getModeName(s_radioP4.mode), s_radioP4.channel);
    return true;
}

bool isRadioPowered() {
    return s_radioP4.powered && (s_radioP4.mode != RadioMode::Off);
}

void setRadioPower(bool on) {
    s_radioP4.powered = on;
    if (!on) {
        s_radioP4.mode = RadioMode::Off;
        cbdos::network::disconnectWifi();
    } else if (s_radioP4.mode == RadioMode::Off) {
        s_radioP4.mode = RadioMode::EspNow;
    }
    ESP_LOGI(TAG_RADIO_P4, "Radio P4 Power: %s", on ? "ON" : "OFF");
}

bool setMode(RadioMode mode) {
    s_radioP4.mode = mode;
    s_radioP4.powered = (mode != RadioMode::Off);
    ESP_LOGI(TAG_RADIO_P4, "Radio P4 Mode: %s", getModeName(mode));
    return true;
}

RadioMode getMode() {
    return s_radioP4.mode;
}

const char* getModeName(RadioMode mode) {
    switch (mode) {
        case RadioMode::Off: return "Apagada (OFF)";
        case RadioMode::WifiSta: return "Wi-Fi";
        case RadioMode::EspNow: return "ESP-NOW Normal";
        case RadioMode::EspNowLR: return "ESP-NOW LR";
        case RadioMode::Hybrid: return "Hibrido";
        default: return "Desconocido";
    }
}

uint8_t getChannel() {
    return s_radioP4.channel;
}

bool setChannel(uint8_t channel) {
    if (channel < 1 || channel > 13) return false;
    s_radioP4.channel = channel;
    return true;
}

int8_t getTxPower() {
    return s_radioP4.txPower;
}

bool setTxPower(int8_t dbm) {
    if (dbm < 2 || dbm > 20) return false;
    s_radioP4.txPower = dbm;
    return true;
}

static void radioScanWorkerP4(void* param) {
    while (true) {
        if (s_radioP4.wifiScanning) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            std::vector<WifiApInfo> aps;
            s_radioP4.wifiScanning = false;
            if (s_radioP4.wifiScanCb) {
                s_radioP4.wifiScanCb(aps, true);
            }
        }

        if (s_radioP4.channelSweeping) {
            std::vector<DiscoveredNode> nodes;
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

bool startWifiScan(WifiScanCallback cb) {
    s_radioP4.wifiScanCb = cb;
    s_radioP4.wifiScanning = true;
    if (!s_p4ScanTask) {
        xTaskCreate(radioScanWorkerP4, "RadioScanP4", 4096, nullptr, 1, &s_p4ScanTask);
    }
    return true;
}

bool startChannelSweep(ChannelSweepCallback cb) {
    s_radioP4.sweepCb = cb;
    s_radioP4.channelSweeping = true;
    if (!s_p4ScanTask) {
        xTaskCreate(radioScanWorkerP4, "RadioScanP4", 4096, nullptr, 1, &s_p4ScanTask);
    }
    return true;
}

void stopScan() {
    s_radioP4.wifiScanning = false;
    s_radioP4.channelSweeping = false;
}

} // namespace radio
} // namespace cbdos
