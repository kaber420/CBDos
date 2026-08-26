#!/usr/bin/env python3
"""
Servicio de Alojamiento y Compilación de Páginas .mesh (ServiceId: 0x07) para CBDos.
Compila HTML dinámico a Bytecode TLVGL ultra-denso y sirve páginas locales.
"""

import re
import struct
from pathlib import Path
from typing import Optional, Dict, Any, Tuple

from services.base_service import BaseService, MeshContext
from tlvgl_compiler import TLVGLCompiler, MAX_W, MAX_H
import mesh_proto as MESH


class HostingService(BaseService):
    def __init__(self, content_dir: Path, max_w: int = MAX_W, max_h: int = MAX_H):
        super().__init__(name="MeshHosting", service_id=MESH.MESH_SVC_TLVGL_REQUEST)
        self.content_dir = Path(content_dir)
        self.max_w = max_w
        self.max_h = max_h
        self.compiler = TLVGLCompiler()
        self.cache: Dict[Tuple[str, int, int, float], bytes] = {}

    def resolve_mesh_url(self, url: str) -> str:
        url = url.strip()
        for suffix in ('.mesh', '.tlvgl', '.html'):
            if url.endswith(suffix):
                url = url[: -len(suffix)]
                break
        url = url.split('#')[0].split('?')[0].rstrip('/')
        if not url or url in ('home', 'index'):
            return 'index.html'
        return url + '.html'

    def compile_or_cache(self, filename: str) -> Optional[bytes]:
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

    def handle_request(self, payload: bytes, client_entry: Optional[Dict[str, Any]], reply_short_id: int, ctx: MeshContext) -> Optional[Tuple[int, bytes]]:
        if not self.enabled:
            return None

        tag, value = MESH.parse_uplink_tlv(payload)
        req_url = "index.mesh"

        if tag == MESH.TYPE_REQ_URL and value:
            req_url = value.decode('utf-8', errors='ignore')
        elif tag == MESH.TYPE_REQ_LINK_CLICK and value:
            link_id = value[0]
            req_url = self.compiler.last_link_map.get(link_id, f"link_{link_id}.mesh")
        elif tag == MESH.TYPE_REQ_INPUT_SUBMIT and value:
            elem_id = value[0]
            txt = value[1:].decode('utf-8', errors='ignore')
            if ctx.debug:
                print(f"⌨️ [Hosting] INPUT_SUBMIT #{elem_id}: '{txt}'")
        elif tag == MESH.TYPE_REQ_CONTROL_EVT and value:
            elem_id = value[0]
            val = struct.unpack(">h", value[1:3])[0] if len(value) >= 3 else 0
            if ctx.debug:
                print(f"🎛️ [Hosting] CONTROL_EVT #{elem_id}: {val}")

        clean_file = self.resolve_mesh_url(req_url)
        tlv_bytes = self.compile_or_cache(clean_file)
        if tlv_bytes is None:
            tlv_bytes = self.compile_or_cache("index.html")

        if tlv_bytes is None:
            tlv_bytes = b"\x10\x00\x00\xfe" # Fallback página vacía

        resp_payload = b"PH" + tlv_bytes
        return (MESH.MESH_SVC_TLVGL_RESPONSE, resp_payload)
