#pragma once

#include "MeshCoreTypes.hpp"
#include "cbdos/mesh/mesh_transport.hpp"
#include <vector>
#include <string>
#include <mutex>
#include <functional>
#include <memory>

namespace cbdos {
namespace apps {
namespace meshcore {

using MessageReceivedCallback = std::function<void(const MeshMessage&)>;
using NodeDiscoveredCallback  = std::function<void(const MeshNode&)>;
using StatusChangedCallback   = std::function<void()>;

class MeshCoreEngine {
public:
    static MeshCoreEngine& getInstance();

    // ────────────────────────────────────────────────────────────
    // Control del Motor y Ciclo de Vida (Offline-First)
    // ────────────────────────────────────────────────────────────
    bool init();
    void stop();
    bool isRunning() const { return m_running; }

    // ────────────────────────────────────────────────────────────
    // Gestión Multi-Interfaz (Radio 1, Radio 2, USB)
    // ────────────────────────────────────────────────────────────
    bool isInterfaceEnabled(MeshInterfaceId id) const;
    void setInterfaceEnabled(MeshInterfaceId id, bool enable);
    MeshInterfaceConfig getInterfaceConfig(MeshInterfaceId id) const;
    void setInterfaceConfig(const MeshInterfaceConfig& cfg);
    size_t getActiveInterfaceCount() const;

    // Inyección de Transporte HAL (ESP-NOW, SX1262, USB-CDC)
    void setTransport(MeshInterfaceId id, cbdos::mesh::IMeshTransport* transport);

    // ────────────────────────────────────────────────────────────
    // Identidad y Nodos
    // ────────────────────────────────────────────────────────────
    void setLocalIdentity(uint16_t shortId, const std::string& name);
    uint16_t getLocalShortId() const { return m_localShortId; }
    std::string getLocalName() const { return m_localName; }

    std::vector<MeshNode> getNodes();
    void sendBeacon(); // Emite baliza Hello/Presence

    // ────────────────────────────────────────────────────────────
    // Mensajería y Canales
    // ────────────────────────────────────────────────────────────
    bool sendMessage(uint16_t targetId, const std::string& text);
    bool sendMessage(uint16_t targetId, uint16_t channelId, const std::string& text);
    std::vector<MeshMessage> getMessages(uint16_t channelId = 0);
    void clearMessages();

    // Gestión de Canales
    std::vector<MeshChannel> getChannels();
    bool addChannel(const std::string& name, bool isPrivate, const std::string& key = "");
    bool removeChannel(uint16_t id);
    uint16_t getActiveChannelId() const { return m_activeChannelId; }
    void setActiveChannelId(uint16_t id);
    std::string getActiveChannelName() const;

    // Callbacks para la UI
    void setMessageCallback(MessageReceivedCallback cb) { m_msgCb = cb; }
    void setNodeCallback(NodeDiscoveredCallback cb) { m_nodeCb = cb; }
    void setStatusCallback(StatusChangedCallback cb) { m_statusCb = cb; }

    // Procesamiento de paquetes crudos recibidos desde la HAL
    void handleRawPacket(MeshInterfaceId iface, const uint8_t* data, size_t len, int8_t rssi);

private:
    MeshCoreEngine();
    ~MeshCoreEngine() = default;
    MeshCoreEngine(const MeshCoreEngine&) = delete;
    MeshCoreEngine& operator=(const MeshCoreEngine&) = delete;

    void updateNode(uint16_t shortId, const std::string& name, NodeType type, int8_t rssi, uint8_t hops, MeshInterfaceId iface);
    std::string encryptPayload(const std::string& text, const std::string& key);
    std::string decryptPayload(const std::string& cipherText, const std::string& key);
    bool isPacketSeen(uint32_t msgId);
    void markPacketSeen(uint32_t msgId);
    void forwardPacket(MeshInterfaceId incomingIface, const uint8_t* data, size_t len);

    bool m_running = false;
    uint16_t m_localShortId = 0x1337;
    std::string m_localName = "Cyberdeck";
    uint32_t m_nextMsgId = 1;
    uint16_t m_activeChannelId = 0; // 0 = #general
    uint16_t m_nextChannelId = 1;

    MeshInterfaceConfig m_interfaces[static_cast<size_t>(MeshInterfaceId::MaxInterfaces)];
    cbdos::mesh::IMeshTransport* m_transports[static_cast<size_t>(MeshInterfaceId::MaxInterfaces)] = {nullptr, nullptr, nullptr};

    std::vector<MeshNode> m_nodes;
    std::vector<MeshMessage> m_messages;
    std::vector<MeshChannel> m_channels;

    uint32_t m_seenPacketIds[64] = {0};
    size_t m_seenIdx = 0;

    mutable std::mutex m_mutex;
    MessageReceivedCallback m_msgCb = nullptr;
    NodeDiscoveredCallback  m_nodeCb = nullptr;
    StatusChangedCallback   m_statusCb = nullptr;
};

} // namespace meshcore
} // namespace apps
} // namespace cbdos

