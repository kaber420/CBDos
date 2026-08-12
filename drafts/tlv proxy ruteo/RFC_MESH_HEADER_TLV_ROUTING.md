# RFC 0001: Binary Mesh Routing and TLV Payload Frame Specification (LoRa, FLRC, TCP/IP)

**Status:** Standard Technical Specification  
**Date:** August 2026  
**Authors:** espOS32 Core Architecture Team  
**Category:** Networking / Protocol Specification  

---

## 1. ABSTRACT & SCOPE

This specification defines the **Binary MeshHeader Protocol** and the **TLV Payload Structure** used for routing, presentation, and event transport across the `espOS32` ecosystem.

The primary goal of this architecture is **Transport Independence**: the same binary frame structure flows identically whether transmitted across low-speed long-range radio links (SX1262 LoRa @ 915MHz, SX1280 FLRC @ 2.4GHz) or high-speed IP networks (WiFi, Ethernet, TCP/IP sockets, Unix Domain Sockets).

---

## 2. PROTOCOL LAYERING ARCHITECTURE

```text
┌──────────────────────────────────────────────────────────────────┐
│                   TLVGL Application Layer                        │
│   (Uplink: REQ_URL, REQ_SUBMIT | Downlink: "PH" + Widget TLVs)   │
├──────────────────────────────────────────────────────────────────┤
│                     MeshHeader Network Layer                     │
│    (Addressing Levels 1..4, Pseudo-BGP inter-ASN / OSPF intra)   │
├──────────────────────────────────────────────────────────────────┤
│                    Transport Abstraction Layer                   │
│   ┌───────────────────────┬───────────────────┬──────────────┐   │
│   │ TCP/IP Socket (:8765) │ Radio FLRC 2.4GHz │ LoRa 915MHz  │   │
│   └───────────────────────┴───────────────────┴──────────────┘   │
└──────────────────────────────────────────────────────────────────┘
```

---

## 3. MESHHEADER NETWORK LAYER SPECIFICATION

### 3.1. Control Byte Bitfield Layout

Every frame begins with a **1-byte Control Byte** (or up to 3 bytes in extended levels) defining routing behavior, addressing modes, and service identification.

```text
 7       6       5       4       3       2       1       0
┌───────┬───────┬───────┬───────┬───────┬─────────────────┐
│ GLOBAL│ SIGNAL│ INTRA │ SHORT │ DST   │   SERVICE ID    │
│  BIT  │  BIT  │ ZONE  │  ID   │ ONLY  │   (Bits 3..0)   │
└───────┴───────┴───────┴───────┴───────┴─────────────────┘
```

| Bit Field | Mask | Name | Description |
|---|---|---|---|
| **Bit 7** | `0x80` | `MESH_CTRL_GLOBAL_BIT` | 1 = Global Inter-ASN BGP Routing (21-byte header) |
| **Bit 6** | `0x40` | `MESH_CTRL_SIGNAL_BIT` | 1 = Control / Signaling Packet, 0 = Data Packet |
| **Bit 5** | `0x20` | `MESH_CTRL_INTRA_ZONE` | 1 = Intra-Zone OSPF Routing (13-byte or 9-byte header) |
| **Bit 4** | `0x10` | `MESH_CTRL_SHORT_ID` | 1 = Node IDs are 2-byte Short IDs; 0 = 4-byte UUIDs |
| **Bit 3** | `0x08` | `MESH_CTRL_DST_ONLY` | 1 = Ultra-light local header (3-byte total) |
| **Bits 3..0** | `0x0F` | `SERVICE_ID` | Service Identifier (0x07 = TLVGL REQ, 0x08 = TLVGL RESP, 0x05 = Proxy, 0x01 = Chat) |

---

### 3.2. Service ID Mapping Table

| Service ID | Hex Code | Purpose | Direction |
|---|---|---|---|
| `SvcChat` | `0x01` | Mesh P2P Text Chat | Bidirectional |
| `SvcProxy` | `0x05` | HTTP Proxy Relay Request | Uplink |
| `SvcTlvglRequest` | `0x07` | TLVGL Browser Navigation Request | Uplink (ESP32 → Gateway) |
| `SvcTlvglResponse` | `0x08` | TLVGL Rendered Page Payload | Downlink (Gateway → ESP32) |

---

### 3.3. Addressing Levels and Header Wire Formats

The header length is dynamically calculated from the Control Byte flags:

#### Level 1: Ultra-Lightweight Local Header (3 Bytes)
Used for local single-hop RF radio transmissions or direct client-gateway links:

```text
 0               1               2               3 (Bytes)
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Control Byte |      Destination Short ID     |
| (0x0F / 0x08) |           (16 Bits)           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

#### Level 2: Standard Local Header (9 Bytes)
Used for multi-hop local network delivery with 32-bit UUIDs:

```text
 0               1                               5               9 (Bytes)
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Control Byte |           Dst UUID            |   Src UUID    |
|    (0x00)     |           (32 Bits)           |   (32 Bits)   |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

#### Level 3: Intra-Zone OSPF Header (13 Bytes or 9 Bytes Short)
Used for routing across towers/nodes within the same Autonomous System (ASN):

```text
 0               1               3               5               9              13 (Bytes)
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Control Byte |   Src Tower   |   Dst Tower   |   Src UUID    |   Dst UUID    |
| (0x20 / 0x30) |   (16 Bits)   |   (16 Bits)   |   (32 Bits)   |   (32 Bits)   |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

#### Level 4: Inter-ASN Global BGP Header (21 Bytes)
Used for WAN routing between distinct Autonomous Systems:

```text
 0               1               3               5               7              11 (Bytes)
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Control Byte |    Src ASN    |   Src Zone    |   Src Tower   |   Src UUID    |
|    (0x80)     |   (16 Bits)   |   (16 Bits)   |   (16 Bits)   |   (32 Bits)   |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|    Dst ASN    |   Dst Zone    |   Dst Tower   |   Dst UUID    |
|   (16 Bits)   |   (16 Bits)   |   (16 Bits)   |   (32 Bits)   |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

---

## 4. TLV APPLICATION PAYLOAD SPECIFICATION

Application data is encoded using Type-Length-Value (TLV) tuples.

```text
 0               1               3               3 + Length (Bytes)
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|   Tag Type    |    Payload Length (16-bit)    | Value Bytes   |
|   (1 Byte)    |          Big-Endian           | (Length Bytes)|
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

### 4.1. Uplink Event Tags (ESP32 → Gateway)

Uplink payloads contain a single event node:

| Tag Hex | Constant Name | Payload Format | Description |
|---|---|---|---|
| `0x01` | `TYPE_REQ_URL` | `[URL Bytes...]` | Request navigation to `.mesh` URL |
| `0x20` | `TYPE_REQ_INPUT_SUBMIT` | `[ElemID: 1B] [Text Bytes...]` | Form submission event |
| `0x21` | `TYPE_REQ_LINK_CLICK` | `[LinkID: 1B]` | Hyperlink click event |

Example Wire Frame (Uplink Navigation Request):
```text
[MeshHeader 3B] 0F 00 01 | [TLV Node 20B] 01 00 11 68 74 74 70 3a 2f 2f 69 6e 64 65 78 2e 6d 65 73 68
```

---

### 4.2. Downlink Page Tags (Gateway → ESP32)

Downlink payloads MUST begin with the **2-byte Magic Header `'P' 'H'` (`0x50 0x48`)**, followed by widget nodes, and MUST be terminated by `TYPE_END` (`0xFE`).

| Tag Hex | Constant Name | Payload Format | LVGL Rendering Mapping |
|---|---|---|---|
| `0x10` | `TYPE_ABS_PAGE` | None (0 bytes) | Root Page Container (`lv_obj_create`) |
| `0x1A` | `TYPE_ABS_PANEL` | `[X:2][Y:2][W:2][H:2][BgColor:2]` | Container Panel (`lv_obj_create`) |
| `0x11` | `TYPE_ABS_TEXT` | `[X:2][Y:2][W:2][H:2][Style:1][Text...]` | Label Widget (`lv_label_create`) |
| `0x12` | `TYPE_ABS_LINK` | `[X:2][Y:2][W:2][H:2][LinkID:1][Text...]` | Button Widget (`lv_button_create`) |
| `0x13` | `TYPE_ABS_INPUT` | `[X:2][Y:2][W:2][H:2][Act\0Name\0Placeholder]` | TextArea Widget (`lv_textarea_create`) |
| `0xFE` | `TYPE_END` | None (0 bytes) | End of Frame Stream Marker |

Example Wire Frame (Downlink Page Response):
```text
[MeshHeader 3B] 08 00 01 | [Magic 2B] 50 48 | [Page 3B] 10 00 00 | [Text 20B] 11 00 11 ... | [End 1B] FE
```

---

## 5. ROUTER ENGINE & FORWARDING ALGORITHM

The Router Engine (`gateway/router-go/pkg/router`) maintains two isolated routing tables:
1. `serviceMap[byte]`: Mappings from `ServiceID` to backend service connection handlers (e.g. `SvcTlvglRequest` `0x07` → Hosting Backend `:8766`).
2. `nodeRoutes[uint32]`: Mappings from Node Short IDs / UUIDs to active client socket connections (`net.Conn`).

```text
                      Incoming Mesh Packet
                               │
                       Parse MeshHeader
                               │
               ┌───────────────┴───────────────┐
               ▼                               ▼
      Is Request Service?             Is Response / Node Direct?
   (ServiceID == 0x07 / 0x05)        (ServiceID == 0x08 / Node Traffic)
               │                               │
               ▼                               ▼
     Lookup serviceMap[svc]           Lookup nodeRoutes[DstID]
               │                               │
               ▼                               ▼
     Forward to Backend              Forward to Client
      Handler (:8766)             Socket / Radio Link
```

---

## 6. EMPIRICAL VALIDATION & CONFORMANCE

Conformance to this RFC is verified via automated end-to-end integration tests (`test_router_e2e.py`) and live serial monitor telemetry on physical ESP32-S3 hardware:

```text
[ESP32 Hardware Telemetry /dev/ttyACM0]
[TLV Client] Conectando a Pasarela 192.168.66.254:8765...
[TLV Client] Enviados 23 bytes a la pasarela.
[TLV Client] Recibidos 586 bytes desde la pasarela.

[Go Router Log]
2026/08/11 20:51:28 [Router] Nueva conexión aceptada desde 192.168.66.248:63114 (tcp)
2026/08/11 20:51:28 [Router] 🔀 Trama de 23B reenviada con éxito a DstID=0x00000001 (Servicio=0x07)
2026/08/11 20:51:28 [Router] 🔀 Trama de 586B reenviada con éxito a DstID=0x00000001 (Servicio=0x08)
```
