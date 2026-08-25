#!/usr/bin/env python3
"""
Módulo de protocolo de cabeceras de red mesh y tramas TLV para CBDos.
"""

import struct

# Tags Downlink TLV
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
TYPE_ABS_PANEL     = 0x1A
TYPE_ABS_CHART     = 0x1B
TYPE_ABS_ARC       = 0x1C
TYPE_ABS_SPINNER   = 0x1D
TYPE_END           = 0xFE

# Tags Uplink TLV
TYPE_REQ_URL          = 0x01
TYPE_REQ_INPUT_SUBMIT = 0x20
TYPE_REQ_LINK_CLICK   = 0x21
TYPE_REQ_CONTROL_EVT  = 0x22

# Flags de Control MeshHeader
MESH_CTRL_GLOBAL_BIT   = 0x80 # Inter-ASN 21B
MESH_CTRL_SIGNAL_BIT   = 0x40 # Control
MESH_CTRL_INTRA_ZONE   = 0x20 # Intra-zona 9B/13B
MESH_CTRL_SHORT_ID     = 0x10 # Short ID (2B)
MESH_CTRL_DST_ONLY     = 0x08 # Ultra-ligero local (3B)

# Service IDs
MESH_SVC_CHAT           = 0x01
MESH_SVC_PROXY          = 0x05
MESH_SVC_TLVGL_REQUEST  = 0x07
MESH_SVC_TLVGL_RESPONSE = 0x08


def parse_mesh_header(data: bytes):
    """
    Decodifica una cabecera MeshHeader.
    Devuelve (dict_campos, header_len) o (None, 0).
    """
    if len(data) < 3:
        return None, 0

    ctrl = data[0]
    is_global = bool(ctrl & MESH_CTRL_GLOBAL_BIT)
    is_intra = bool(ctrl & MESH_CTRL_INTRA_ZONE)
    is_dst_only = bool(ctrl & MESH_CTRL_DST_ONLY)
    is_short = bool(ctrl & MESH_CTRL_SHORT_ID)

    if is_dst_only:
        dst_id = struct.unpack(">H", data[1:3])[0]
        return {
            'control': ctrl,
            'service': ctrl & 0x07,
            'src_id': 0,
            'dst_id': dst_id,
            'type': 'DST_ONLY_3B'
        }, 3

    if not is_global and not is_intra:
        if is_short and len(data) >= 5:
            dst_id, src_id = struct.unpack(">HH", data[1:5])
            return {
                'control': ctrl,
                'service': ctrl & 0x07,
                'src_id': src_id,
                'dst_id': dst_id,
                'type': 'LOCAL_SHORT_5B'
            }, 5
        elif len(data) >= 9:
            dst_id, src_id = struct.unpack(">II", data[1:9])
            return {
                'control': ctrl,
                'service': ctrl & 0x07,
                'src_id': src_id,
                'dst_id': dst_id,
                'type': 'LOCAL_UUID_9B'
            }, 9

    return None, 0


def build_response_header(control: int, dst_id: int) -> bytes:
    """Construye una cabecera de respuesta ultra-ligera DST_ONLY de 3 bytes."""
    return struct.pack(">BH", control | MESH_CTRL_DST_ONLY, dst_id & 0xFFFF)


def parse_uplink_tlv(data: bytes):
    """
    Decodifica la primera trama TLV de una petición uplink.
    Devuelve (tag, payload_bytes) o (None, None).
    """
    if len(data) < 3:
        return None, None

    tag = data[0]
    length = struct.unpack(">H", data[1:3])[0]
    if len(data) < 3 + length:
        return None, None

    value = data[3:3 + length]
    return tag, value
