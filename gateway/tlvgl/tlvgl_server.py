import argparse
import asyncio
from collections import OrderedDict
from pathlib import Path
import struct
import sys

# Ensure package directory is in sys.path
pkg_dir = Path(__file__).parent
if str(pkg_dir) not in sys.path:
    sys.path.insert(0, str(pkg_dir))

try:
    from tlvgl_compiler import TLVGLCompiler
except ImportError:
    from .tlvgl_compiler import TLVGLCompiler

try:
    import mesh_proto as MESH
except ImportError:
    from . import mesh_proto as MESH

MAX_W = 480
MAX_H = 640
DEFAULT_PORT = 8765
CONTENT_DIR = Path(__file__).parent / 'content'


class TLVGLServer:
    def __init__(self, content_dir: Path = CONTENT_DIR, max_w: int = MAX_W, max_h: int = MAX_H, cache_limit: int = 32):
        self.content_dir = Path(content_dir)
        self.max_w = max_w
        self.max_h = max_h
        self.cache_limit = cache_limit
        self.cache = OrderedDict()
        self.compiler = TLVGLCompiler()

    async def handle_client(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter):
        try:
            first_bytes = await reader.read(512)
            if not first_bytes:
                return

            # Modo binario: primer byte es un Control byte MeshHeader válido
            if self._looks_binary(first_bytes):
                await self._handle_mesh_binary(reader, writer, first_bytes)
                return

            # Modo legado ASCII: GET /archivo W= H=
            line = first_bytes.decode('utf-8', errors='replace').strip()
            parts = line.split()
            if not parts or parts[0].upper() != 'GET':
                await self._send_response(writer, None, "", self.max_w, self.max_h)
                return

            raw_filename = parts[1] if len(parts) > 1 else '/index.html'
            clean_filename = raw_filename.lstrip('/')
            if not clean_filename:
                clean_filename = 'index.html'

            parsed_w = None
            parsed_h = None
            for part in parts[2:]:
                if '=' in part:
                    k, v = part.split('=', 1)
                    k_upper = k.upper()
                    if k_upper == 'W':
                        try:
                            parsed_w = int(v)
                        except ValueError:
                            pass
                    elif k_upper == 'H':
                        try:
                            parsed_h = int(v)
                        except ValueError:
                            pass

            w = self.max_w if parsed_w is None else min(parsed_w, self.max_w)
            h = self.max_h if parsed_h is None else min(parsed_h, self.max_h)

            tlv_bytes = self._compile_or_cache(clean_filename, w, h)

            await self._send_response(writer, tlv_bytes, clean_filename, w, h)
        except Exception:
            pass
        finally:
            writer.close()
            try:
                await writer.wait_closed()
            except Exception:
                pass

    @staticmethod
    def _looks_binary(data: bytes) -> bool:
        """Una trama binaria MeshHeader tiene el primer byte como control byte
        válido. El modo ASCII siempre empieza por 'GET '."""
        if not data:
            return False
        if data[0:4] in (b'GET ', b'POST'):
            return False
        ctrl = data[0]
        if ctrl == 0x00:
            return False
        if len(data) < 3:
            return False
        return True

    def _compile_or_cache(self, clean_filename: str, w: int, h: int):
        """Compila el archivo HTML a TLV con caché LRU. Devuelve bytes o None."""
        cache_key = (clean_filename, w, h)
        if cache_key in self.cache:
            tlv_bytes = self.cache[cache_key]
            self.cache.move_to_end(cache_key)
            return tlv_bytes

        target_path = (self.content_dir / clean_filename).resolve()
        base_dir = self.content_dir.resolve()
        is_safe = False
        try:
            target_path.relative_to(base_dir)
            is_safe = True
        except ValueError:
            is_safe = False

        tlv_bytes = None
        if is_safe and target_path.is_file():
            with open(target_path, 'r', encoding='utf-8') as f:
                html_content = f.read()
            tlv_bytes = self.compiler.compile(html_content, w, h)
            self.cache[cache_key] = tlv_bytes
            if len(self.cache) > self.cache_limit:
                self.cache.popitem(last=False)
        return tlv_bytes

    async def _handle_mesh_binary(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter, first_bytes: bytes):
        """Procesa una trama binaria MeshHeader + TLV del firmware ESP32."""
        peer = writer.get_extra_info('peername')
        uuid_str = self._format_peer_uuid(peer)

        hdr, hdr_len = MESH.parse_mesh_header(first_bytes)
        if hdr is None:
            print(f"[UUID={uuid_str}] ⚠ trama MeshHeader inválida ({len(first_bytes)}B): "
                  f"{first_bytes[:16].hex()}")
            return

        tlv_data = first_bytes[hdr_len:]
        tag, value = MESH.parse_uplink_tlv(tlv_data)

        requested_url = "http://home.mesh"
        if tag == MESH.TYPE_REQ_URL:
            requested_url = value.decode('utf-8', errors='ignore')
            print(f"[UUID={uuid_str}] REQ_URL: '{requested_url}'")
        elif tag == MESH.TYPE_REQ_LINK_CLICK:
            link_id = value[0] if len(value) > 0 else 0
            print(f"[UUID={uuid_str}] LINK_CLICK id={link_id}")
            requested_url = f"http://link-{link_id}.mesh"
        elif tag == MESH.TYPE_REQ_INPUT_SUBMIT:
            elem_id = value[0] if len(value) > 0 else 0
            txt = value[1:].decode('utf-8', errors='ignore')
            print(f"[UUID={uuid_str}] INPUT_SUBMIT elem={elem_id} '{txt}'")
            requested_url = txt if txt else requested_url

        # Resolver la URL interna .mesh a un archivo HTML del content_dir
        clean_filename = self._resolve_mesh_url(requested_url)

        tlv_bytes = self._compile_or_cache(clean_filename, self.max_w, self.max_h)
        if tlv_bytes is None:
            # Fallback: index.html (página por defecto)
            tlv_bytes = self._compile_or_cache('index.html', self.max_w, self.max_h)
        if tlv_bytes is None:
            # Página vacía mínima: nunca dejar colgado al cliente
            page_tlv = b"PH" + MESH.build_tlv_node(MESH.TYPE_ABS_PAGE, b"") + bytes([MESH.TYPE_END])
            response = MESH.build_response_header(MESH.MESH_CTRL_DST_ONLY | MESH.MESH_SVC_TLVGL_RESPONSE)
            response += page_tlv
            writer.write(response)
            await writer.drain()
            print(f"[UUID={uuid_str}] → {len(response)}B página vacía (sin contenido) para '{requested_url}'")
            return

        # Construir respuesta MeshHeader + TLV (igual que gateway_prototype.py)
        page_tlv = b"PH" + tlv_bytes
        response = MESH.build_response_header(MESH.MESH_CTRL_DST_ONLY | MESH.MESH_SVC_TLVGL_RESPONSE)
        response += page_tlv
        writer.write(response)
        await writer.drain()
        print(f"[UUID={uuid_str}] → {len(response)}B respuesta TLVGL para '{requested_url}'")

    def _resolve_mesh_url(self, url: str) -> str:
        """Convierte una URL .mesh a un archivo del content_dir.

        Reglas:
          http://x.mesh        → x.html (o index.html si x == 'home')
          http://x.mesh/pag    → x/pag.html
          path absoluto        → se usa tal cual (buscando .html)
        """
        url = url.strip()
        if url.startswith('http://'):
            url = url[7:]
        if url.startswith('https://'):
            url = url[8:]
        # Quitar sufijos .mesh / .tlvgl / query / fragmento
        for suffix in ('.mesh', '.tlvgl', '.html'):
            if url.endswith(suffix):
                url = url[: -len(suffix)]
                break
        url = url.split('#')[0].split('?')[0]
        url = url.rstrip('/')
        if not url or url == 'home':
            return 'index.html'
        return url + '.html'

    async def _send_response(self, writer: asyncio.StreamWriter, tlv_bytes: bytes | None, filename: str, w: int, h: int):
        if tlv_bytes is None:
            size = 0
            payload = b''
        else:
            size = len(tlv_bytes)
            payload = tlv_bytes

        peer = writer.get_extra_info('peername')
        uuid_str = self._format_peer_uuid(peer)
        print(f"[UUID={uuid_str}] GET /{filename} {w}x{h} → {size} bytes")

        header = struct.pack('>I', size)
        writer.write(header + payload)
        await writer.drain()

    @staticmethod
    def _format_peer_uuid(peer) -> str:
        if peer and isinstance(peer, tuple):
            ip = peer[0]
            try:
                import socket
                ip_bytes = socket.inet_aton(ip)
                return ip_bytes.hex().upper()
            except Exception:
                return str(ip)
        return "00000000"


_default_server = None


def get_default_server() -> TLVGLServer:
    global _default_server
    if _default_server is None:
        _default_server = TLVGLServer()
    return _default_server


async def handle_client(reader: asyncio.StreamReader, writer: asyncio.StreamWriter):
    await get_default_server().handle_client(reader, writer)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Servidor TCP TLVGL")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help=f"Puerto TCP (default: {DEFAULT_PORT})")
    parser.add_argument("--content-dir", type=str, default=str(CONTENT_DIR), help="Directorio de contenido HTML")
    parser.add_argument("--max-w", type=int, default=MAX_W, help=f"Ancho máximo (default: {MAX_W})")
    parser.add_argument("--max-h", type=int, default=MAX_H, help=f"Alto máximo (default: {MAX_H})")

    args = parser.parse_args()

    server = TLVGLServer(
        content_dir=Path(args.content_dir),
        max_w=args.max_w,
        max_h=args.max_h
    )

    async def main():
        srv = await asyncio.start_server(server.handle_client, '0.0.0.0', args.port)
        print(f"Servidor TLVGL corriendo en 0.0.0.0:{args.port} (content_dir={args.content_dir})")
        async with srv:
            await srv.serve_forever()

    asyncio.run(main())
