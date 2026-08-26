#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <vector>
#include <functional>
#include <ctime>

namespace cbdos {
namespace mesh {

// Límite máximo de trama ESP-NOW
static constexpr size_t ESPNOW_MAX_FRAME_SIZE = 250;

// Modos de Radio seleccionables
enum class RadioMode : uint8_t {
    Auto         = 0, // Detección y conmutación automática
    EspNowNormal = 1, // 2.4 GHz Estándar (802.11 b/g/n, ~1 Mbps)
    EspNowLR     = 2, // 2.4 GHz Long Range (+20 dBm, ~1 km)
    WifiIp       = 3  // Wi-Fi TCP/IP Legacy
};

// Servicios de Malla (Bits 4..0 de Control)
enum class ServiceId : uint8_t {
    Chat            = 0x01,
    FileTransfer    = 0x02,
    SensorTelemetry = 0x03,
    WebProxy        = 0x05,
    TlvglRequest    = 0x07,
    TlvglResponse   = 0x08,
    RoutingControl  = 0x0F
};

// Flags en el byte de Control
static constexpr uint8_t CTRL_GLOBAL_BIT   = (1 << 7); // 1 = Inter-ASN BGP (21B)
static constexpr uint8_t CTRL_SIGNAL_BIT   = (1 << 6); // 1 = Señalización / Control
static constexpr uint8_t CTRL_INTRA_ZONE   = (1 << 5); // 1 = Intra-Zona OSPF (9B / 13B)
static constexpr uint8_t CTRL_SHORT_ID     = (1 << 4); // 1 = Short ID (2B), 0 = Full UUID (4B)
static constexpr uint8_t CTRL_DST_ONLY     = (1 << 3); // 1 = Solo Dst Short ID (3B ultra-ligero)

/**
 * @brief Micro-Header de fragmentación (2 Bytes)
 * Permite segmentar payloads grandes (ej: páginas TLV) en micro-chunks <= 250B
 */
#pragma pack(push, 1)
struct MicroChunkHeader {
    uint8_t chunk_info; // [Chunk Index: 4 bits] | [Total Chunks: 4 bits]
    uint8_t msg_id;     // ID de correlación del mensaje completo

    inline uint8_t getChunkIndex() const { return (chunk_info >> 4) & 0x0F; }
    inline uint8_t getTotalChunks() const { return chunk_info & 0x0F; }
    inline void set(uint8_t index, uint8_t total) {
        chunk_info = ((index & 0x0F) << 4) | (total & 0x0F);
    }
};
#pragma pack(pop)

/**
 * @brief Estructura deserializada de cabecera Mesh
 */
struct MeshHeaderParsed {
    uint8_t  control = 0;
    uint8_t  service = 0;
    
    // Pseudo-BGP (21B)
    uint16_t src_asn = 0;
    uint16_t dst_asn = 0;
    
    // Pseudo-OSPF (9B / 13B)
    uint16_t src_zone = 0;
    uint16_t dst_zone = 0;
    uint16_t src_tower = 0;
    uint16_t dst_tower = 0;
    
    // Identificadores de Nodo
    uint32_t src_id = 0;
    uint32_t dst_id = 0;
    
    bool is_short_id = true;
    bool is_dst_only = false;
    bool is_global   = false;
    bool is_intra_zone = false;
};

/**
 * @brief Estructura de paquete recibido completo (reensamblado)
 */
struct MeshPacket {
    uint8_t src_mac[6] = {0};
    MeshHeaderParsed header;
    std::vector<uint8_t> payload;
    int8_t rssi = 0;
};

// Banderas de servicios de Torre (Legacy y Broadcast)
static constexpr uint8_t TOWER_SVC_INTERNET = (1 << 0);
static constexpr uint8_t TOWER_SVC_TIME     = (1 << 1);

// Banderas de Estado del PoP (Byte 6 del Micro-Broadcast de 7 Bytes)
static constexpr uint8_t POP_STATUS_INTERNET_UP  = (1 << 0); // 1 = Salida a Internet activa, 0 = Offline/Solo local
static constexpr uint8_t POP_STATUS_PROXY_OPEN   = (1 << 1); // 1 = Salida libre por proxy, 0 = Requiere portal cautivo/login
static constexpr uint8_t POP_STATUS_BAAS_BUSY    = (1 << 2); // 1 = Servidor digestor web ocupado, 0 = Disponible
static constexpr uint8_t POP_STATUS_ALERT_ACTIVE = (1 << 3); // 1 = Alerta activa en el tablón del PoP, 0 = Normal

/**
 * @brief Micro-Broadcast de Metadatos del PoP (Exactamente 7 Bytes Little-Endian)
 * Emitido periódicamente (cada 60s) por el Gateway/Torre sin requerir respuesta (0 TX del cliente)
 */
#pragma pack(push, 1)
struct PoPBroadcastFrame {
    uint32_t epoch;       // Bytes 0..3: Unix Epoch (Segundos Unix actuales)
    uint16_t cover_hash;  // Bytes 4..5: Hash CRC16 de la página de inicio (index.bcml)
    uint8_t  status_code; // Byte 6: Bitmask de estado del PoP (POP_STATUS_*)
};
#pragma pack(pop)

// Estructura de Torre descubierta en el aire
struct DiscoveredTower {
    uint8_t mac[6] = {0};
    uint16_t short_id = 0;
    char name[32] = {0};
    uint8_t channel = 1;
    uint8_t supported_modes = 0x03; // 0x01 = Normal, 0x02 = LR, 0x04 = FLRC
    uint8_t active_services = 0;
    time_t last_advertised_epoch = 0;
    uint16_t cover_hash = 0;
    uint8_t status_code = 0;
    int8_t rssi = -100;
    uint32_t last_seen_ms = 0;
};

// Callback cuando se recibe un paquete completo para un servicio
using MeshServiceCallback = std::function<void(const MeshPacket& packet)>;

} // namespace mesh
} // namespace cbdos
