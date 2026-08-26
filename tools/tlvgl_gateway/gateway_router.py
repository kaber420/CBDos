#!/usr/bin/env python3
"""
Motor Gateway-Router Frontal para CBDos.
Integra Enrutamiento por Service ID, Tabla Pseudo-ARP (10.x.y.z IPv4 Mesh + DAD)
y Control de Acceso Proxy para clientes ESP32 sobre ESP-NOW y TCP.
"""

import sys
import struct
import time
import urllib.request
import json
from pathlib import Path
from typing import Optional, Tuple

import mesh_proto as MESH
from pseudo_arp import PseudoArpTable
from tlvgl_compiler import TLVGLCompiler, MAX_W, MAX_H


class GatewayRouter:
    def __init__(self, content_dir: Path, arp_storage: str = "clients.db", max_w: int = MAX_W, max_h: int = MAX_H, debug: bool = False):
        self.content_dir = Path(content_dir)
        self.max_w = max_w
        self.max_h = max_h
        self.debug = debug
        self.compiler = TLVGLCompiler()
        self.arp_table = PseudoArpTable(db_path=arp_storage)
        self.cache = {}

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

    def _resolve_mesh_url(self, url: str) -> str:
        url = url.strip()
        for suffix in ('.mesh', '.tlvgl', '.html'):
            if url.endswith(suffix):
                url = url[: -len(suffix)]
                break
        url = url.split('#')[0].split('?')[0].rstrip('/')
        if not url or url in ('home', 'index'):
            return 'index.html'
        return url + '.html'

    def _compile_or_cache(self, filename: str) -> Optional[bytes]:
        target_path = (self.content_dir / filename).resolve()
        if not target_path.is_file():
            return None

        mtime = target_path.stat().st_mtime
        cache_key = (filename, self.max_w, self.max_h, mtime)
        if cache_key in self.cache:
            return self.cache[cache_key]

        try:
            with open(target_path, 'r', encoding='utf-8') as f:
                html = f.read()
            tlv = self.compiler.compile(html, self.max_w, self.max_h)
            self.cache[cache_key] = tlv
            return tlv
        except Exception as e:
            print(f"❌ Error compilando {filename}: {e}")
            return None

    def _generate_proxy_page(self, title: str, message: str, is_error: bool = False) -> bytes:
        """Genera una página TLVGL sintética para respuestas de Proxy o Errores ACL."""
        bg_color = "#181c26" if not is_error else "#2A1515"
        txt_color = "#E2E8F0" if not is_error else "#FF6B6B"
        html = f"""<!DOCTYPE html>
<html>
<head><title>{title}</title></head>
<body>
  <panel style="left: 12px; top: 15px; width: 456px; height: 100px; background-color: {bg_color};">
    <h1>{title}</h1>
    <p style="color: {txt_color};">{message}</p>
  </panel>
  <a href="/index.html" style="left: 12px; top: 130px; width: 456px;">⬅️ Volver al Inicio</a>
</body>
</html>"""
        return self.compiler.compile(html, self.max_w, self.max_h)

    def _fetch_proxy_url(self, url: str) -> bytes:
        """Descarga una URL externa vía Proxy y la transcodifica a TLVGL."""
        if not url.startswith("http://") and not url.startswith("https://"):
            url = "http://" + url
        try:
            req = urllib.request.Request(url, headers={'User-Agent': 'CBDos-Mesh-Gateway/1.0'})
            with urllib.request.urlopen(req, timeout=5.0) as resp:
                content_type = resp.headers.get_content_type()
                raw_data = resp.read()
                if "html" in content_type:
                    html_text = raw_data.decode('utf-8', errors='ignore')
                    return self.compiler.compile(html_text, self.max_w, self.max_h)
                elif "json" in content_type:
                    data = json.loads(raw_data.decode('utf-8', errors='ignore'))
                    pretty_json = json.dumps(data, indent=2)[:300]
                    return self._generate_proxy_page("Respuesta JSON API", pretty_json)
                else:
                    return self._generate_proxy_page("Contenido Externo", f"Recibidos {len(raw_data)} bytes ({content_type})")
        except Exception as e:
            return self._generate_proxy_page("Error Proxy Web", f"No se pudo cargar {url}: {e}", is_error=True)

    def route_and_process(self, data: bytes, src_mac: bytes = b"", rssi: int = 0) -> Optional[bytes]:
        """
        Punto de entrada principal del Gateway-Router:
        1. Desempaqueta MeshHeader.
        2. Registra o actualiza en Tabla Pseudo-ARP (IPv4 Mesh + DAD).
        3. Enruta por Service ID (Local .mesh, Proxy Web o Control de Red).
        4. Retorna la trama empaquetada lista para emisión en radio o TCP.
        """
        if len(data) < 3:
            return None

        # 1. Parseo de cabecera
        hdr, hdr_len = MESH.parse_mesh_header(data)
        if not hdr:
            return None

        ctrl = hdr['control']
        service_id = ctrl & 0x07 # Bits 0..2 son el Service ID (0x07 = TLVGL REQ, 0x05 = Proxy, 0x01 = Chat)
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
            # El destino de respuesta es el Short ID asignado en la tabla
            reply_short_id = client_entry["short_id"]
        else:
            # Fallback al Short ID que vino en la cabecera
            reply_short_id = src_id if (src_id != 0 and src_id != 0xFFFF) else (dst_id if dst_id != 0xFFFF else 0x0001)
            client_entry = self.arp_table.get_by_short_id(reply_short_id)

        tlv_payload = data[hdr_len:]

        # 3. Despacho por Servicio:
        # A) SERVICIO: Ruteo y Señalización de Red (Handshake / Asociación)
        if is_signal and len(tlv_payload) >= 4 and tlv_payload[0] == 0x01: # Tag Probe Request con Payload de sondeo
            assigned_ip = client_entry["ipv4"] if client_entry else "10.0.0.2"
            resp_payload = bytearray([0x02]) # Tag Probe Response
            resp_payload += struct.pack(">H", 0x0001) # Tower ID (0x0001)
            resp_payload += bytes([1, 0x02]) # Canal 1, LR mode
            tower_name = "Gateway CBDos Mesh"
            resp_payload += bytes([len(tower_name)]) + tower_name.encode('utf-8')
            
            resp_hdr = MESH.build_response_header(0x4F, reply_short_id)
            return resp_hdr + bytes(resp_payload)

        # B) SERVICIO: Petición Navegador TLVGL (0x07)
        if service_id == MESH.MESH_SVC_TLVGL_REQUEST or service_id == 0x07:
            tag, value = MESH.parse_uplink_tlv(tlv_payload)
            req_url = "index.mesh"

            if tag == MESH.TYPE_REQ_URL:
                req_url = value.decode('utf-8', errors='ignore')
                print(f"🌐 [Router -> TLVGL] Nodo [{client_entry['ipv4'] if client_entry else f'0x{reply_short_id:04X}'}] pide: '{req_url}'")
            elif tag == MESH.TYPE_REQ_LINK_CLICK:
                link_id = value[0] if len(value) > 0 else 0
                req_url = self.compiler.last_link_map.get(link_id, f"link_{link_id}.mesh")
                print(f"🖱️ [Router -> TLVGL] LINK_CLICK #{link_id} → '{req_url}'")
            elif tag == MESH.TYPE_REQ_INPUT_SUBMIT:
                elem_id = value[0] if len(value) > 0 else 0
                txt = value[1:].decode('utf-8', errors='ignore')
                print(f"⌨️ [Router -> TLVGL] INPUT_SUBMIT #{elem_id}: '{txt}'")
            elif tag == MESH.TYPE_REQ_CONTROL_EVT:
                elem_id = value[0] if len(value) > 0 else 0
                val = struct.unpack(">h", value[1:3])[0] if len(value) >= 3 else 0
                print(f"🎛️ [Router -> TLVGL] CONTROL_EVT #{elem_id}: {val}")

            # ¿Es una URL externa que requiere Proxy Web?
            is_external = req_url.startswith("http://") or req_url.startswith("https://") or ('.' in req_url and not req_url.endswith('.mesh'))

            if is_external:
                # Validar permiso en la Tabla Pseudo-ARP (ACL)
                if client_entry and not client_entry.get("proxy_acl", False):
                    print(f"⛔ [Proxy ACL] Bloqueado acceso a Internet para IP={client_entry['ipv4']} (Sin Permiso)")
                    tlv_bytes = self._generate_proxy_page("Acceso Denegado", "Tu nodo no tiene permiso de salida a Internet en esta Torre.", is_error=True)
                else:
                    print(f"🌍 [Proxy Web] Descargando y transcodificando: {req_url}")
                    tlv_bytes = self._fetch_proxy_url(req_url)
            else:
                # Servicio local desde el directorio de contenido .mesh
                clean_file = self._resolve_mesh_url(req_url)
                tlv_bytes = self._compile_or_cache(clean_file)
                if tlv_bytes is None:
                    tlv_bytes = self._compile_or_cache("index.html")

            if tlv_bytes is None:
                tlv_bytes = b"\x10\x00\x00\xfe" # Fallback página vacía

            resp_payload = b"PH" + tlv_bytes
            resp_hdr = MESH.build_response_header(MESH.MESH_CTRL_DST_ONLY | MESH.MESH_SVC_TLVGL_RESPONSE, reply_short_id)

            self.log_telemetry(
                client_entry=client_entry,
                short_id=reply_short_id,
                req_url=req_url,
                resp_len=len(resp_payload),
                service_name="TLVGL Proxy" if is_external else "TLVGL Local"
            )

            return resp_hdr + resp_payload

        # C) SERVICIO: Proxy Directo (0x05)
        if service_id == MESH.MESH_SVC_PROXY:
            url = tlv_payload.decode('utf-8', errors='ignore')
            if client_entry and not client_entry.get("proxy_acl", False):
                tlv_bytes = self._generate_proxy_page("Acceso Denegado", "Proxy deshabilitado por ACL", is_error=True)
            else:
                tlv_bytes = self._fetch_proxy_url(url)
            
            resp_payload = b"PH" + tlv_bytes
            resp_hdr = MESH.build_response_header(MESH.MESH_CTRL_DST_ONLY | MESH.MESH_SVC_TLVGL_RESPONSE, reply_short_id)

            self.log_telemetry(
                client_entry=client_entry,
                short_id=reply_short_id,
                req_url=url,
                resp_len=len(resp_payload),
                service_name="Direct Proxy"
            )

            return resp_hdr + resp_payload

        return None
