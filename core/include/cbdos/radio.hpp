#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>

namespace cbdos {
namespace radio {

/**
 * @brief Modos de operación para el transceptor RF integrado de 2.4 GHz
 */
enum class RadioMode {
    Off = 0,        // Radio apagada (Modo seguro / Ultra bajo consumo)
    WifiSta = 1,    // Wi-Fi Estación TCP/IP estándar (Conexión a AP / Internet)
    EspNow = 2,     // ESP-NOW Normal (Capa 2 @ 1-2 Mbps, baja latencia)
    EspNowLR = 3,   // ESP-NOW Long Range (Protocolo 802.11 LR @ 250 kbps, +20dBm)
    Hybrid = 4      // Híbrido (Wi-Fi STA + Recepción simultánea ESP-NOW en canal asociado)
};

/**
 * @brief Estructura de resultado de escaneo de Punto de Acceso Wi-Fi
 */
struct WifiApInfo {
    std::string ssid;
    int8_t rssi = -127;
    uint8_t channel = 1;
    bool isEncrypted = false;
    std::string bssid;
};

/**
 * @brief Información de Nodo / Gateway detectado en el aire mediante sondeo
 */
struct DiscoveredNode {
    uint8_t mac[6] = {0};
    uint16_t short_id = 0;
    uint32_t uuid = 0;
    char name[32] = {0};
    uint8_t channel = 1;
    int8_t rssi = -127;
    uint8_t supported_modes = 0;
    uint32_t last_seen_ms = 0;
};

/**
 * @brief Estructura de configuración persistente de la radio integrada
 */
struct RadioConfig {
    bool enabled = true;
    RadioMode mode = RadioMode::EspNow;
    uint8_t channel = 1;
    int8_t txPower = 20; // +20 dBm (0..20)
};

// Callbacks de escaneo asíncrono
using WifiScanCallback = std::function<void(const std::vector<WifiApInfo>& aps, bool success)>;
using ChannelSweepCallback = std::function<void(uint8_t currentChannel, uint8_t totalChannels, const std::vector<DiscoveredNode>& nodes, bool finished)>;

/**
 * @brief Interfaz abstracta del Backend de Hardware de Radio (HAL)
 */
class IRadioBackend {
public:
    virtual ~IRadioBackend() = default;

    virtual bool init(const RadioConfig& cfg) = 0;
    virtual bool setPower(bool on) = 0;
    virtual bool isPowered() const = 0;

    virtual bool setMode(RadioMode mode) = 0;
    virtual RadioMode getMode() const = 0;

    virtual bool setChannel(uint8_t channel) = 0;
    virtual uint8_t getChannel() const = 0;

    virtual bool setTxPower(int8_t dbm) = 0;
    virtual int8_t getTxPower() const = 0;

    virtual bool startWifiScan(WifiScanCallback cb) = 0;
    virtual bool startChannelSweep(ChannelSweepCallback cb) = 0;
    virtual void stopScan() = 0;
};

void setRadioBackend(IRadioBackend* backend);
IRadioBackend* getRadioBackend();

/**
 * @brief Inicializa el subsistema de radio en el arranque con la configuración persistida en NVS
 */
bool init();

/**
 * @brief APIs de control del transceptor físico de radio 2.4 GHz
 */
bool isRadioPowered();
void setRadioPower(bool on);

bool setMode(RadioMode mode);
RadioMode getMode();
const char* getModeName(RadioMode mode);

uint8_t getChannel();
bool setChannel(uint8_t channel);

int8_t getTxPower();
bool setTxPower(int8_t dbm);

/**
 * @brief Inicia un escaneo Wi-Fi real no bloqueante de redes circundantes
 */
bool startWifiScan(WifiScanCallback cb);

/**
 * @brief Inicia un barrido multicanal real (Channel Hopping Sweep 1..13) para descubrir nodos/gateways en el aire
 */
bool startChannelSweep(ChannelSweepCallback cb);

/**
 * @brief Cancela cualquier escaneo o barrido en curso
 */
void stopScan();

} // namespace radio
} // namespace cbdos
