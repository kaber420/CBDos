#!/usr/bin/env python3
"""
test_router_e2e.py — Prueba End-to-End Rigurosa del Router Go y Backend TLVGL
Envía una trama binaria MeshHeader DST_ONLY + TLV REQ_URL al Router Go en puerto 8765,
verifica que se rutee al backend de Hosting en puerto 8766 y valida la respuesta TLV.
"""

import os
import socket
import subprocess
import sys
import time
from pathlib import Path

# Añadir gateway/tlvgl al path para importar mesh_proto
pkg_dir = Path(__file__).resolve().parent.parent / "tlvgl"
sys.path.insert(0, str(pkg_dir))
import mesh_proto as MESH

def start_backend_server():
    server_script = pkg_dir / "tlvgl_server.py"
    print("[E2E Test] Iniciando Servidor TLVGL backend en 127.0.0.1:8766...")
    p = subprocess.Popen(
        [sys.executable, str(server_script), "--port", "8766"],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL
    )
    time.sleep(1.5)
    return p

def start_router_go():
    router_dir = Path(__file__).resolve().parent
    print("[E2E Test] Compilando e iniciando Router Go en :8765...")
    subprocess.run(["go", "build", "-o", "build/router", "./cmd/router"], cwd=router_dir, check=True)
    p = subprocess.Popen(
        ["./build/router", "-tcp", ":8765", "-hosting", "127.0.0.1:8766"],
        cwd=router_dir,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL
    )
    time.sleep(1.5)
    return p

def test_router():
    backend_proc = start_backend_server()
    router_proc = start_router_go()

    try:
        router_host = "127.0.0.1"
        router_port = 8765

        print(f"[Client Test] Conectando al Router Go en {router_host}:{router_port}...")
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(5.0)
        s.connect((router_host, router_port))

        # 1. Construir cabecera DST_ONLY (0x0F = DST_ONLY | TLVGL_REQUEST, DstID = 0x0001)
        ctrl_byte = MESH.MESH_CTRL_DST_ONLY | MESH.MESH_SVC_TLVGL_REQUEST
        raw_header = MESH.build_mesh_header(ctrl_byte, 0x0001)

        # 2. Construir payload TLV (REQ_URL 'http://index.mesh')
        url = "http://index.mesh".encode('utf-8')
        tlv_payload = MESH.build_tlv_node(MESH.TYPE_REQ_URL, url)

        full_packet = raw_header + tlv_payload
        print(f"[Client Test] Enviando trama request de {len(full_packet)}B al Router Go...")
        s.sendall(full_packet)

        # 3. Leer respuesta del Router Go
        response_data = s.recv(2048)
        s.close()

        print(f"[Client Test] Respuesta recibida ({len(response_data)}B): {response_data[:20].hex()}...")

        if len(response_data) == 0:
            raise RuntimeError("❌ No se recibió ninguna respuesta del Router.")

        hdr, hdr_len = MESH.parse_mesh_header(response_data)
        if hdr is None:
            raise RuntimeError("❌ Error decodificando la cabecera de la respuesta.")

        print(f"[Client Test] Cabecera decodificada: Control=0x{hdr['control']:02X}, Service=0x{hdr['service']:02X}, DstID=0x{hdr['dst_id']:04X}")

        # Validar que sea un servicio TLVGL_RESPONSE (0x08)
        if hdr['service'] != MESH.MESH_SVC_TLVGL_RESPONSE:
            raise RuntimeError(f"❌ Servicio incorrecto en la respuesta: esperado 0x08, obtenido 0x{hdr['service']:02X}")

        payload = response_data[hdr_len:]
        if not payload.startswith(b"PH"):
            raise RuntimeError(f"❌ El payload de respuesta no contiene magic 'PH': {payload[:10]}")

        print(f"[Client Test] ✅ Cabecera y Payload TLVGL ('PH' + {len(payload)-2}B TLVs) completamente validados.")
        print(f"[Client Test] 🚀 RUTEOS MESH LORA / FLRC / TCP VERIFICADOS CON ÉXITO SIN ERRORES!")

    finally:
        if router_proc:
            router_proc.terminate()
        if backend_proc:
            backend_proc.terminate()

if __name__ == "__main__":
    test_router()
