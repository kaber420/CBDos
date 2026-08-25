#!/usr/bin/env python3
"""
Servidor Gateway Asíncrono TLVGL / Mesh para CBDos.
Escucha peticiones por TCP (WiFi) y por Serial (Dongle USB ESP-NOW) simultáneamente,
compilando HTML a TLVGL super denso en tiempo real.
"""

import asyncio
import argparse
import struct
import re
from pathlib import Path
from collections import OrderedDict

from tlvgl_compiler import TLVGLCompiler, MAX_W, MAX_H
from serial_transport import SerialEspNowTransport
import mesh_proto as MESH

DEFAULT_PORT = 8080
CONTENT_DIR = Path(__file__).parent / "content"


class TLVGLServer:
    def __init__(self, content_dir: Path = CONTENT_DIR, max_w: int = MAX_W, max_h: int = MAX_H):
        self.content_dir = Path(content_dir)
        self.content_dir.mkdir(parents=True, exist_ok=True)
        self.max_w = max_w
        self.max_h = max_h
        self.compiler = TLVGLCompiler()
        self.cache = OrderedDict()
        self.cache_limit = 64

    def _resolve_mesh_url(self, url: str) -> str:
        url = url.strip()
        url = re.sub(r'^https?:/+', '', url, flags=re.IGNORECASE)

        for suffix in ('.mesh', '.tlvgl', '.html'):
            if url.endswith(suffix):
                url = url[: -len(suffix)]
                break
        url = url.split('#')[0].split('?')[0].rstrip('/')
        if not url or url == 'home' or url == 'index':
            return 'index.html'
        return url + '.html'

    def _compile_or_cache(self, filename: str, w: int, h: int) -> bytes | None:
        target_path = (self.content_dir / filename).resolve()
        if not target_path.is_file():
            return None

        mtime = target_path.stat().st_mtime
        cache_key = (filename, w, h, mtime)
        if cache_key in self.cache:
            return self.cache[cache_key]

        try:
            with open(target_path, 'r', encoding='utf-8') as f:
                html_content = f.read()
            tlv_bytes = self.compiler.compile(html_content, w, h)
            self.cache[cache_key] = tlv_bytes
            if len(self.cache) > self.cache_limit:
                self.cache.popitem(last=False)
            return tlv_bytes
        except Exception as e:
            print(f"❌ Error compilando {filename}: {e}")
            return None

    def process_mesh_packet(self, data: bytes) -> bytes:
        """Procesa una trama binaria MeshHeader + TLV y devuelve el paquete binario de respuesta."""
        if len(data) < 3:
            return b""

        hdr, hdr_len = MESH.parse_mesh_header(data)
        if not hdr:
            return b""

        tlv_data = data[hdr_len:]
        tag, value = MESH.parse_uplink_tlv(tlv_data)

        req_url = "home.mesh"
        if tag == MESH.TYPE_REQ_URL:
            req_url = value.decode('utf-8', errors='ignore')
            print(f"🌐 [Mesh] REQ_URL: '{req_url}'")
        elif tag == MESH.TYPE_REQ_LINK_CLICK:
            link_id = value[0] if len(value) > 0 else 0
            req_url = self.compiler.last_link_map.get(link_id, f"link_{link_id}.mesh")
            print(f"🖱️ [Mesh] LINK_CLICK #{link_id} → '{req_url}'")
        elif tag == MESH.TYPE_REQ_INPUT_SUBMIT:
            elem_id = value[0] if len(value) > 0 else 0
            txt = value[1:].decode('utf-8', errors='ignore')
            print(f"⌨️ [Mesh] INPUT_SUBMIT #{elem_id}: '{txt}'")
        elif tag == MESH.TYPE_REQ_CONTROL_EVT:
            elem_id = value[0] if len(value) > 0 else 0
            val = struct.unpack(">h", value[1:3])[0] if len(value) >= 3 else 0
            print(f"🎛️ [Mesh] CONTROL_EVT #{elem_id}: {val}")

        clean_file = self._resolve_mesh_url(req_url)
        tlv_bytes = self._compile_or_cache(clean_file, self.max_w, self.max_h)
        if tlv_bytes is None:
            tlv_bytes = self._compile_or_cache("index.html", self.max_w, self.max_h)

        if tlv_bytes is None:
            tlv_bytes = b"PH\x10\x00\x00\xfe"
        else:
            tlv_bytes = b"PH" + tlv_bytes

        resp_hdr = MESH.build_response_header(
            MESH.MESH_CTRL_DST_ONLY | MESH.MESH_SVC_TLVGL_RESPONSE,
            hdr['dst_id'] if hdr['dst_id'] != 0 else 0x0001
        )
        return resp_hdr + tlv_bytes

    async def handle_client(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter):
        peer = writer.get_extra_info('peername')
        peer_str = f"{peer[0]}:{peer[1]}" if peer else "desconocido"

        try:
            data = await asyncio.wait_for(reader.read(4096), timeout=10.0)
            if not data:
                return

            # 1. ¿Trama MeshHeader binaria?
            if len(data) >= 3 and (data[0] & 0x07) in (MESH.MESH_SVC_TLVGL_REQUEST, MESH.MESH_SVC_PROXY):
                resp = self.process_mesh_packet(data)
                if resp:
                    writer.write(resp)
                    await writer.drain()
                    print(f"[{peer_str}] 📤 Enviados {len(resp)} bytes por TCP")
                return

            # 2. Petición HTTP estándar
            first_line = data.split(b"\r\n")[0].decode('utf-8', errors='ignore')
            m = re.match(r'GET\s+/([^\s?#]*)', first_line)
            if m:
                path = m.group(1)
                clean_file = self._resolve_mesh_url(path)
                tlv_bytes = self._compile_or_cache(clean_file, self.max_w, self.max_h)
                if tlv_bytes:
                    payload = b"PH" + tlv_bytes
                    http_resp = (
                        f"HTTP/1.1 200 OK\r\n"
                        f"Content-Type: application/octet-stream\r\n"
                        f"Content-Length: {len(payload)}\r\n\r\n"
                    ).encode('utf-8') + payload
                    writer.write(http_resp)
                    await writer.drain()
                    print(f"[{peer_str}] 📤 HTTP GET /{clean_file} → {len(payload)} bytes")
                    return

        except Exception as e:
            print(f"⚠️ Error atendiendo cliente [{peer_str}]: {e}")
        finally:
            try:
                writer.close()
                await writer.wait_closed()
            except Exception:
                pass


def on_espnow_packet_received(data: bytes, transport: SerialEspNowTransport, server: TLVGLServer):
    """Callback cuando el Dongle USB captura un paquete del aire vía ESP-NOW."""
    if len(data) < 2:
        return

    # Extraer micro-chunk header (2B)
    chunk_info = data[0]
    msg_id = data[1]
    chunk_idx = (chunk_info >> 4) & 0x0F
    total_chunks = chunk_info & 0x0F
    payload = data[2:] if (total_chunks > 0 and chunk_idx < total_chunks) else data

    print(f"📻 [Dongle ESP-NOW] Trama recibida (chunk {chunk_idx+1}/{total_chunks}, {len(data)}B)")
    resp = server.process_mesh_packet(payload)
    if resp:
        print(f"📻 [Dongle ESP-NOW] Emitiendo respuesta ({len(resp)}B) por radio...")
        transport.send_packet(resp, msg_id=msg_id)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Servidor Gateway TLVGL para CBDos")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help=f"Puerto TCP (default: {DEFAULT_PORT})")
    parser.add_argument("--serial", type=str, default="", help="Puerto Serial del Dongle USB ESP-NOW (ej: /dev/ttyACM1)")
    parser.add_argument("--content-dir", type=str, default=str(CONTENT_DIR), help="Directorio de contenido HTML")
    args = parser.parse_args()

    server = TLVGLServer(content_dir=Path(args.content_dir))
    serial_transport = None

    if args.serial:
        serial_transport = SerialEspNowTransport(
            port=args.serial,
            on_packet_cb=lambda data, tr: on_espnow_packet_received(data, tr, server)
        )
        serial_transport.start()

    async def main():
        srv = await asyncio.start_server(server.handle_client, '0.0.0.0', args.port)
        print(f"🚀 Servidor TLVGL Gateway activo en TCP puerto {args.port}")
        if args.serial:
            print(f"📻 Puente Dongle USB ESP-NOW activo en {args.serial}")
        print(f"📁 Directorio de contenido: {args.content_dir}")
        async with srv:
            await srv.serve_forever()

    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nServidor detenido.")
        if serial_transport:
            serial_transport.stop()
