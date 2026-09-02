#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <functional>

namespace cbdos {
namespace network {

enum class InterfaceType {
    Disabled = 0,
    RadioPacket,   // LoRa SX1262, FLRC SX1280, ESP-NOW
    IpNetwork,     // Wi-Fi STA/AP con stack TCP/IP
    BluetoothLe,   // BLE 5.0 GATT
    SerialModem    // USB-CDC / UART TNC
};

enum class InterfaceMode {
    Off = 0,
    EspNow,
    EspNowLR,
    WifiStation,
    WifiAccessPoint,
    BleGattServer,
    LoRa_915MHz,
    LoRa_868MHz,
    FLRC_2400MHz,
    UsbCdcTnc
};

typedef void (*PacketRecvCallback)(const uint8_t* payload, size_t len, int rssi, int snr, void* userCtx);

/**
 * @brief Contrato abstracto para interfaces de red físicas/virtuales (Capa de Enlace / Data Link)
 */
class INetworkInterface {
public:
    virtual ~INetworkInterface() = default;

    virtual const char* getName() const = 0;
    virtual InterfaceType getType() const = 0;
    virtual InterfaceMode getMode() const = 0;
    virtual bool setMode(InterfaceMode mode) = 0;

    virtual bool isReady() const = 0;
    virtual int sendPacket(const uint8_t* buffer, size_t len) = 0;
    virtual void setPacketRecvCallback(PacketRecvCallback cb, void* userCtx) = 0;

    virtual uint8_t getChannel() const { return 1; }
    virtual bool setChannel(uint8_t channel) { return true; }
    virtual bool getMacAddress(uint8_t out_mac[6]) { return false; }
    virtual const char* getAlias() const { return nullptr; }
    virtual int8_t getTxPower() const { return 20; }
};

#define MAX_NETWORK_SLOTS 4

/**
 * @brief Orquestador Singleton de Interfaces de Red y Radios
 */
class NetworkInterfaceManager {
public:
    static NetworkInterfaceManager& getInstance();

    bool registerInterface(uint8_t slot, INetworkInterface* iface);
    INetworkInterface* getInterface(uint8_t slot);
    uint8_t getInterfaceCount() const;

    void setAllOffline();
    bool isAnyInterfaceActive() const;

private:
    NetworkInterfaceManager() = default;
    ~NetworkInterfaceManager() = default;
    NetworkInterfaceManager(const NetworkInterfaceManager&) = delete;
    NetworkInterfaceManager& operator=(const NetworkInterfaceManager&) = delete;

    INetworkInterface* m_slots[MAX_NETWORK_SLOTS] = {nullptr};
};

} // namespace network
} // namespace cbdos
