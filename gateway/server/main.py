import socket
import struct
import os
import urllib.parse
import threading

# Motores
from renderer_raw import start_raw_engine as start_engine, stop_raw_engine as stop_engine
from renderer_raw import render_page_raw as render_page

from encoder import encode_elements_to_tlv

MAX_W = 480
MAX_H = 640

def parse_request(raw: str):
    """Parsea 'GET http://url W=240 H=320' o legado 'http://url'.
    Retorna (url, screen_w, screen_h)."""
    parts = raw.strip().split()
    if not parts:
        return '', MAX_W, MAX_H
    # Formato nuevo: GET <url> [W=N] [H=N]
    if parts[0].upper() == 'GET' and len(parts) >= 2:
        url = parts[1]
    else:
        # Formato legado: solo la URL
        url = parts[0]
    screen_w = MAX_W
    screen_h = MAX_H
    for p in parts:
        if p.startswith('W='):
            try: screen_w = min(int(p[2:]), MAX_W)
            except ValueError: pass
        elif p.startswith('H='):
            try: screen_h = min(int(p[2:]), MAX_H)
            except ValueError: pass
    return url, screen_w, screen_h

def handle_client(conn, addr):
    print(f"[{addr}] Cliente conectado")
    try:
        data = conn.recv(1024)
        if not data:
            return
            
        raw = data.decode('utf-8', errors='ignore')
        url, screen_w, screen_h = parse_request(raw)
        print(f"[{addr}] Resolución solicitada: {screen_w}x{screen_h}")
        print(f"[{addr}] URL solicitada: {url}")
        
        if not url.startswith('http'):
            url = 'https://' + url
            
        print(f"[{addr}] Renderizando en Motor Web...")
        elements = render_page(url, width=screen_w, height=screen_h)
        print(f"[{addr}] Extracted {len(elements)} visual elements")
        
        # 2. Codificar a TLV Absoluto
        tlv_payload = encode_elements_to_tlv(elements)
        print(f"[{addr}] Encoded to {len(tlv_payload)} bytes of TLV")
        
        # 3. Comprimir (saltado por ahora)
        compressed_payload = tlv_payload
        
        # 4. Enviar
        header = len(compressed_payload).to_bytes(4, byteorder='big')
        conn.sendall(header + compressed_payload)
        print(f"[{addr}] Payload sent successfully.")
        
    except Exception as e:
        print(f"[{addr}] Error handling client: {e}")
    finally:
        conn.close()

def start_gateway():
    print("Iniciando Motor Crudo por WebSockets (Raw BiDi CDP)...")
    start_engine()
    
    # TCP Server (simulating the Alternet Radio Protocol)
    HOST = '0.0.0.0'
    PORT = 8080

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind((HOST, PORT))
        s.listen()
        print(f"Gateway Server listening on TCP {HOST}:{PORT} (Dual-Engine: Raw BiDi=True)")
        
        try:
            while True:
                conn, addr = s.accept()
                client_thread = threading.Thread(target=handle_client, args=(conn, addr))
                client_thread.start()
        except KeyboardInterrupt:
            print("\nShutting down Gateway...")
            stop_engine()

if __name__ == "__main__":
    start_gateway()
