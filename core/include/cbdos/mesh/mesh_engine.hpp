#pragma once

#include "mesh_types.hpp"
#include "mesh_transport.hpp"
#include <unordered_map>
#include <map>
#include <vector>
#include <memory>
#include <mutex>
#include <string>

namespace cbdos {
namespace mesh {

struct ArpEntry {
    uint8_t mac[6];
    uint32_t last_seen_ms;
};

struct ChunkReassembly {
    uint8_t msg_id;
    uint8_t total_chunks;
    uint8_t received_mask; // Bitmask para hasta 8 chunks (hasta ~1.9 KB por paquete)
    uint32_t start_time_ms;
    std::vector<std::vector<uint8_t>> chunks;
    uint8_t src_mac[6];
    int8_t rssi;
};

/**
 * @brief Motor principal de red en malla para CBDos (C++ agnóstico)
 */
class MeshEngine {
public:
    static MeshEngine& getInstance();

    /**
     * @brief Inyecta la implementación física de transporte (ESP-NOW, LoRa, etc.)
     */
    void setTransport(IMeshTransport* transport);
    IMeshTransport* getTransport() const { return m_transport; }

    /**
     * @brief Inicializa el motor de malla y la radio
     */
    bool init(uint8_t channel = 1);

    /**
     * @brief Detiene el motor y la radio
     */
    void stop();

    bool isRunning() const { return m_running; }

    /**
     * @brief Configura y obtiene el modo de radio activo
     */
    void setRadioMode(RadioMode mode);
    RadioMode getRadioMode() const { return m_radioMode; }
    const char* getRadioModeName() const;

    /**
     * @brief Configura la identidad del nodo local
     */
    void setLocalIdentity(uint16_t short_id, uint32_t uuid, uint16_t tower_id = 0, uint16_t zone_id = 0, uint16_t asn = 0);
    uint16_t getLocalShortId() const { return m_localShortId; }
    uint32_t getLocalUuid() const { return m_localUuid; }

    /**
     * @brief Envía un payload a un destino con fragmentación automática
     * @param svc Servicio destino (ej: TlvglRequest, Chat)
     * @param dst_id Short ID (2B) o UUID (4B) del nodo destino
     * @param payload Datos útiles
     * @param len Longitud del payload
     * @param dst_only Si es true, usa cabecera ultra-corta de 3 bytes
     * @param target_mac MAC destino opcional (si es null, busca en ARP o hace broadcast)
     */
    bool sendPacket(ServiceId svc, uint32_t dst_id, const uint8_t* payload, size_t len, bool dst_only = true, const uint8_t* target_mac = nullptr);

    /**
     * @brief Atajo para emitir una petición TLVGL por el aire (espnow://... o .mesh)
     */
    bool sendTlvRequest(const char* url, uint32_t dst_id = 0xFFFF);

    /**
     * @brief Registra un handler para un servicio específico
     */
    void registerServiceHandler(ServiceId svc, MeshServiceCallback cb);
    void unregisterServiceHandler(ServiceId svc);

    /**
     * @brief Sondeo y Escaneo de Torres en el Aire
     */
    void sendTowerProbe();
    void setEpochReceivedCallback(std::function<void(time_t)> cb); // Inyectado por el BSP al arranque
    const std::vector<DiscoveredTower>& getDiscoveredTowers() const { return m_discoveredTowers; }
    void clearDiscoveredTowers() { m_discoveredTowers.clear(); }
    void setDiscoveredTowersCallback(std::function<void()> cb) { m_towersCb = cb; }

    uint8_t getChannel() const;
    bool setChannel(uint8_t ch);
    bool getMacAddress(uint8_t mac[6]);

    int8_t getLastRssi() const { return m_lastRssi; }
    uint32_t getRxPackets() const { return m_rxPackets; }
    uint32_t getTxPackets() const { return m_txPackets; }

    /**
     * @brief Tarea periódica de mantenimiento (limpieza de fragmentos huérfanos y expiración ARP)
     */
    void tick(uint32_t current_time_ms);

    /**
     * @brief Mapea un Short ID a su MAC si existe en la tabla Pseudo-ARP
     */
    bool lookupArp(uint16_t short_id, uint8_t out_mac[6]) const;

private:
    MeshEngine();
    ~MeshEngine();

    MeshEngine(const MeshEngine&) = delete;
    MeshEngine& operator=(const MeshEngine&) = delete;

    void onRawDataReceived(const uint8_t* src_mac, const uint8_t* data, size_t len, int8_t rssi);
    void processCompletedPacket(const uint8_t* src_mac, const uint8_t* full_data, size_t full_len, int8_t rssi);
    void updateArp(uint16_t short_id, const uint8_t* mac, uint32_t now_ms);

    IMeshTransport* m_transport = nullptr;
    RadioMode m_radioMode = RadioMode::Auto;
    bool m_running = false;
    uint8_t m_currentMsgId = 1;

    uint16_t m_localShortId = 0;
    uint32_t m_localUuid = 0;
    uint16_t m_localTower = 0x0001;
    uint16_t m_localZone = 0x0001;
    uint16_t m_localAsn = 0x0001;

    std::map<uint8_t, MeshServiceCallback> m_serviceHandlers;
    std::map<uint16_t, ArpEntry> m_arpTable;
    std::vector<ChunkReassembly> m_reassemblies;
    std::vector<DiscoveredTower> m_discoveredTowers;
    std::function<void()> m_towersCb = nullptr;
    std::function<void(time_t)> m_epochReceivedCb = nullptr;

    int8_t m_lastRssi = -127;
    uint32_t m_rxPackets = 0;
    uint32_t m_txPackets = 0;
};

} // namespace mesh
} // namespace cbdos
