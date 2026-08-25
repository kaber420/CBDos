#include "cbdos/mesh/mesh_engine.hpp"
#include "mesh_header.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace cbdos {
namespace mesh {

static constexpr uint32_t REASSEMBLY_TIMEOUT_MS = 2500; // 2.5 segundos para recibir todos los fragmentos
static constexpr uint32_t ARP_EXPIRY_MS         = 300000; // 5 minutos de validez ARP
static constexpr size_t MAX_CHUNK_PAYLOAD       = 240;   // Payload máx por fragmento

MeshEngine& MeshEngine::getInstance() {
    static MeshEngine s_instance;
    return s_instance;
}

MeshEngine::MeshEngine() {
}

MeshEngine::~MeshEngine() {
    stop();
}

void MeshEngine::setTransport(IMeshTransport* transport) {
    if (m_transport && m_running) {
        m_transport->stop();
    }
    m_transport = transport;
    if (m_transport) {
        m_transport->setRecvCallback([this](const uint8_t* src_mac, const uint8_t* data, size_t len, int8_t rssi) {
            this->onRawDataReceived(src_mac, data, len, rssi);
        });
    }
}

bool MeshEngine::init(uint8_t channel) {
    if (!m_transport) return false;
    if (m_running) return true;

    if (m_transport->init(channel)) {
        m_running = true;

        uint8_t mac[6] = {0};
        if (getMacAddress(mac)) {
            if (m_localUuid == 0) {
                // IPv4 Mesh: 10.MAC[3].MAC[4].MAC[5] (RFC 1918)
                m_localUuid = (10 << 24) | (static_cast<uint32_t>(mac[3]) << 16) | (static_cast<uint32_t>(mac[4]) << 8) | static_cast<uint32_t>(mac[5]);
            }
            if (m_localShortId == 0) {
                // Short ID candidato natural: 0xMAC[4]MAC[5]
                m_localShortId = (static_cast<uint16_t>(mac[4]) << 8) | static_cast<uint16_t>(mac[5]);
            }
        }
        return true;
    }
    return false;
}

void MeshEngine::stop() {
    if (m_transport && m_running) {
        m_transport->stop();
    }
    m_running = false;
    m_reassemblies.clear();
}

void MeshEngine::setRadioMode(RadioMode mode) {
    m_radioMode = mode;
    if (m_transport) {
        m_transport->setRadioMode(mode);
    }
}

const char* MeshEngine::getRadioModeName() const {
    switch (m_radioMode) {
        case RadioMode::EspNowNormal: return "ESP-NOW Normal (2.4G)";
        case RadioMode::EspNowLR:     return "ESP-NOW Long Range";
        case RadioMode::WifiIp:       return "Wi-Fi TCP/IP";
        case RadioMode::Auto:
        default:                      return "AUTO (Inteligente)";
    }
}

void MeshEngine::setLocalIdentity(uint16_t short_id, uint32_t uuid, uint16_t tower_id, uint16_t zone_id, uint16_t asn) {
    m_localShortId = short_id;
    m_localUuid    = uuid;
    m_localTower   = tower_id;
    m_localZone    = zone_id;
    m_localAsn     = asn;
}

void MeshEngine::registerServiceHandler(ServiceId svc, MeshServiceCallback cb) {
    m_serviceHandlers[static_cast<uint8_t>(svc)] = cb;
}

void MeshEngine::unregisterServiceHandler(ServiceId svc) {
    m_serviceHandlers.erase(static_cast<uint8_t>(svc));
}

bool MeshEngine::lookupArp(uint16_t short_id, uint8_t out_mac[6]) const {
    auto it = m_arpTable.find(short_id);
    if (it != m_arpTable.end()) {
        memcpy(out_mac, it->second.mac, 6);
        return true;
    }
    return false;
}

void MeshEngine::updateArp(uint16_t short_id, const uint8_t* mac, uint32_t now_ms) {
    if (!mac || short_id == 0 || short_id == 0xFFFF) return;
    ArpEntry& entry = m_arpTable[short_id];
    memcpy(entry.mac, mac, 6);
    entry.last_seen_ms = now_ms;
}

bool MeshEngine::sendPacket(ServiceId svc, uint32_t dst_id, const uint8_t* payload, size_t len, bool dst_only, const uint8_t* target_mac) {
    if (!m_transport || !m_running) return false;

    // Construir cabecera de malla
    MeshHeader hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.control = static_cast<uint8_t>(svc) & 0x0F;

    if (dst_only) {
        hdr.control |= CTRL_DST_ONLY;
        hdr.dst_id = dst_id;
        hdr.is_dst_only = true;
        hdr.is_short_id = true;
    } else {
        hdr.control |= CTRL_SHORT_ID;
        hdr.src_id = m_localShortId;
        hdr.dst_id = dst_id;
        hdr.is_short_id = true;
    }

    uint8_t mesh_hdr_buf[32];
    size_t mesh_hdr_len = build_mesh_header(mesh_hdr_buf, sizeof(mesh_hdr_buf), &hdr);
    if (mesh_hdr_len == 0) return false;

    // Buffer total a transmitir = [MeshHeader][Payload]
    std::vector<uint8_t> full_packet;
    full_packet.reserve(mesh_hdr_len + len);
    full_packet.insert(full_packet.end(), mesh_hdr_buf, mesh_hdr_buf + mesh_hdr_len);
    if (payload && len > 0) {
        full_packet.insert(full_packet.end(), payload, payload + len);
    }

    size_t total_size = full_packet.size();
    uint8_t msg_id = m_currentMsgId++;
    if (m_currentMsgId == 0) m_currentMsgId = 1;

    // Calcular fragmentos
    uint8_t total_chunks = static_cast<uint8_t>((total_size + MAX_CHUNK_PAYLOAD - 1) / MAX_CHUNK_PAYLOAD);
    if (total_chunks == 0) total_chunks = 1;
    if (total_chunks > 8) return false; // Límite de seguridad (hasta ~1.9 KB)

    // Determinar MAC destino
    uint8_t dest_mac[6];
    const uint8_t* final_mac = nullptr;
    if (target_mac) {
        final_mac = target_mac;
    } else if (dst_id != 0xFFFF && lookupArp(static_cast<uint16_t>(dst_id), dest_mac)) {
        final_mac = dest_mac;
    } // nullptr = Broadcast FF:FF:FF:FF:FF:FF

    size_t offset = 0;
    for (uint8_t i = 0; i < total_chunks; ++i) {
        size_t chunk_len = std::min(MAX_CHUNK_PAYLOAD, total_size - offset);

        uint8_t frame[ESPNOW_MAX_FRAME_SIZE];
        MicroChunkHeader mchunk;
        mchunk.set(i, total_chunks);
        mchunk.msg_id = msg_id;

        memcpy(frame, &mchunk, sizeof(MicroChunkHeader));
        memcpy(frame + sizeof(MicroChunkHeader), full_packet.data() + offset, chunk_len);

        size_t frame_len = sizeof(MicroChunkHeader) + chunk_len;
        if (!m_transport->sendRaw(final_mac, frame, frame_len)) {
            return false;
        }

        offset += chunk_len;
    }

    return true;
}

bool MeshEngine::sendTlvRequest(const char* url, uint32_t dst_id) {
    if (!url) return false;
    size_t url_len = strlen(url);

    // Trama TLV Request: [Tag: 0x01][Len_H: 0x00][Len_L: url_len][URL String]
    std::vector<uint8_t> req_tlv;
    req_tlv.resize(3 + url_len);
    req_tlv[0] = 0x01; // TYPE_REQ_URL
    req_tlv[1] = (url_len >> 8) & 0xFF;
    req_tlv[2] = url_len & 0xFF;
    memcpy(req_tlv.data() + 3, url, url_len);

    return sendPacket(ServiceId::TlvglRequest, dst_id, req_tlv.data(), req_tlv.size(), true, nullptr);
}

void MeshEngine::onRawDataReceived(const uint8_t* src_mac, const uint8_t* data, size_t len, int8_t rssi) {
    if (!src_mac || !data || len < sizeof(MicroChunkHeader)) return;

    const MicroChunkHeader* mchunk = reinterpret_cast<const MicroChunkHeader*>(data);
    uint8_t chunk_idx = mchunk->getChunkIndex();
    uint8_t total_chunks = mchunk->getTotalChunks();
    uint8_t msg_id = mchunk->msg_id;

    if (total_chunks == 0 || chunk_idx >= total_chunks || total_chunks > 8) return;

    size_t chunk_payload_len = len - sizeof(MicroChunkHeader);
    const uint8_t* chunk_payload = data + sizeof(MicroChunkHeader);

    // Si es un solo chunk no fragmentado, procesar instantáneamente sin cola
    if (total_chunks == 1) {
        processCompletedPacket(src_mac, chunk_payload, chunk_payload_len, rssi);
        return;
    }

    // Buscar reensamblado en curso
    ChunkReassembly* active_reassembly = nullptr;
    for (auto& r : m_reassemblies) {
        if (r.msg_id == msg_id && memcmp(r.src_mac, src_mac, 6) == 0) {
            active_reassembly = &r;
            break;
        }
    }

    // Crear nuevo reensamblado si no existe
    if (!active_reassembly) {
        ChunkReassembly new_r;
        new_r.msg_id = msg_id;
        new_r.total_chunks = total_chunks;
        new_r.received_mask = 0;
        new_r.start_time_ms = 0; // Se actualizará en tick si es necesario
        new_r.chunks.resize(total_chunks);
        memcpy(new_r.src_mac, src_mac, 6);
        new_r.rssi = rssi;
        m_reassemblies.push_back(std::move(new_r));
        active_reassembly = &m_reassemblies.back();
    }

    // Almacenar fragmento
    if (!(active_reassembly->received_mask & (1 << chunk_idx))) {
        active_reassembly->chunks[chunk_idx].assign(chunk_payload, chunk_payload + chunk_payload_len);
        active_reassembly->received_mask |= (1 << chunk_idx);
    }

    // Verificar si se completaron todos los fragmentos
    uint8_t expected_mask = (1 << total_chunks) - 1;
    if (active_reassembly->received_mask == expected_mask) {
        // Concatenar todos los fragmentos
        std::vector<uint8_t> full_packet;
        for (const auto& c : active_reassembly->chunks) {
            full_packet.insert(full_packet.end(), c.begin(), c.end());
        }

        processCompletedPacket(src_mac, full_packet.data(), full_packet.size(), rssi);

        // Eliminar reensamblado completado
        for (auto it = m_reassemblies.begin(); it != m_reassemblies.end(); ++it) {
            if (it->msg_id == msg_id && memcmp(it->src_mac, src_mac, 6) == 0) {
                m_reassemblies.erase(it);
                break;
            }
        }
    }
}

void MeshEngine::processCompletedPacket(const uint8_t* src_mac, const uint8_t* full_data, size_t full_len, int8_t rssi) {
    if (!full_data || full_len < 1) return;

    MeshHeader raw_hdr;
    size_t hdr_consumed = parse_mesh_header(full_data, full_len, &raw_hdr);
    if (hdr_consumed == 0 || hdr_consumed > full_len) return;

    MeshPacket packet;
    memcpy(packet.src_mac, src_mac, 6);
    packet.rssi = rssi;

    packet.header.control = raw_hdr.control;
    packet.header.service = raw_hdr.control & 0x0F;
    packet.header.src_id = raw_hdr.src_id;
    packet.header.dst_id = raw_hdr.dst_id;
    packet.header.src_asn = raw_hdr.src_asn;
    packet.header.dst_asn = raw_hdr.dst_asn;
    packet.header.src_zone = raw_hdr.src_zone;
    packet.header.dst_zone = raw_hdr.dst_zone;
    packet.header.src_tower = raw_hdr.src_tower;
    packet.header.dst_tower = raw_hdr.dst_tower;
    packet.header.is_short_id = raw_hdr.is_short_id;
    packet.header.is_dst_only = raw_hdr.is_dst_only;
    packet.header.is_global = (raw_hdr.control & CTRL_GLOBAL_BIT) != 0;
    packet.header.is_intra_zone = (raw_hdr.control & CTRL_INTRA_ZONE) != 0;

    // Aprender en tabla ARP si se conoce el Short ID
    if (raw_hdr.src_id > 0 && raw_hdr.src_id < 0xFFFF) {
        updateArp(static_cast<uint16_t>(raw_hdr.src_id), src_mac, 0);
    }

    // Copiar payload
    size_t payload_len = full_len - hdr_consumed;
    if (payload_len > 0) {
        packet.payload.assign(full_data + hdr_consumed, full_data + full_len);
    }

    // Si es mensaje de Control de Ruteo (Probe Response = 0x02)
    if (packet.header.service == static_cast<uint8_t>(ServiceId::RoutingControl)) {
        if (packet.payload.size() >= 6 && packet.payload[0] == 0x02) {
            uint16_t tower_id = (packet.payload[1] << 8) | packet.payload[2];
            uint8_t ch = packet.payload[3];
            uint8_t modes = packet.payload[4];
            uint8_t name_len = packet.payload[5];
            
            DiscoveredTower* dt = nullptr;
            for (auto& t : m_discoveredTowers) {
                if (memcmp(t.mac, src_mac, 6) == 0) {
                    dt = &t;
                    break;
                }
            }
            if (!dt) {
                DiscoveredTower new_t;
                memcpy(new_t.mac, src_mac, 6);
                m_discoveredTowers.push_back(new_t);
                dt = &m_discoveredTowers.back();
            }
            dt->short_id = tower_id;
            dt->channel = ch;
            dt->supported_modes = modes;
            dt->rssi = rssi;
            dt->last_seen_ms = 0;

            size_t copy_n = name_len < 31 ? name_len : 31;
            if (packet.payload.size() >= 6 + copy_n) {
                memcpy(dt->name, packet.payload.data() + 6, copy_n);
                dt->name[copy_n] = '\0';
            } else {
                snprintf(dt->name, sizeof(dt->name), "Torre 0x%04X", tower_id);
            }

            if (m_towersCb) {
                m_towersCb();
            }
        }
    }

    // Despachar al servicio correspondiente
    auto it = m_serviceHandlers.find(packet.header.service);
    if (it != m_serviceHandlers.end() && it->second) {
        it->second(packet);
    }
}

void MeshEngine::sendTowerProbe() {
    if (!m_running) {
        init(1);
    }

    // Payload Probe Request: [Tag 0x01][MyShortID 2B][Name 10B]
    std::vector<uint8_t> probe;
    probe.push_back(0x01); // Tag Probe Request
    probe.push_back((m_localShortId >> 8) & 0xFF);
    probe.push_back(m_localShortId & 0xFF);
    const char* client_name = "CBDos S3";
    probe.push_back((uint8_t)strlen(client_name));
    probe.insert(probe.end(), client_name, client_name + strlen(client_name));

    sendPacket(ServiceId::RoutingControl, 0xFFFF, probe.data(), probe.size(), true, nullptr);
}

uint8_t MeshEngine::getChannel() const {
    if (m_transport) return m_transport->getChannel();
    return 1;
}

bool MeshEngine::setChannel(uint8_t ch) {
    if (m_transport) return m_transport->setChannel(ch);
    return false;
}

bool MeshEngine::getMacAddress(uint8_t mac[6]) {
    if (m_transport && mac) return m_transport->getMacAddress(mac);
    return false;
}

void MeshEngine::tick(uint32_t current_time_ms) {
    // Limpieza de reensamblados que hayan excedido el timeout
    m_reassemblies.erase(
        std::remove_if(m_reassemblies.begin(), m_reassemblies.end(),
            [current_time_ms](const ChunkReassembly& r) {
                if (r.start_time_ms > 0 && (current_time_ms - r.start_time_ms) > REASSEMBLY_TIMEOUT_MS) {
                    return true;
                }
                return false;
            }),
        m_reassemblies.end()
    );
}

} // namespace mesh
} // namespace cbdos
