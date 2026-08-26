#!/usr/bin/env python3
"""
Servicio de Control de Ruteo y Señalización de Red (ServiceId: 0x0F) para CBDos.
Maneja handshakes de asociación de nodos, anuncios de torre y descubrimiento en el aire.
"""

import struct
from typing import Optional, Dict, Any, Tuple
from services.base_service import BaseService, MeshContext


class RoutingService(BaseService):
    def __init__(self, tower_id: int = 0x0001, tower_name: str = "Gateway CBDos Mesh", channel: int = 1, is_lr: bool = False):
        super().__init__(name="RoutingControl", service_id=0x0F)
        self.tower_id = tower_id
        self.tower_name = tower_name
        self.channel = channel
        self.is_lr = is_lr

    def handle_request(self, payload: bytes, client_entry: Optional[Dict[str, Any]], reply_short_id: int, ctx: MeshContext) -> Optional[Tuple[int, bytes]]:
        if not self.enabled or len(payload) < 1:
            return None

        tag = payload[0]

        # Tag 0x01: PROBE / ASSOC REQUEST
        if tag == 0x01:
            assigned_ip = client_entry["ipv4"] if client_entry else "10.0.0.2"
            assigned_sid = client_entry["short_id"] if client_entry else reply_short_id

            resp = bytearray([0x02]) # Tag 0x02: PROBE / ASSOC RESPONSE
            resp += struct.pack(">H", self.tower_id)
            resp += bytes([self.channel, 0x02 if self.is_lr else 0x01])
            name_bytes = self.tower_name.encode('utf-8')[:31]
            resp += bytes([len(name_bytes)]) + name_bytes

            if ctx.debug:
                print(f"📡 [RoutingService] Asociación respondida a IP={assigned_ip} (ShortID=0x{assigned_sid:04X})")

            return (0x0F, bytes(resp))

        return None
