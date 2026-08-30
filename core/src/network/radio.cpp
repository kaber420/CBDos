#include "cbdos/radio.hpp"
#include "cbdos/config_manager.hpp"

namespace cbdos {
namespace radio {

static IRadioBackend* s_radioBackend = nullptr;

void setRadioBackend(IRadioBackend* backend) {
    s_radioBackend = backend;
}

IRadioBackend* getRadioBackend() {
    return s_radioBackend;
}

bool init() {
    RadioConfig cfg;
    ConfigManager::getInstance().loadRadio(cfg);
    if (s_radioBackend) {
        return s_radioBackend->init(cfg);
    }
    return false;
}

bool isRadioPowered() {
    if (s_radioBackend) {
        return s_radioBackend->isPowered();
    }
    return false;
}

void setRadioPower(bool on) {
    if (s_radioBackend) {
        s_radioBackend->setPower(on);
    }
}

bool setMode(RadioMode mode) {
    if (s_radioBackend) {
        return s_radioBackend->setMode(mode);
    }
    return false;
}

RadioMode getMode() {
    if (s_radioBackend) {
        return s_radioBackend->getMode();
    }
    return RadioMode::Off;
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
    if (s_radioBackend) {
        return s_radioBackend->getChannel();
    }
    return 1;
}

bool setChannel(uint8_t channel) {
    if (s_radioBackend) {
        return s_radioBackend->setChannel(channel);
    }
    return false;
}

int8_t getTxPower() {
    if (s_radioBackend) {
        return s_radioBackend->getTxPower();
    }
    return 20;
}

bool setTxPower(int8_t dbm) {
    if (s_radioBackend) {
        return s_radioBackend->setTxPower(dbm);
    }
    return false;
}

bool startWifiScan(WifiScanCallback cb) {
    if (s_radioBackend) {
        return s_radioBackend->startWifiScan(cb);
    }
    if (cb) {
        cb({}, false);
    }
    return false;
}

bool startChannelSweep(ChannelSweepCallback cb) {
    if (s_radioBackend) {
        return s_radioBackend->startChannelSweep(cb);
    }
    if (cb) {
        cb(1, 13, {}, true);
    }
    return false;
}

void stopScan() {
    if (s_radioBackend) {
        s_radioBackend->stopScan();
    }
}

} // namespace radio
} // namespace cbdos
