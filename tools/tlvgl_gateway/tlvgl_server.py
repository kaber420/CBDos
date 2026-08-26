#!/usr/bin/env python3
"""
Servidor Gateway Asíncrono TLVGL / Mesh para CBDos.
Integra el motor GatewayRouter con Tabla Pseudo-ARP (10.x.y.z IPv4 Mesh + DAD),
enrutamiento por Service ID y Proxy Web de transcodificación.
"""

import asyncio
import argparse
import struct
import re
import time
import os
from pathlib import Path

from gateway_router import GatewayRouter
from serial_transport import SerialEspNowTransport
import mesh_proto as MESH

DEFAULT_PORT = 8080
CONTENT_DIR = Path(__file__).parent / "content"


class TLVGLServer:
    def __init__(self, content_dir: Path = CONTENT_DIR):
        self.router = GatewayRouter(content_dir=content_dir)

    def process_mesh_packet(self, data: bytes, src_mac: bytes = b"", rssi: int = 0) -> bytes:
        """Delega el procesamiento al motor GatewayRouter."""
        resp = self.router.route_and_process(data, src_mac=src_mac, rssi=rssi)
        return resp if resp else b""

    async def handle_client(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter):
        peer = writer.get_extra_info('peername')
        peer_str = f"{peer[0]}:{peer[1]}" if peer else "desconocido"

        try:
            data = await asyncio.wait_for(reader.read(4096), timeout=10.0)
            if not data:
                return

            # 1. ¿Trama MeshHeader binaria?
            if len(data) >= 3 and (data[0] & 0x0F) in (MESH.MESH_SVC_TLVGL_REQUEST, MESH.MESH_SVC_PROXY, 0x07, 0x0F):
                resp = self.process_mesh_packet(data)
                if resp:
                    writer.write(resp)
                    await writer.drain()
                    print(f"[{peer_str}] 📤 Enviados {len(resp)} bytes por TCP")
                return

            # 2. Petición HTTP estándar (Legacy / Fallback)
            first_line = data.split(b"\r\n")[0].decode('utf-8', errors='ignore')
            m = re.match(r'GET\s+/([^\s?#]*)', first_line)
            if m:
                path = m.group(1)
                clean_file = self.router._resolve_mesh_url(path)
                tlv_bytes = self.router._compile_or_cache(clean_file)
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


def on_espnow_packet_received(data: bytes, transport: SerialEspNowTransport, server: TLVGLServer, src_mac: bytes = b"", rssi: int = 0):
    """Callback cuando el Dongle USB captura un paquete del aire vía ESP-NOW."""
    if len(data) < 2:
        return

    # Extraer micro-chunk header (2B)
    chunk_info = data[0]
    msg_id = data[1]
    chunk_idx = (chunk_info >> 4) & 0x0F
    total_chunks = chunk_info & 0x0F
    payload = data[2:] if (total_chunks > 0 and chunk_idx < total_chunks) else data

    resp = server.process_mesh_packet(payload, src_mac=src_mac, rssi=rssi)
    if resp:
        transport.send_packet(resp, msg_id=msg_id)


import os


def load_config_file(conf_path: Path) -> dict:
    """Carga variables simples clave=valor desde gateway.conf."""
    conf = {}
    if conf_path.is_file():
        try:
            with open(conf_path, "r", encoding="utf-8") as f:
                for line in f:
                    line = line.strip()
                    if line and not line.startswith("#") and "=" in line:
                        k, v = line.split("=", 1)
                        conf[k.strip().upper()] = v.strip()
        except Exception as e:
            print(f"⚠️ Error leyendo {conf_path}: {e}")
    return conf


if __name__ == '__main__':
    conf_file = Path(__file__).parent / "gateway.conf"
    cfg = load_config_file(conf_file)

    default_port = int(cfg.get("PORT", DEFAULT_PORT))
    default_serial = cfg.get("SERIAL_PORT", "")
    default_debug = cfg.get("DEBUG", "false").lower() in ("true", "1", "yes") or os.environ.get("CBDOS_DEBUG", "0") in ("1", "true") or os.environ.get("DEBUG", "0") in ("1", "true")
    
    raw_content = cfg.get("CONTENT_DIR", "content")
    if Path(raw_content).is_absolute():
        default_content = str(Path(raw_content))
    else:
        default_content = str((Path(__file__).parent / raw_content).resolve())

    raw_services = cfg.get("SERVICES", "routing,hosting,proxy")
    services_list = [s.strip() for s in raw_services.split(",") if s.strip()]

    default_radio_mode = cfg.get("RADIO_MODE", "normal")

    parser = argparse.ArgumentParser(description="Servidor Gateway-Router TLVGL para CBDos")
    parser.add_argument("--port", type=int, default=default_port, help=f"Puerto TCP (default: {default_port})")
    parser.add_argument("--serial", type=str, default=default_serial, help="Puerto Serial del Dongle USB ESP-NOW (ej: /dev/ttyACM0)")
    parser.add_argument("--radio-mode", type=str, default=default_radio_mode, choices=["normal", "lr"], help="Modo de radio del Dongle (normal / lr)")
    parser.add_argument("--content-dir", type=str, default=default_content, help="Directorio de contenido HTML")
    parser.add_argument("--services", type=str, default=",".join(services_list), help="Servicios activos separados por coma (routing, hosting, proxy)")
    parser.add_argument("-d", "--debug", action="store_true", default=default_debug, help="Habilita modo desarrollo/debug con telemetría detallada")
    args = parser.parse_args()

    active_services = [s.strip() for s in args.services.split(",") if s.strip()]
    server = TLVGLServer(content_dir=Path(args.content_dir))
    server.router.debug = args.debug
    server.router.configure_services(active_services)

    serial_transport = None

    if args.serial:
        serial_transport = SerialEspNowTransport(
            port=args.serial,
            on_packet_cb=lambda data, tr, mac, rssi: on_espnow_packet_received(data, tr, server, src_mac=mac, rssi=rssi)
        )
        if serial_transport.start():
            # Configurar modo de radio (normal / lr)
            time.sleep(0.1)
            serial_transport.set_radio_mode(args.radio_mode)

    async def broadcast_worker(router: GatewayRouter, transport: Optional[SerialEspNowTransport]):
        """Emite periódicamente el Micro-Broadcast PoP (Hora Epoch + Hash Portada + Status) cada 60s."""
        while True:
            try:
                if transport and transport.running:
                    pkt = router.get_pop_broadcast_packet()
                    transport.send_packet(pkt, msg_id=0xAA)
                    if router.debug:
                        print(f"📡 [PoP Broadcast] Emitido micro-frame (7B) por ESP-NOW: Epoch={int(time.time())} Hash=0x{router.routing_svc.cover_hash:04X} Status=0x{router.routing_svc.status_code:02X}")
            except Exception as e:
                print(f"⚠️ Error en broadcast_worker: {e}")
            await asyncio.sleep(60)

    async def main():
        srv = await asyncio.start_server(server.handle_client, '0.0.0.0', args.port)
        mode_str = "🟢 MODO DESARROLLO (Telemetría Detallada)" if args.debug else "⚪ MODO PRODUCCIÓN (Logs Compactos)"
        print(f"🚀 Gateway-Router TLVGL activo en TCP puerto {args.port} | {mode_str}")
        print(f"🧩 Servicios cargados: {', '.join(active_services)}")
        if args.serial:
            print(f"📻 Puente Dongle USB ESP-NOW activo en {args.serial}")
        print(f"📁 Directorio de contenido: {args.content_dir}")

        # Iniciar emisor periódico de 60 segundos
        asyncio.create_task(broadcast_worker(server.router, serial_transport))

        async with srv:
            await srv.serve_forever()

    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nServidor detenido.")
        if serial_transport:
            serial_transport.stop()

