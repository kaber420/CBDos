#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace cbdos {
namespace apps {
namespace meshcore {

// ────────────────────────────────────────────────────────────────
// Identificadores de Interfaces / Ranuras de Radio
// ────────────────────────────────────────────────────────────────
enum class MeshInterfaceId : uint8_t {
    Interface1_Internal = 0, // Radio Interna (ESP-NOW 2.4 GHz)
    Interface2_Backpack = 1, // Mochila LoRa (SX1262 915/868 MHz o SX1280 2.4 GHz)
    Interface3_USB      = 2, // USB-CDC / Módem Serial TNC
    MaxInterfaces       = 3
};

enum class MeshInterfaceType : uint8_t {
    None = 0,
    EspNow,
    SX1262_LoRa,
    SX1280_FLRC,
    USB_CDC,
    UART_Serial
};

struct MeshInterfaceConfig {
    MeshInterfaceId id;
    std::string name;
    MeshInterfaceType type;
    bool enabled = false;
    uint8_t channel = 1;
    float frequency = 915.0f;
    uint32_t baudRate = 115200;
};

// ────────────────────────────────────────────────────────────────
// Tipos de Nodo en la Malla
// ────────────────────────────────────────────────────────────────
enum class NodeType : uint8_t {
    Client = 0,
    Repeater = 1,
    Gateway = 2
};

struct MeshNode {
    uint32_t uuid = 0;
    uint16_t shortId = 0;
    std::string name = "Desconocido";
    NodeType type = NodeType::Client;
    int8_t rssi = -128;
    uint8_t hops = 0;
    uint32_t lastSeenSec = 0;
    bool isDirect = true;
    MeshInterfaceId seenOnInterface = MeshInterfaceId::Interface1_Internal;
};

// ────────────────────────────────────────────────────────────────
// Mensajes de Chat y Canales
// ────────────────────────────────────────────────────────────────
struct MeshMessage {
    uint32_t id = 0;
    uint16_t senderId = 0;
    std::string senderName;
    uint16_t targetId = 0xFFFF; // 0xFFFF = Broadcast / General
    uint16_t channelId = 0;     // 0 = #general
    std::string channelName = "#general";
    std::string text;
    uint32_t timestampSec = 0;
    bool isOutgoing = false;
    bool isAcked = false;
    bool isEncrypted = false;
    MeshInterfaceId interfaceId = MeshInterfaceId::Interface1_Internal;
};

struct MeshChannel {
    uint16_t id = 0;
    std::string name = "#general";
    bool isPrivate = false;
    std::string pskKey; // Clave / Contraseña de cifrado para canales privados
};

} // namespace meshcore
} // namespace apps
} // namespace cbdos

