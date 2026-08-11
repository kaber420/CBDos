"""
mesh_proto.py — Capa de red MeshHeader para el gateway TLVGL (Python)

Espejo exacto de firmware/src/Core/mesh_header.c. Decodifica la cabecera
binaria de pseudo-ruteo (3B, 9B, 13B, 21B) y las tramas TLV uplink/downlink,
para que el gateway hable el mismo protocolo binario que el firmware.

El formato respeta EXACTAMENTE el firmware real (mesh_header.h):
  bit 7  GLOBAL_BIT   → 21B inter-ASN
  bit 6  SIGNAL_BIT   → señalización
  bit 5  INTRA_ZONE   → 13B intra-zona OSPF
  bit 4  SHORT_ID     → IDs 2B (si no, UUID 4B)
  bit 3  DST_ONLY     → 3B ultra-ligera
  bits 0-3 service    → TLVGL_REQUEST 0x07, TLVGL_RESPONSE 0x08
"""

import struct

# ── Flags del Control byte (mesh_header.h) ──
MESH_CTRL_GLOBAL_BIT = (1 << 7)
MESH_CTRL_SIGNAL_BIT = (1 << 6)
MESH_CTRL_INTRA_ZONE = (1 << 5)
MESH_CTRL_SHORT_ID   = (1 << 4)
MESH_CTRL_DST_ONLY   = (1 << 3)

# ── Servicios (mesh_header.h) ──
MESH_SVC_CHAT           = 0x01
MESH_SVC_PROXY          = 0x05
MESH_SVC_TLVGL_REQUEST  = 0x07
MESH_SVC_TLVGL_RESPONSE = 0x08

# ── Tags TLV Downlink (tlv_parser.h) ──
TYPE_ABS_PAGE      = 0x10
TYPE_ABS_TEXT      = 0x11
TYPE_ABS_LINK      = 0x12
TYPE_ABS_INPUT     = 0x13
TYPE_ABS_IMAGE     = 0x14
TYPE_ABS_CHECKBOX  = 0x15
TYPE_ABS_SWITCH    = 0x16
TYPE_ABS_SLIDER    = 0x17
TYPE_ABS_PROGRESS  = 0x18
TYPE_ABS_DROPDOWN  = 0x19
TYPE_END           = 0xFE

# ── Tags TLV Uplink (tlv_parser.h) ──
TYPE_REQ_URL          = 0x01
TYPE_REQ_INPUT_SUBMIT = 0x20
TYPE_REQ_LINK_CLICK   = 0x21


def parse_mesh_header(data: bytes):
    """Decodifica la cabecera MeshHeader.

    Espejo de parse_mesh_header() de mesh_header.c. Devuelve
    (hdr_dict, hdr_len) o (None, 0) si la trama es inválida/corta.
    """
    if not data or len(data) < 1:
        return None, 0

    hdr = {
        'control': data[0],
        'service': data[0] & 0x0F,
        'flags': {
            'GLOBAL_BIT': bool(data[0] & MESH_CTRL_GLOBAL_BIT),
            'SIGNAL_BIT': bool(data[0] & MESH_CTRL_SIGNAL_BIT),
            'INTRA_ZONE': bool(data[0] & MESH_CTRL_INTRA_ZONE),
            'SHORT_ID':   bool(data[0] & MESH_CTRL_SHORT_ID),
            'DST_ONLY':   bool(data[0] & MESH_CTRL_DST_ONLY),
        },
        'src_asn': 0, 'dst_asn': 0,
        'src_zone': 0, 'dst_zone': 0,
        'src_tower': 0, 'dst_tower': 0,
        'src_id': 0, 'dst_id': 0,
        'is_short_id': False,
        'is_dst_only': False,
    }

    ctrl = data[0]
    offset = 1

    # Nivel 1: ultra-ligera 3B (DST_ONLY)
    if ctrl & MESH_CTRL_DST_ONLY:
        if offset + 2 > len(data):
            return None, 0
        hdr['dst_id'] = struct.unpack(">H", data[offset:offset + 2])[0]
        hdr['is_short_id'] = True
        hdr['is_dst_only'] = True
        return hdr, offset + 2

    # Nivel 4: inter-ASN global 21B
    if ctrl & MESH_CTRL_GLOBAL_BIT:
        if offset + 20 > len(data):
            return None, 0
        (hdr['src_asn'], hdr['src_zone'], hdr['src_tower'],
         hdr['src_id']) = _unpack_addr(data[offset:offset + 10])
        offset += 10
        (hdr['dst_asn'], hdr['dst_zone'], hdr['dst_tower'],
         hdr['dst_id']) = _unpack_addr(data[offset:offset + 10])
        offset += 10
        return hdr, offset

    # Niveles 2 y 3: intra-zona OSPF
    if ctrl & MESH_CTRL_INTRA_ZONE:
        if offset + 8 > len(data):
            return None, 0
        hdr['src_tower'] = struct.unpack(">H", data[offset:offset + 2])[0]
        hdr['dst_tower'] = struct.unpack(">H", data[offset + 2:offset + 4])[0]
        if ctrl & MESH_CTRL_SHORT_ID:
            # 9B: short ids 2B
            if offset + 8 > len(data):
                return None, 0
            hdr['src_id'] = struct.unpack(">H", data[offset + 4:offset + 6])[0]
            hdr['dst_id'] = struct.unpack(">H", data[offset + 6:offset + 8])[0]
            hdr['is_short_id'] = True
            return hdr, offset + 8
        else:
            # 13B: uuid 4B
            if offset + 12 > len(data):
                return None, 0
            hdr['src_id'] = struct.unpack(">I", data[offset + 4:offset + 8])[0]
            hdr['dst_id'] = struct.unpack(">I", data[offset + 8:offset + 12])[0]
            return hdr, offset + 12

    # Nivel 2 estándar local 9B (dst uuid + src uuid)
    if offset + 8 > len(data):
        return None, 0
    hdr['dst_id'] = struct.unpack(">I", data[offset:offset + 4])[0]
    hdr['src_id'] = struct.unpack(">I", data[offset + 4:offset + 8])[0]
    return hdr, offset + 8


def _unpack_addr(block: bytes):
    """Desempaqueta un bloque de dirección de 10B: asn(2)+zone(2)+tower(2)+id(4)."""
    asn, zone, tower = struct.unpack(">HHH", block[0:6])
    node_id = struct.unpack(">I", block[6:10])[0]
    return asn, zone, tower, node_id


def build_mesh_header(control: int, dst_id: int) -> bytes:
    """Construye la cabecera de respuesta DST_ONLY de 3B.

    Igual que build_mesh_header() del firmware para el caso ultra-ligero
    usado por TlvBrowserView. `dst_id` es el Short ID de 2B.
    """
    return struct.pack(">BH", control, dst_id & 0xFFFF)


def build_tlv_node(tag: int, payload: bytes) -> bytes:
    return struct.pack(">BH", tag, len(payload)) + payload


def parse_uplink_tlv(data: bytes):
    """Parsea el primer nodo TLV uplink: devuelve (tag, value) o (None, None)."""
    if len(data) < 3:
        return None, None
    tag = data[0]
    length = struct.unpack(">H", data[1:3])[0]
    if 3 + length > len(data):
        return None, None
    return tag, data[3:3 + length]


def build_response_header(control: int = 0x08) -> bytes:
    """Cabecera de respuesta estándar para TlvBrowserView.

    El firmware valida service = control & 0x3F == MESH_SVC_TLVGL_RESPONSE.
    Con ctrl = 0x08 (DST_ONLY + service 0x08), parse_mesh_header lo lee
    como 3B y processNetworkPacket renderiza payload desde offset 3.
    """
    return build_mesh_header(control, 0x0001)
