import time
import struct
from typing import Optional, Dict, Any, Tuple
from services.base_service import BaseService, MeshContext

# Banderas de Estado del PoP (Byte 6)
POP_STATUS_INTERNET_UP  = (1 << 0) # 1 = Salida a Internet activa
POP_STATUS_PROXY_OPEN   = (1 << 1) # 1 = Proxy de navegación libre
POP_STATUS_BAAS_BUSY    = (1 << 2) # 1 = Servidor digestor web ocupado
POP_STATUS_ALERT_ACTIVE = (1 << 3) # 1 = Alerta activa en el tablón


class RoutingService(BaseService):
    def __init__(self, tower_id: int = 0x0001, tower_name: str = "Gateway CBDos Mesh", channel: int = 1, is_lr: bool = False):
        super().__init__(name="RoutingControl", service_id=0x0F)
        self.tower_id = tower_id
        self.tower_name = tower_name
        self.channel = channel
        self.is_lr = is_lr

        # Estado del PoP y Portada
        self.cover_hash = 0x0000
        self.status_code = POP_STATUS_INTERNET_UP | POP_STATUS_PROXY_OPEN
        self.last_broadcast_ts = 0

    def init_service(self, ctx: MeshContext):
        """Suscribirse a eventos de otros módulos (Hosting, Proxy, Alertas)."""
        ctx.event_bus.subscribe("cover_updated", self._on_cover_updated)
        ctx.event_bus.subscribe("internet_status_changed", self._on_internet_status_changed)
        ctx.event_bus.subscribe("alert_status_changed", self._on_alert_status_changed)

    def _on_cover_updated(self, data: Any):
        if isinstance(data, dict) and "hash" in data:
            self.cover_hash = int(data["hash"]) & 0xFFFF
        elif isinstance(data, int):
            self.cover_hash = data & 0xFFFF

    def _on_internet_status_changed(self, is_up: bool):
        if is_up:
            self.status_code |= POP_STATUS_INTERNET_UP
        else:
            self.status_code &= ~POP_STATUS_INTERNET_UP

    def _on_alert_status_changed(self, has_alert: bool):
        if has_alert:
            self.status_code |= POP_STATUS_ALERT_ACTIVE
        else:
            self.status_code &= ~POP_STATUS_ALERT_ACTIVE

    def generate_pop_broadcast(self) -> bytes:
        """
        Genera el Micro-Broadcast de 7 Bytes (Little-Endian):
        [0..3]: Unix Epoch (uint32_t LE)
        [4..5]: Hash Portada (uint16_t LE)
        [  6 ]: Status Code (uint8_t)
        """
        epoch = int(time.time())
        return struct.pack("<IHB", epoch, self.cover_hash, self.status_code)

    def handle_request(self, payload: bytes, client_entry: Optional[Dict[str, Any]], reply_short_id: int, ctx: MeshContext) -> Optional[Tuple[int, bytes]]:
        if not self.enabled or len(payload) < 1:
            return None

        tag = payload[0]

        # Tag 0x01: PROBE / ASSOC REQUEST (Legacy/Extendido)
        if tag == 0x01:
            assigned_ip = client_entry["ipv4"] if client_entry else "10.0.0.2"
            assigned_sid = client_entry["short_id"] if client_entry else reply_short_id

            resp = bytearray([0x02]) # Tag 0x02: PROBE / ASSOC RESPONSE
            resp += struct.pack(">H", self.tower_id)
            resp += bytes([self.channel, 0x02 if self.is_lr else 0x01])
            name_bytes = self.tower_name.encode('utf-8')[:31]
            resp += bytes([len(name_bytes)]) + name_bytes

            # Servicios activos + Epoch (8B LE)
            epoch = int(time.time())
            active_services = 0x03 # Internet (1) | Time (2)
            resp += bytes([active_services])
            resp += struct.pack("<Q", epoch)

            if ctx.debug:
                print(f"📡 [RoutingService] Asociación respondida a IP={assigned_ip} (ShortID=0x{assigned_sid:04X})")

            return (0x0F, bytes(resp))

        return None
