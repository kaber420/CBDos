#!/usr/bin/env python3
"""
Motor Gateway-Router Frontal y Service Hub para CBDos.
Orquesta el enrutamiento por Capa 3 (Pseudo-ARP SQLite3 con DAD) y despacha
peticiones a los servicios modulares independientes (Routing, Hosting, Proxy).
"""

import sys
from pathlib import Path
from typing import Optional, Dict, Any, Tuple

import mesh_proto as MESH
from pseudo_arp import PseudoArpTable
from services.base_service import MeshContext
from services.routing_service import RoutingService
from services.hosting_service import HostingService
from services.proxy_service import ProxyService


class GatewayRouter:
    def __init__(self, content_dir: Path, db_path: str = "data/router.db", debug: bool = False, services_list: Optional[list] = None):
        self.content_dir = Path(content_dir)
        self.debug = debug
        self.arp_table = PseudoArpTable(db_path=db_path)
        self.ctx = MeshContext(arp_table=self.arp_table, debug=self.debug)

        # 1. Instanciar Servicios Modulares Desacoplados
        self.routing_svc = RoutingService(tower_id=0x0001, tower_name="Gateway CBDos Mesh", channel=1)
        self.hosting_svc = HostingService(content_dir=self.content_dir)
        self.proxy_svc = ProxyService()

        self.services = {
            0x0F: self.routing_svc,
            MESH.MESH_SVC_TLVGL_REQUEST: self.hosting_svc,
            MESH.MESH_SVC_PROXY: self.proxy_svc
        }

        # 2. Inicializar y enlazar servicios al Bus de Eventos
        for svc in self.services.values():
            svc.init_service(self.ctx)

        # 3. Configurar servicios activos según el perfil
        if services_list is not None:
            self.configure_services(services_list)

    def get_pop_broadcast_packet(self) -> bytes:
        """Genera la trama completa de Micro-Broadcast (Header 3B + Payload 7B) para emisión."""
        payload_7b = self.routing_svc.generate_pop_broadcast()
        # Header ultra-corto (3B): Control = 0x4F (DST_ONLY | RoutingControl), DstID = 0xFFFF (Broadcast)
        header = MESH.build_response_header(MESH.MESH_CTRL_DST_ONLY | 0x0F, 0xFFFF)
        return header + payload_7b

    def configure_services(self, active_services: list):
        """Habilita o deshabilita servicios por nombre ('routing', 'hosting', 'proxy')."""
        active_set = set(s.strip().lower() for s in active_services)
        
        self.routing_svc.enabled = "routing" in active_set or "all" in active_set
        self.hosting_svc.enabled = "hosting" in active_set or "all" in active_set
        self.proxy_svc.enabled = "proxy" in active_set or "all" in active_set

        if self.debug:
            print(f"🧩 [GatewayRouter] Servicios activos: Routing={self.routing_svc.enabled}, Hosting={self.hosting_svc.enabled}, Proxy={self.proxy_svc.enabled}")

    def log_telemetry(self, client_entry: Optional[dict], short_id: int, req_url: str, resp_len: int, service_name: str = "TLVGL"):
        """Muestra una tarjeta ASCII formateada de telemetría si el modo debug está activo."""
        if not self.debug:
            ip_str = client_entry.get("ipv4", f"0x{short_id:04X}") if client_entry else f"0x{short_id:04X}"
            print(f"🌐 [{service_name}] [{ip_str}] '{req_url}' → {resp_len}B")
            return

        ip_str = client_entry.get("ipv4", "10.x.y.z") if client_entry else f"0x{short_id:04X}"
        mac_str = client_entry.get("mac", "Radio-Local") if client_entry else "N/A"
        proxy_str = "🟢 AUTORIZADO" if (client_entry and client_entry.get("proxy_acl", False)) else "🔴 SOLO LOCAL"
        rssi = client_entry.get("rssi", 0) if client_entry else 0
        sig_str = f"{rssi} dBm" if rssi != 0 else "Local"

        print(f"\r\n┌── 📡 [Mesh Telemetría] Servicio: {service_name} ──────────────────────────")
        print(f"│ 🪪 Cliente IP:    {ip_str} (UUID)")
        print(f"│ 🏷️ Short ID:     0x{short_id:04X} | MAC: {mac_str} | Señal: {sig_str}")
        print(f"│ 🌐 Recurso:       '{req_url}'")
        print(f"│ 🛡️ Estado ACL:    {proxy_str}")
        print(f"│ 📤 Bytecode TLV:  {resp_len} Bytes emitidos")
        print(f"└───────────────────────────────────────────────────────────────\r\n")

    def route_and_process(self, data: bytes, src_mac: bytes = b"", rssi: int = 0) -> Optional[bytes]:
        """
        Punto de entrada maestro del Gateway-Router:
        1. Desempaqueta MeshHeader.
        2. Registra o actualiza en Tabla Pseudo-ARP (SQLite3).
        3. Enruta la petición al servicio modular correspondiente.
        4. Retorna la trama empaquetada lista para emisión.
        """
        if len(data) < 3:
            return None

        # 1. Parseo de cabecera
        hdr, hdr_len = MESH.parse_mesh_header(data)
        if not hdr:
            return None

        ctrl = hdr['control']
        service_id = ctrl & 0x07 # Bits 0..2 (0x07=TLVGL, 0x05=Proxy, 0x01=Chat)
        is_signal = bool(ctrl & MESH.MESH_CTRL_SIGNAL_BIT)
        src_id = hdr.get('src_id', 0)
        dst_id = hdr.get('dst_id', 0x0001)

        # 2. Registro / Actualización en Tabla Pseudo-ARP si se conoce la MAC
        client_entry = None
        if src_mac and len(src_mac) == 6:
            client_entry = self.arp_table.register_or_update(
                mac_bytes=src_mac,
                candidate_short_id=src_id if src_id != 0 and src_id != 0xFFFF else None,
                rssi=rssi
            )
            reply_short_id = client_entry["short_id"]
        else:
            reply_short_id = src_id if (src_id != 0 and src_id != 0xFFFF) else (dst_id if dst_id != 0xFFFF else 0x0001)
            client_entry = self.arp_table.get_by_short_id(reply_short_id)

        tlv_payload = data[hdr_len:]

        # 3. Despacho a Servicios:
        # A) Control de Ruteo y Señalización de Red (0x0F / Signal)
        if is_signal:
            res = self.routing_svc.handle_request(tlv_payload, client_entry, reply_short_id, self.ctx)
            if res:
                resp_svc, resp_data = res
                resp_hdr = MESH.build_response_header(0x4F, reply_short_id)
                return resp_hdr + resp_data

        # B) Petición TLVGL (0x07)
        if service_id in (MESH.MESH_SVC_TLVGL_REQUEST, 0x07):
            tag, value = MESH.parse_uplink_tlv(tlv_payload)
            req_url = value.decode('utf-8', errors='ignore') if (tag == MESH.TYPE_REQ_URL and value) else "index.mesh"

            # ¿Es una URL externa que requiere el Proxy?
            is_external = req_url.startswith("http://") or req_url.startswith("https://") or ('.' in req_url and not req_url.endswith('.mesh'))

            if is_external:
                res = self.proxy_svc.handle_request(req_url.encode('utf-8'), client_entry, reply_short_id, self.ctx)
            else:
                res = self.hosting_svc.handle_request(tlv_payload, client_entry, reply_short_id, self.ctx)

            if res:
                resp_svc, resp_data = res
                resp_hdr = MESH.build_response_header(MESH.MESH_CTRL_DST_ONLY | resp_svc, reply_short_id)

                self.log_telemetry(
                    client_entry=client_entry,
                    short_id=reply_short_id,
                    req_url=req_url,
                    resp_len=len(resp_data),
                    service_name="TLVGL Proxy" if is_external else "TLVGL Local"
                )

                return resp_hdr + resp_data

        # C) Petición Proxy Directo (0x05)
        if service_id == MESH.MESH_SVC_PROXY:
            res = self.proxy_svc.handle_request(tlv_payload, client_entry, reply_short_id, self.ctx)
            if res:
                resp_svc, resp_data = res
                resp_hdr = MESH.build_response_header(MESH.MESH_CTRL_DST_ONLY | resp_svc, reply_short_id)

                self.log_telemetry(
                    client_entry=client_entry,
                    short_id=reply_short_id,
                    req_url=tlv_payload.decode('utf-8', errors='ignore'),
                    resp_len=len(resp_data),
                    service_name="Direct Proxy"
                )

                return resp_hdr + resp_data

        return None
