#!/usr/bin/env python3
"""
GATEWAY-ROUTER de la red mesh (Pseudo-BGP / Pseudo-OSPF).
Rutea tramas MeshHeader según el DESTINO del paquete.

Cabecera VARIABLE según flags del Control byte (mismo formato que
firmware/src/Core/mesh_header.c):
  3B  DST_ONLY   (local, tabla de torre: Short ID dst)
  9B  local std  (UUID dst + UUID src)            [Nivel 1]
  13B INTRA_ZONE (torre src + torre dst + UUIDs)  [Nivel 2/3 OSPF]
  21B GLOBAL BIT (ASN + zona + torre + UUID ×2)   [Nivel 4 BGP]

Decisión de ruteo (plan_red_mesh_routing.md §Capas):
  - LOCAL (DST_ONLY):  es para reap-a nosotros. Entregar al servicio.
  - INTRA_ZONE:        mirar torre destino → reenviar al siguiente salto OSPF.
  - GLOBAL:            mirar ASN destino → reenviar al next-hop BGP.
  - Otro:              reenviar opaco (no abrimos el payload).

Uso:
  python3 gateway_router.py                    # abre un TCP server en :8765
  python3 gateway_router.py --tcp :8765 --lora /dev/ttyUSB0 ... (futuro)
"""

import socket
import struct
import sys
import threading
import time

# ── Tags de servicio (bits 0-3 del Control) ──
MESH_SVC_CHAT           = 0x01
MESH_SVC_PROXY          = 0x05
MESH_SVC_TLVGL_REQUEST  = 0x07
MESH_SVC_TLVGL_RESPONSE = 0x08

# ── CONTROL BYTE — 3 NIVELES priorizados (§3.3 Espec) ──
# Nivel 1 (1B) = SOLO ruteo y lo crítico/común (máxima velocidad):
MESH_CTRL_GLOBAL_BIT = 1 << 7   # 0x80 inter-ASN (ruteo, prioridad máx)
MESH_CTRL_SRC_SHORT  = 1 << 6   # 0x40 origen = Short ID 2B (ID por punta)
MESH_CTRL_INTRA_ZONE = 1 << 5   # 0x20 OSPF intra-zona (ruteo)
MESH_CTRL_DST_SHORT  = 1 << 4   # 0x10 destino = Short ID 2B (ID por punta)
MESH_CTRL_DST_ONLY   = 1 << 3   # 0x08 ultra-ligera local 3B
# bits 2-0 = servicio
#
# 2 bits de ID por PUNTA → 4 estados (la petición de combos variados):
#   SRC_SHORT | DST_SHORT | identidad                       cabecera
#      0      |    0      | src UUID 4B + dst UUID 4B        21/13B
#      0      |    1      | src UUID 4B + dst SHORT 2B       19/11B
#      1      |    0      | src SHORT 2B + dst UUID 4B       19/11B
#      1      |    1      | src SHORT 2B + dst SHORT 2B      17/9B
# SIGNAL y lo que no cupo/subcomún → Nivel 2 (0xFF + ext). Futuro → Nivel 3.

HOST = "0.0.0.0"
PORT = 8765

# ── "Este nodo" (identidad local del router-gateway) ──
MY_NODE = {
    "asn":   0x0001,
    "zone":  0x0001,
    "tower": 0x00FE,          # torre local
    "uuid":  0x000000FE,      # UUID del gateway
    "short": 0x00FE,          # Short ID que conoce la torre
}

# ─────────────────────────────────────────────────────────────
# INTERFACES DE RUTEO
# Cada ruta apunta a un nombre de interfaz. La interfaz define el
# ENLACE FÍSICO y su CABECERA, clave táctica del diseño:
#
#   LOCAL   → radio 2.4G FLRC/LoRa (lenta, cientos de kbps). Se usa
#             Short ID (2B) en la cabecera (3B/9B) para ahorrar aire.
#             Solo relevantes las máquinas en mi ASN/zona.
#
#   TRUNK   → WiFi (decenas/cientos de Mbps) o VPN. Enlace troncal a
#             OTRA zona / otro ASN. Aquí NO se escatiman bytes: se manda
#             la cabecera COMPLETA (13B intra-zona / 21B global) con
#             UUID + ASN + zona + torre, porque el ancho de banda sobra.
#             Por eso lo corto es para la radio, lo largo es para el trunk.
# ─────────────────────────────────────────────────────────────

# ── Registro de interfaces físicas ──
#   local : hacia los clientes (::677 y futuros) conectados a la radio local
#   trunk : enlaces WiFi/VPN de alta velocidad a otras zonas/ASN
INTERFACES = {
    "radio-local": {"type": "radio", "sw": "especificar", "speed_hint": "kbps"},
    "trunk-a":     {"type": "wifi",  "peer": ("192.168.1.50", 8765), "speed_hint": "mbps"},
    "trunk-b":     {"type": "wifi",  "peer": ("192.168.1.60", 8765), "speed_hint": "mbps"},
    "trunk-c":     {"type": "vpn",   "peer": ("203.0.113.20", 8765), "speed_hint": "mbps"},
}

# ── Tabla Pseudo-BGP: ASN dst → interfaz (trunk WiFi/VPN, Mbps) ──
BGP_TABLE = {
    0x0001: "radio-local",    # este ASN → bajar a OSPF/entregar
    0x0042: "trunk-a",        # ASN vecino → trunk WiFi
    0x00FF: "trunk-b",
    0x0100: "trunk-c",        # ASN lejano → VPN
}

# ── Tabla Pseudo-OSPF: torre dst → interfaz (zona/torre) ──
OSPF_TABLE = {
    0x00FE: "radio-local",    # torre local → entregar por radio
    0x00A3: "trunk-a",        # torre vecina → trunk WiFi
    0x00B1: "trunk-b",
}

# ── TABLA DE CLIENTES LOCALES (equiv. a una tabla ARP) ──
# Mapea la identificación del cliente a la interfaz donde está (radio local).
# Dos claves posibles:
#   "short": Short ID (2B) asignado por la torre al establecer sesión (Fase 1→2).
#   "uuid":  UUID (4B) del cliente.
# "peer": dónde entregarlo realmente (socket TCP por WiFi del cliente ESP32).
CLIENT_TABLE = {
    0x0001: {"short": 0x0001, "uuid": 0x00000001, "iface": "radio-local",
             "peer": ("192.168.1.147", 8765)},
    0x1234: {"short": 0x1234, "uuid": 0x00001234, "iface": "radio-local",
             "peer": ("10.0.0.5", 8765)},
}
# Lookup rápido uuid(4B int) → entrada / interfaz / peer
def _client_by_uuid(u: int):
    for e in CLIENT_TABLE.values():
        if e["uuid"] == u:
            return e
    return None
def _client_by_short(s: int):
    return CLIENT_TABLE.get(s)
def _uuid_peer(u: int):
    e = _client_by_uuid(u)
    return e["peer"] if e else None
def _short_peer(s: int):
    e = _client_by_short(s)
    return e["peer"] if e else None


class GatewayRouter:
    def __init__(self):
        self.peers = {}        # next-hop name → (host, port) reales del peer

    # ─────────────────────────────────────────────
    # CAPA DE RED: parsear la cabecera VARIABLE
    # ─────────────────────────────────────────────
    def parse_header(self, data: bytes):
        """Devuelve (offset, control, service, dst) o (None,...).

        CABECERA DE CONTROL VARIABLE EN 3 NIVELES (Espec §3.3):
          Nivel 1: data[0]            (1B)  control compacto 0x00-0xFE
          Nivel 2: 0xFF + ext[1]      (2B)  control extendido
          Nivel 3: 0xFF 0xFF + ext[2] (3B)  control ultra-extendido
        El 0xFF se detecta ANTES de interpretar flags.
        """
        if not data:
            return None

        hdr = 0           # bytes consumidos por el control (1/2/3)
        while hdr < 3 and hdr < len(data) and data[hdr] == 0xFF:
            hdr += 1
        if hdr >= len(data) or (hdr == 3 and data[2] == 0xFF):
            return None   # sin byte de control final
        ctrl = data[hdr]
        if ctrl == 0xFF:
            return None
        hdr += 1

        service = ctrl & 0x0F   # bits 0-3 (ver comentario en TlvBrowserView)
        output = {
            "level":    hdr,   # 1,2,3 = niveles de control consumidos
            "control":  ctrl,
            "ctrl_start": hdr, # offset donde empieza la parte de direccion
            "service":  service,
            "flags": {
                "GLOBAL":   bool(ctrl & MESH_CTRL_GLOBAL_BIT),
                "SRC_SHORT":bool(ctrl & MESH_CTRL_SRC_SHORT),
                "INTRA":    bool(ctrl & MESH_CTRL_INTRA_ZONE),
                "DST_SHORT":bool(ctrl & MESH_CTRL_DST_SHORT),
                "DST_ONLY": bool(ctrl & MESH_CTRL_DST_ONLY),
            },
            "dst_short": None,
            "asn_dst": None,
            "tower_dst": None,
            "uuid_dst": None,
        }
        off = hdr   # la direccion arranca tras el control (1/2/3B)

        # Nivel 1: ultra-ligera 3B (short dst)
        if output["flags"]["DST_ONLY"]:
            if len(data) < off + 2:
                return None
            output["offset"] = off + 2
            output["dst_short"] = struct.unpack(">H", data[off:off+2])[0]
            return output

        # GLOBAL (inter-ASN). Soporte dual:
        # GLOBAL (inter-ASN). Un extremo REAL usa la identidad completa:
        #   ASN(2)+Zona(2)+Torre(2)=6B + campo-ID final =
        #     Short ID (2B) si ese extremo pone el bit X_SHORT
        #     UUID     (4B) si ese extremo NO pone el bit X_SHORT
        # Así se expresan los 4 modos (§3.3): 21B / 19B / 19B / 17B.
        if output["flags"]["GLOBAL"]:
            # -- origen -- (ASN+Zona+Torre 6B + campo-ID)
            src_asn, src_zone, src_tower = struct.unpack(">HHH", data[off:off+6])
            off += 6
            if output["flags"]["SRC_SHORT"]:
                output["src_short"] = struct.unpack(">H", data[off:off+2])[0]
                off += 2
            else:
                off += 4   # uuid src (opaco)
            # -- destino -- (ASN+Zona+Torre 6B + campo-ID)
            dst_asn, dst_zone, dst_tower = struct.unpack(">HHH", data[off:off+6])
            off += 6
            if output["flags"]["DST_SHORT"]:
                output["dst_short"] = struct.unpack(">H", data[off:off+2])[0]
                off += 2
            else:
                output["uuid_dst"] = data[off:off+4]
                off += 4
            output["offset"] = off
            output["asn_dst"] = dst_asn
            output["tower_dst"] = dst_tower
            return output

        # INTRA_ZONE (OSPF): torre src(2) + torre dst(2) + campo-ID src +
        #   campo-ID dst. Short 2B o UUID 4B según cada bit X_SHORT.
        if output["flags"]["INTRA"]:
            src_tower = struct.unpack(">H", data[off:off+2])[0]
            dst_tower = struct.unpack(">H", data[off+2:off+4])[0]
            off += 4
            if output["flags"]["SRC_SHORT"]:
                output["src_short"] = struct.unpack(">H", data[off:off+2])[0]
                off += 2
            else:
                off += 4   # uuid src (opaco)
            if output["flags"]["DST_SHORT"]:
                output["dst_short"] = struct.unpack(">H", data[off:off+2])[0]
                off += 2
            else:
                output["uuid_dst"] = data[off:off+4]
                off += 4
            output["offset"] = off
            output["tower_dst"] = dst_tower
            return output

        # Cabecera local (misma torre, sin flag DST_ONLY/GLOBAL/INTRA):
        #   dst field = UUID 4B (DST_SHORT=0) o Short 2B (DST_SHORT=1)
        if output["flags"]["DST_SHORT"]:
            if len(data) < off + 2:
                return None
            output["offset"] = off + 2
            output["dst_short"] = struct.unpack(">H", data[off:off+2])[0]
        else:
            if len(data) < off + 4:
                return None
            output["offset"] = off + 4
            output["uuid_dst"] = data[off:off+4]
        return output

    # ─────────────────────────────────────────────
    # CAPA DE RUTEO: decidir por destino
    # ─────────────────────────────────────────────
    def route_or_forward(self, parsed):
        """Devuelve {'action': 'deliver'|'forward'|'drop', 'next_hop': str|}."""
        ctrl = parsed["control"]
        f = parsed["flags"]

        # ¿Es para mí (nuestro nodo)?
        if f["GLOBAL"]:
            dst_asn = parsed["asn_dst"]
            if dst_asn == MY_NODE["asn"]:
                # Ya en mi ASN → bajar a OSPF (torre)
                return self._route_ospf(parsed)
            next_hop = BGP_TABLE.get(dst_asn)
            if not next_hop:
                return {"action": "drop", "reason": f"ASN {dst_asn:#06x} desconocido"}
            return {"action": "forward", "next_hop": next_hop}

        if f["INTRA"]:
            return self._route_ospf(parsed)

        # DST_ONLY → es local (torre): entregar al cliente por su Short ID (tabla ARP)
        if f["DST_ONLY"]:
            short = parsed.get("dst_short")
            cli = _client_by_short(short) if short is not None else None
            if cli:
                return {"action": "forward", "next_hop": cli["iface"],
                        "peer": cli["peer"],
                        "reason": f"cliente ARP por Short ID 0x{short:04X}"}
            return {"action": "deliver", "why": "DST_ONLY local (broadcast o sin peer)"}

        # Cabecera local estándar: uuid dst
        dst_uuid = parsed["uuid_dst"]
        if dst_uuid:
            d = int.from_bytes(dst_uuid, "big")
            if d == MY_NODE["uuid"] or parsed["dst_short"] == MY_NODE["short"]:
                return {"action": "deliver", "why": "uuid/short es nuestro"}
            # ¿Es un cliente de Nuestra tabla ARP local?
            cli = _client_by_uuid(d) or _client_by_short(parsed["dst_short"])
            if cli:
                # cli["iface"] es radio-local → entregar a ese peer por WiFi
                return {"action": "forward", "next_hop": cli["iface"],
                        "peer": cli["peer"],
                        "reason": f"cliente ARP local uuid {d:#010x}"}
            return {"action": "forward", "next_hop": "radio-local",
                    "reason": f"uuid {d:#010x} no en ARP local (re-difundir radio)"}

        return {"action": "drop", "reason": "no destino identificable"}

    def _tx(self, peer, data: bytes):
        """Reenvía la trama cruda a un peer sin abrir el payload (router)."""
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as o:
                o.settimeout(3)
                o.connect(peer)
                o.sendall(data)
                o.shutdown(socket.SHUT_WR)
                resp = o.recv(2048)
                if resp:
                    print(f"  [fwd] ← respuesta {len(resp)}B de {peer}")
                    return resp
        except Exception as e:
            print(f"  [fwd] ✗ error hacia {peer}: {e}")
        return None

    def _route_ospf(self, parsed):
        tower = parsed.get("tower_dst")
        if tower == MY_NODE["tower"]:
            return {"action": "deliver", "why": "torre local"}
        hop = OSPF_TABLE.get(tower)
        if not hop:
            return {"action": "drop", "reason": f"torre {tower:#06x} desconocida"}
        # Aunque se rutee por OSPF, el enlace de salida es un TRUNK WiFi/Mbps
        iface = INTERFACES.get(hop, {})
        return {"action": "forward", "next_hop": hop, "peer": iface.get("peer"),
                "trunk": iface.get("type") == "wifi"}

    # ─────────────────────────────────────────────
    # MANEJO DE CONEXIÓN
    # ─────────────────────────────────────────────
    def handle_conn(self, conn, addr):
        print(f"[{addr}] ← trama recibida")
        try:
            data = conn.recv(2048)
            if not data:
                return
            resp = self.forward(data)
            if resp:
                conn.sendall(resp)
        except Exception as e:
            print(f"  error: {e}")
        finally:
            conn.close()

    def forward(self, data: bytes):
        parsed = self.parse_header(data)
        if parsed is None:
            print("  [rte] ⚠ trampa inválida (cabecera corta)")
            return None
        ctrl = parsed["control"]
        svc = parsed["service"]
        action = self.route_or_forward(parsed)
        print(f"  [rte] service=0x{svc:02X} flags={parsed['flags']}")
        print(f"  [rte] decisión: {action}")
        if action["action"] == "deliver":
            # Servicio al que va (TLVGL o futuro)
            if svc == MESH_SVC_TLVGL_REQUEST:
                print("  [svc] TLVGL_REQUEST → derivar al server/proxy (futuro)")
                return None   # aquí el gateway entregaría la petición al server
            print(f"  [svc] servicio 0x{svc:02X} → aún sin handler")
            return None
        if action["action"] == "forward":
            hop = action.get("next_hop")
            peer = action.get("peer")
            iface = INTERFACES.get(hop, {})
            kind = iface.get("type", "radio")
            if peer:
                print(f"  [fwd] interfaz={hop} ({kind}, {iface.get('speed_hint')}) → {peer}")
                self._tx(peer, data)
            else:
                print(f"  [fwd] interfaz={hop} ({kind}) sin peer configurado (noop)")
            return None
        print(f"  [rte] DROP: {action.get('reason','')}")
        return None

    def run(self):
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            s.bind((HOST, PORT))
            s.listen(5)
            print(f"=== GATEWAY-ROUTER escuchando en {HOST}:{PORT} ===")
            print(f"  nodo: ASN={MY_NODE['asn']:#06x} zona={MY_NODE['zone']:#06x} "
                  f"torre={MY_NODE['tower']:#06x} uuid={MY_NODE['uuid']:#010x}")
            while True:
                conn, addr = s.accept()
                threading.Thread(target=self.handle_conn, args=(conn, addr), daemon=True).start()


if __name__ == "__main__":
    GatewayRouter().run()