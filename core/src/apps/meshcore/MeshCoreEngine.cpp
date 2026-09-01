#include "MeshCoreEngine.hpp"
#include "cbdos/network_interface.hpp"
#include "cbdos/log.hpp"
#include "cbdos/time.hpp"
#include <cstring>
#include <algorithm>

namespace cbdos {
namespace apps {
namespace meshcore {

static const char* TAG = "MeshCore";
static const uint16_t MESHCORE_MAGIC = 0x4D43; // 'MC'

enum PacketType : uint8_t {
    PKT_BEACON = 0x01,
    PKT_CHAT   = 0x02,
    PKT_ACK    = 0x03
};

MeshCoreEngine& MeshCoreEngine::getInstance() {
    static MeshCoreEngine instance;
    return instance;
}

MeshCoreEngine::MeshCoreEngine() {
    // Configuración inicial de ranuras
    m_interfaces[0] = {MeshInterfaceId::Interface1_Internal, "Radio 1 (ESP-NOW)", MeshInterfaceType::EspNow, true, 1, 2400.0f, 0};
    m_interfaces[1] = {MeshInterfaceId::Interface2_Backpack, "Radio 2 (LoRa Mochila)", MeshInterfaceType::SX1262_LoRa, false, 1, 915.0f, 0};
    m_interfaces[2] = {MeshInterfaceId::Interface3_USB,      "Radio 3 (USB Módem)", MeshInterfaceType::USB_CDC, false, 0, 0.0f, 115200};

    // Canal público predeterminado
    m_channels.push_back({0, "#general", false, ""});
}

bool MeshCoreEngine::init() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_running) return true;

    CBD_LOG_I(TAG, "Iniciando motor MeshCore (Identity: 0x%04X, Name: %s)...", m_localShortId, m_localName.c_str());

    // Sincronizar y enganchar interfaces reales con NetworkInterfaceManager
    for (size_t i = 0; i < static_cast<size_t>(MeshInterfaceId::MaxInterfaces); ++i) {
        auto* iface = cbdos::network::NetworkInterfaceManager::getInstance().getInterface(i);
        if (iface) {
            m_interfaces[i].name = iface->getName();
            m_interfaces[i].channel = iface->getChannel();
            if (m_interfaces[i].enabled) {
                iface->setMode(cbdos::network::InterfaceMode::EspNow);
                iface->setPacketRecvCallback([](const uint8_t* payload, size_t len, int rssi, int snr, void* userCtx) {
                    auto* engine = static_cast<MeshCoreEngine*>(userCtx);
                    if (engine) {
                        engine->handleRawPacket(MeshInterfaceId::Interface1_Internal, payload, len, static_cast<int8_t>(rssi));
                    }
                }, this);
            }
        }
    }

    m_running = true;

    if (m_statusCb) m_statusCb();

    // Emitir presencia inicial
    sendBeacon();

    return true;
}

void MeshCoreEngine::stop() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_running) return;

    CBD_LOG_I(TAG, "Deteniendo motor MeshCore y liberando interfaces...");

    for (size_t i = 0; i < static_cast<size_t>(MeshInterfaceId::MaxInterfaces); ++i) {
        auto* iface = cbdos::network::NetworkInterfaceManager::getInstance().getInterface(i);
        if (iface) {
            iface->setMode(cbdos::network::InterfaceMode::Off);
        }
        if (m_transports[i]) {
            m_transports[i]->stop();
        }
    }

    m_running = false;
    if (m_statusCb) m_statusCb();
}

bool MeshCoreEngine::isInterfaceEnabled(MeshInterfaceId id) const {
    size_t idx = static_cast<size_t>(id);
    if (idx >= static_cast<size_t>(MeshInterfaceId::MaxInterfaces)) return false;
    auto* iface = cbdos::network::NetworkInterfaceManager::getInstance().getInterface(idx);
    if (iface) {
        return iface->isReady() && (iface->getMode() != cbdos::network::InterfaceMode::Off);
    }
    return m_interfaces[idx].enabled;
}

void MeshCoreEngine::setInterfaceEnabled(MeshInterfaceId id, bool enable) {
    size_t idx = static_cast<size_t>(id);
    if (idx >= static_cast<size_t>(MeshInterfaceId::MaxInterfaces)) return;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_interfaces[idx].enabled = enable;
        auto* iface = cbdos::network::NetworkInterfaceManager::getInstance().getInterface(idx);
        if (iface) {
            if (enable) {
                iface->setMode(cbdos::network::InterfaceMode::EspNow);
                iface->setPacketRecvCallback([](const uint8_t* payload, size_t len, int rssi, int snr, void* userCtx) {
                    auto* engine = static_cast<MeshCoreEngine*>(userCtx);
                    if (engine) {
                        engine->handleRawPacket(MeshInterfaceId::Interface1_Internal, payload, len, static_cast<int8_t>(rssi));
                    }
                }, this);
            } else {
                iface->setMode(cbdos::network::InterfaceMode::Off);
            }
        }
        if (m_transports[idx]) {
            if (enable && m_running) {
                m_transports[idx]->init(m_interfaces[idx].channel);
            } else if (!enable) {
                m_transports[idx]->stop();
            }
        }
    }

    if (m_statusCb) m_statusCb();
}

MeshInterfaceConfig MeshCoreEngine::getInterfaceConfig(MeshInterfaceId id) const {
    size_t idx = static_cast<size_t>(id);
    if (idx >= static_cast<size_t>(MeshInterfaceId::MaxInterfaces)) return {};
    auto* iface = cbdos::network::NetworkInterfaceManager::getInstance().getInterface(idx);
    if (iface) {
        MeshInterfaceConfig cfg = m_interfaces[idx];
        cfg.name = iface->getName();
        cfg.channel = iface->getChannel();
        cfg.enabled = iface->isReady() && (iface->getMode() != cbdos::network::InterfaceMode::Off);
        return cfg;
    }
    return m_interfaces[idx];
}

void MeshCoreEngine::setInterfaceConfig(const MeshInterfaceConfig& cfg) {
    size_t idx = static_cast<size_t>(cfg.id);
    if (idx >= static_cast<size_t>(MeshInterfaceId::MaxInterfaces)) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    m_interfaces[idx] = cfg;
    auto* iface = cbdos::network::NetworkInterfaceManager::getInstance().getInterface(idx);
    if (iface) {
        iface->setChannel(cfg.channel);
        if (cfg.enabled) {
            iface->setMode(cbdos::network::InterfaceMode::EspNow);
        } else {
            iface->setMode(cbdos::network::InterfaceMode::Off);
        }
    }
}

size_t MeshCoreEngine::getActiveInterfaceCount() const {
    size_t count = 0;
    for (size_t i = 0; i < static_cast<size_t>(MeshInterfaceId::MaxInterfaces); ++i) {
        auto* iface = cbdos::network::NetworkInterfaceManager::getInstance().getInterface(i);
        if (iface && iface->isReady() && (iface->getMode() != cbdos::network::InterfaceMode::Off)) {
            count++;
        } else if (!iface && m_interfaces[i].enabled) {
            count++;
        }
    }
    return count;
}

void MeshCoreEngine::setTransport(MeshInterfaceId id, cbdos::mesh::IMeshTransport* transport) {
    size_t idx = static_cast<size_t>(id);
    if (idx >= static_cast<size_t>(MeshInterfaceId::MaxInterfaces)) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    m_transports[idx] = transport;
    if (transport && m_interfaces[idx].enabled && m_running) {
        transport->init(m_interfaces[idx].channel);
        transport->setRecvCallback([this, id](const uint8_t* src_mac, const uint8_t* data, size_t len, int8_t rssi) {
            this->handleRawPacket(id, data, len, rssi);
        });
    }
}

void MeshCoreEngine::setLocalIdentity(uint16_t shortId, const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_localShortId = shortId;
    m_localName = name.empty() ? ("CBDos-" + std::to_string(shortId)) : name;
}

std::vector<MeshNode> MeshCoreEngine::getNodes() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_nodes;
}

void MeshCoreEngine::updateNode(uint16_t shortId, const std::string& name, NodeType type, int8_t rssi, uint8_t hops, MeshInterfaceId iface) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (shortId == m_localShortId) return;

    uint32_t now = static_cast<uint32_t>(cbdos::time::getEpoch());

    for (auto& node : m_nodes) {
        if (node.shortId == shortId) {
            node.name = name.empty() ? node.name : name;
            node.type = type;
            node.rssi = rssi;
            node.hops = hops;
            node.lastSeenSec = now;
            node.seenOnInterface = iface;
            return;
        }
    }

    if (m_nodes.size() >= 32) {
        m_nodes.erase(m_nodes.begin());
    }

    MeshNode newNode;
    newNode.shortId = shortId;
    newNode.name = name.empty() ? ("Nodo " + std::to_string(shortId)) : name;
    newNode.type = type;
    newNode.rssi = rssi;
    newNode.hops = hops;
    newNode.lastSeenSec = now;
    newNode.seenOnInterface = iface;
    m_nodes.push_back(newNode);

    CBD_LOG_I(TAG, "[+] Nodo MeshCore descubierto: ID=0x%04X, Nombre='%s', RSSI=%d dBm", shortId, newNode.name.c_str(), rssi);

    if (m_nodeCb) {
        m_nodeCb(newNode);
    }
}

// ────────────────────────────────────────────────────────────────
// Criptografía Simétrica y Gestión de Canales
// ────────────────────────────────────────────────────────────────
std::string MeshCoreEngine::encryptPayload(const std::string& text, const std::string& key) {
    if (key.empty() || text.empty()) return text;
    std::string result = text;
    uint8_t s[256];
    for (int idx = 0; idx < 256; idx++) s[idx] = static_cast<uint8_t>(idx);
    int j = 0;
    for (int idx = 0; idx < 256; idx++) {
        j = (j + s[idx] + static_cast<uint8_t>(key[idx % key.length()])) % 256;
        std::swap(s[idx], s[j]);
    }
    int i = 0; j = 0;
    for (size_t k = 0; k < text.length(); k++) {
        i = (i + 1) % 256;
        j = (j + s[i]) % 256;
        std::swap(s[i], s[j]);
        uint8_t rnd = s[(s[i] + s[j]) % 256];
        result[k] = text[k] ^ rnd;
    }
    return result;
}

std::string MeshCoreEngine::decryptPayload(const std::string& cipherText, const std::string& key) {
    return encryptPayload(cipherText, key); // Simétrico XOR
}

bool MeshCoreEngine::isPacketSeen(uint32_t msgId) {
    if (msgId == 0) return false;
    for (size_t i = 0; i < 128; ++i) {
        if (m_seenPacketIds[i] == msgId) return true;
    }
    return false;
}

void MeshCoreEngine::markPacketSeen(uint32_t msgId) {
    if (msgId == 0) return;
    m_seenPacketIds[m_seenIdx] = msgId;
    m_seenIdx = (m_seenIdx + 1) % 128;
}

void MeshCoreEngine::forwardPacket(MeshInterfaceId incomingIface, const uint8_t* data, size_t len) {
    if (!m_running || !data || len == 0) return;

    for (size_t i = 0; i < static_cast<size_t>(MeshInterfaceId::MaxInterfaces); ++i) {
        // No reenviar por la misma interfaz de la que provino
        if (static_cast<MeshInterfaceId>(i) == incomingIface) continue;

        if (m_interfaces[i].enabled) {
            auto* iface = cbdos::network::NetworkInterfaceManager::getInstance().getInterface(i);
            if (iface && iface->isReady()) {
                iface->sendPacket(data, len);
            } else if (m_transports[i]) {
                m_transports[i]->sendRaw(nullptr, data, len);
            }
        }
    }
}

std::vector<MeshChannel> MeshCoreEngine::getChannels() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_channels;
}

bool MeshCoreEngine::addChannel(const std::string& name, bool isPrivate, const std::string& key) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (name.empty()) return false;
    for (const auto& ch : m_channels) {
        if (ch.name == name) return false;
    }
    MeshChannel newCh;
    newCh.id = m_nextChannelId++;
    newCh.name = name;
    newCh.isPrivate = isPrivate;
    newCh.pskKey = key;
    m_channels.push_back(newCh);
    CBD_LOG_I(TAG, "Nuevo canal agregado: ID=%u, Nombre='%s', Privado=%d", newCh.id, name.c_str(), isPrivate);
    return true;
}

bool MeshCoreEngine::removeChannel(uint16_t id) {
    if (id == 0) return false; // #general no se puede eliminar
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto it = m_channels.begin(); it != m_channels.end(); ++it) {
        if (it->id == id) {
            m_channels.erase(it);
            if (m_activeChannelId == id) {
                m_activeChannelId = 0;
            }
            return true;
        }
    }
    return false;
}

void MeshCoreEngine::setActiveChannelId(uint16_t id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_activeChannelId = id;
}

std::string MeshCoreEngine::getActiveChannelName() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& ch : m_channels) {
        if (ch.id == m_activeChannelId) return ch.name;
    }
    return "#general";
}

void MeshCoreEngine::sendBeacon() {
    if (!m_running) return;

    // Construcción de paquete baliza MeshCore
    uint8_t buffer[64];
    buffer[0] = static_cast<uint8_t>(MESHCORE_MAGIC & 0xFF);
    buffer[1] = static_cast<uint8_t>((MESHCORE_MAGIC >> 8) & 0xFF);
    buffer[2] = PKT_BEACON;
    buffer[3] = 0; // 0 hops
    buffer[4] = static_cast<uint8_t>(m_localShortId & 0xFF);
    buffer[5] = static_cast<uint8_t>((m_localShortId >> 8) & 0xFF);
    buffer[6] = 0xFF; // Broadcast
    buffer[7] = 0xFF;
    buffer[8] = 0;
    buffer[9] = 0;

    size_t nameLen = std::min(m_localName.length(), (size_t)32);
    buffer[10] = static_cast<uint8_t>(nameLen);
    if (nameLen > 0) {
        std::memcpy(&buffer[11], m_localName.c_str(), nameLen);
    }

    size_t totalLen = 11 + nameLen;

    for (size_t i = 0; i < static_cast<size_t>(MeshInterfaceId::MaxInterfaces); ++i) {
        if (m_interfaces[i].enabled) {
            auto* iface = cbdos::network::NetworkInterfaceManager::getInstance().getInterface(i);
            if (iface && iface->isReady()) {
                iface->sendPacket(buffer, totalLen);
            } else if (m_transports[i]) {
                m_transports[i]->sendRaw(nullptr, buffer, totalLen);
            }
        }
    }
}

bool MeshCoreEngine::sendMessage(uint16_t targetId, const std::string& text) {
    return sendMessage(targetId, m_activeChannelId, text);
}

bool MeshCoreEngine::sendMessage(uint16_t targetId, uint16_t channelId, const std::string& text) {
    if (!m_running || text.empty()) return false;

    uint32_t msgId = 0;
    std::string chName = "#general";
    bool isEnc = false;
    std::string pskKey = "";

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        msgId = m_nextMsgId++;

        for (const auto& ch : m_channels) {
            if (ch.id == channelId) {
                chName = ch.name;
                isEnc = ch.isPrivate;
                pskKey = ch.pskKey;
                break;
            }
        }

        MeshMessage msg;
        msg.id = msgId;
        msg.senderId = m_localShortId;
        msg.senderName = m_localName;
        msg.targetId = targetId;
        msg.channelId = channelId;
        msg.channelName = chName;
        msg.text = text;
        msg.timestampSec = static_cast<uint32_t>(cbdos::time::getEpoch());
        msg.isOutgoing = true;
        msg.isAcked = false;
        msg.isEncrypted = isEnc;
        if (m_messages.size() >= 100) {
            m_messages.erase(m_messages.begin());
        }
        m_messages.push_back(msg);

        markPacketSeen(msgId);

        if (m_msgCb) m_msgCb(msg);
    }

    // Cifrar payload si es canal privado
    std::string payloadToSend = text;
    if (isEnc && !pskKey.empty()) {
        payloadToSend = encryptPayload(text, pskKey);
    }

    // Armar trama de datos de chat
    // Magic(2) + Type(1) + Hops(1) + SrcId(2) + DstId(2) + ChannelId(2) + MsgId(4) + Flags(1) + PayloadLen(1) + Payload(N)
    uint8_t buffer[256];
    buffer[0] = static_cast<uint8_t>(MESHCORE_MAGIC & 0xFF);
    buffer[1] = static_cast<uint8_t>((MESHCORE_MAGIC >> 8) & 0xFF);
    buffer[2] = PKT_CHAT;
    buffer[3] = 0; // Hops iniciales (0 saltos)
    buffer[4] = static_cast<uint8_t>(m_localShortId & 0xFF);
    buffer[5] = static_cast<uint8_t>((m_localShortId >> 8) & 0xFF);
    buffer[6] = static_cast<uint8_t>(targetId & 0xFF);
    buffer[7] = static_cast<uint8_t>((targetId >> 8) & 0xFF);
    buffer[8] = static_cast<uint8_t>(channelId & 0xFF);
    buffer[9] = static_cast<uint8_t>((channelId >> 8) & 0xFF);
    buffer[10] = static_cast<uint8_t>(msgId & 0xFF);
    buffer[11] = static_cast<uint8_t>((msgId >> 8) & 0xFF);
    buffer[12] = static_cast<uint8_t>((msgId >> 16) & 0xFF);
    buffer[13] = static_cast<uint8_t>((msgId >> 24) & 0xFF);
    
    uint8_t flags = 0;
    if (isEnc) flags |= 0x01; // Bit 0: Encrypted
    buffer[14] = flags;

    size_t textLen = std::min(payloadToSend.length(), (size_t)200);
    buffer[15] = static_cast<uint8_t>(textLen);
    std::memcpy(&buffer[16], payloadToSend.c_str(), textLen);

    size_t totalLen = 16 + textLen;

    bool sentAny = false;
    for (size_t i = 0; i < static_cast<size_t>(MeshInterfaceId::MaxInterfaces); ++i) {
        if (m_interfaces[i].enabled) {
            auto* iface = cbdos::network::NetworkInterfaceManager::getInstance().getInterface(i);
            if (iface && iface->isReady()) {
                if (iface->sendPacket(buffer, totalLen) > 0) {
                    sentAny = true;
                }
            } else if (m_transports[i]) {
                if (m_transports[i]->sendRaw(nullptr, buffer, totalLen)) {
                    sentAny = true;
                }
            }
        }
    }

    return sentAny;
}

std::vector<MeshMessage> MeshCoreEngine::getMessages(uint16_t channelId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<MeshMessage> filtered;
    for (const auto& msg : m_messages) {
        if (msg.channelId == channelId) {
            filtered.push_back(msg);
        }
    }
    return filtered;
}

void MeshCoreEngine::clearMessages() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_messages.clear();
}

void MeshCoreEngine::handleRawPacket(MeshInterfaceId iface, const uint8_t* data, size_t len, int8_t rssi) {
    if (!data || len < 11) return;

    uint16_t magic = data[0] | (data[1] << 8);
    if (magic != MESHCORE_MAGIC) return;

    uint8_t type = data[2];
    uint8_t hops = data[3];
    uint16_t srcId = data[4] | (data[5] << 8);
    uint16_t dstId = data[6] | (data[7] << 8);

    if (srcId == m_localShortId) return; // Ignorar propios

    if (type == PKT_BEACON) {
        uint8_t payloadLen = data[10];
        if (len < (size_t)(11 + payloadLen)) return;
        std::string name(reinterpret_cast<const char*>(&data[11]), payloadLen);
        updateNode(srcId, name, NodeType::Client, rssi, hops, iface);
        return;
    }

    if (type == PKT_CHAT) {
        if (len < 16) return;
        uint16_t channelId = data[8] | (data[9] << 8);
        uint32_t msgId = data[10] | (data[11] << 8) | (data[12] << 16) | (data[13] << 24);
        uint8_t flags = data[14];
        bool isEncrypted = (flags & 0x01) != 0;
        uint8_t payloadLen = data[15];

        if (len < (size_t)(16 + payloadLen)) return;

        // Anti-duplicados y anti-bucles
        if (isPacketSeen(msgId)) {
            return;
        }
        markPacketSeen(msgId);

        // Actualizar nodo emisor
        updateNode(srcId, "", NodeType::Client, rssi, hops, iface);

        // Reenvío Multi-salto si hops < 7 y no era exclusivo para nosotros
        if (hops < 7 && dstId != m_localShortId) {
            uint8_t fwdBuffer[256];
            size_t totalLen = 16 + payloadLen;
            if (totalLen <= sizeof(fwdBuffer)) {
                std::memcpy(fwdBuffer, data, totalLen);
                fwdBuffer[3] = hops + 1; // Incrementar salto
                forwardPacket(iface, fwdBuffer, totalLen);
            }
        }

        // Si el paquete es para nosotros (broadcast o DM)
        if (dstId == 0xFFFF || dstId == m_localShortId) {
            std::string rawPayload(reinterpret_cast<const char*>(&data[16]), payloadLen);
            std::string finalText = rawPayload;
            std::string chName = "#general";

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                for (const auto& ch : m_channels) {
                    if (ch.id == channelId) {
                        chName = ch.name;
                        if (isEncrypted) {
                            if (!ch.pskKey.empty()) {
                                finalText = decryptPayload(rawPayload, ch.pskKey);
                            } else {
                                finalText = "🔒 [Mensaje Cifrado - Requiere Clave]";
                            }
                        }
                        break;
                    }
                }
            }

            if (channelId != 0 && chName == "#general") {
                // Canal desconocido
                if (isEncrypted) {
                    finalText = "🔒 [Mensaje Cifrado - Canal " + std::to_string(channelId) + "]";
                }
            }

            MeshMessage msg;
            msg.id = msgId;
            msg.senderId = srcId;
            msg.senderName = "Nodo 0x" + std::to_string(srcId);
            for (const auto& node : m_nodes) {
                if (node.shortId == srcId) {
                    msg.senderName = node.name;
                    break;
                }
            }
            msg.targetId = dstId;
            msg.channelId = channelId;
            msg.channelName = chName;
            msg.text = finalText;
            msg.timestampSec = static_cast<uint32_t>(cbdos::time::getEpoch());
            msg.isOutgoing = false;
            msg.isAcked = true;
            msg.isEncrypted = isEncrypted;
            msg.interfaceId = iface;

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (m_messages.size() >= 100) {
                    m_messages.erase(m_messages.begin());
                }
                m_messages.push_back(msg);
            }

            CBD_LOG_I(TAG, "[*] Mensaje MeshCore recibido de 0x%04X en '%s': %s", srcId, chName.c_str(), finalText.c_str());

            if (m_msgCb) {
                m_msgCb(msg);
            }
        }
    }
}

} // namespace meshcore
} // namespace apps
} // namespace cbdos

