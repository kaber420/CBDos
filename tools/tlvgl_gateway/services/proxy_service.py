#!/usr/bin/env python3
"""
Servicio de Proxy Web y Transcodificación de Internet (ServiceId: 0x05) para CBDos.
Descarga contenido externo (APIs, noticias, clima), valida permisos ACL en SQLite
y compila el contenido a Bytecode TLVGL para los clientes de la malla.
"""

import urllib.request
import json
from typing import Optional, Dict, Any, Tuple

from services.base_service import BaseService, MeshContext
from tlvgl_compiler import TLVGLCompiler, MAX_W, MAX_H
import mesh_proto as MESH


class ProxyService(BaseService):
    def __init__(self, max_w: int = MAX_W, max_h: int = MAX_H):
        super().__init__(name="WebProxy", service_id=MESH.MESH_SVC_PROXY)
        self.max_w = max_w
        self.max_h = max_h
        self.compiler = TLVGLCompiler()

    def generate_proxy_page(self, title: str, message: str, is_error: bool = False) -> bytes:
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

    def fetch_url(self, url: str) -> bytes:
        if not url.startswith("http://") and not url.startswith("https://"):
            url = "http://" + url
        try:
            req = urllib.request.Request(url, headers={'User-Agent': 'CBDos-Mesh-Proxy/1.0'})
            with urllib.request.urlopen(req, timeout=6.0) as resp:
                content_type = resp.headers.get_content_type()
                raw_data = resp.read()
                if "html" in content_type:
                    html_text = raw_data.decode('utf-8', errors='ignore')
                    return self.compiler.compile(html_text, self.max_w, self.max_h)
                elif "json" in content_type:
                    data = json.loads(raw_data.decode('utf-8', errors='ignore'))
                    pretty_json = json.dumps(data, indent=2)[:300]
                    return self.generate_proxy_page("Respuesta JSON API", pretty_json)
                else:
                    return self.generate_proxy_page("Contenido Externo", f"Recibidos {len(raw_data)} bytes ({content_type})")
        except Exception as e:
            return self.generate_proxy_page("Error Proxy Web", f"No se pudo cargar {url}: {e}", is_error=True)

    def handle_request(self, payload: bytes, client_entry: Optional[Dict[str, Any]], reply_short_id: int, ctx: MeshContext) -> Optional[Tuple[int, bytes]]:
        if not self.enabled:
            tlv = self.generate_proxy_page("Servicio Deshabilitado", "El servicio de Proxy a Internet no esta activo en esta Torre.", is_error=True)
            return (MESH.MESH_SVC_TLVGL_RESPONSE, b"PH" + tlv)

        # Validar permiso en la tabla Pseudo-ARP (SQLite3)
        if client_entry and not client_entry.get("proxy_acl", False):
            if ctx.debug:
                print(f"⛔ [Proxy ACL] Bloqueado acceso a Internet para IP={client_entry['ipv4']} (Sin Permiso)")
            tlv = self.generate_proxy_page("Acceso Denegado", "Tu nodo no tiene permiso de salida a Internet en esta Torre.", is_error=True)
            return (MESH.MESH_SVC_TLVGL_RESPONSE, b"PH" + tlv)

        url = payload.decode('utf-8', errors='ignore').strip()
        if ctx.debug:
            print(f"🌍 [Proxy Web] Descargando y transcodificando: '{url}'")

        tlv_bytes = self.fetch_url(url)
        return (MESH.MESH_SVC_TLVGL_RESPONSE, b"PH" + tlv_bytes)
