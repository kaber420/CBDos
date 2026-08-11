#!/usr/bin/env python3
"""
Pasarela Servidor Prototipo TLVGL (Python)
Escucha en puerto TCP 8765 y responde a peticiones del Navegador C/C++ de espOS32.
Soporta la cabecera de pseudo-ruteo MeshHeader (3B, 9B, 13B, 21B) y tramas TLV.
"""

import socket
import struct
import sys

PORT = 8765
HOST = "0.0.0.0"

# Tags TLV Downlink
TYPE_ABS_PAGE     = 0x10
TYPE_ABS_TEXT     = 0x11
TYPE_ABS_LINK     = 0x12
TYPE_ABS_INPUT    = 0x13
TYPE_ABS_IMAGE    = 0x14
TYPE_ABS_CHECKBOX = 0x15
TYPE_ABS_SWITCH   = 0x16
TYPE_ABS_SLIDER   = 0x17
TYPE_ABS_PROGRESS = 0x18
TYPE_ABS_DROPDOWN = 0x19
TYPE_END          = 0xFE

# Tags TLV Uplink
TYPE_REQ_URL          = 0x01
TYPE_REQ_INPUT_SUBMIT = 0x20
TYPE_REQ_LINK_CLICK   = 0x21

def build_tlv_node(tag: int, payload: bytes) -> bytes:
    return struct.pack(">BH", tag, len(payload)) + payload

def build_gallery_page() -> bytes:
    buf = bytearray()
    buf.extend(b"PH")
    buf.extend(build_tlv_node(TYPE_ABS_PAGE, b""))
    
    # Titulo
    val_title = struct.pack(">HHHHB", 15, 10, 270, 30, 1) + "Galeria de Fotos Mesh".encode('utf-8')
    buf.extend(build_tlv_node(TYPE_ABS_TEXT, val_title))
    
    # Subtitulo
    val_sub = struct.pack(">HHHHB", 15, 45, 270, 25, 0) + "Servidor de Imagenes TLV".encode('utf-8')
    buf.extend(build_tlv_node(TYPE_ABS_TEXT, val_sub))
    
    # Checkbox Filtro
    val_cb = struct.pack(">BB", 1, 1) + "Mostrar HD".encode('utf-8')
    buf.extend(build_tlv_node(TYPE_ABS_CHECKBOX, val_cb))
    
    # Switch Modo Noche
    val_sw = struct.pack(">BB", 2, 0)
    buf.extend(build_tlv_node(TYPE_ABS_SWITCH, val_sw))
    
    # Slider Brillo
    val_sl = struct.pack(">Bhhh", 3, 0, 100, 85)
    buf.extend(build_tlv_node(TYPE_ABS_SLIDER, val_sl))
    
    # Boton Foto 1
    val_btn1 = struct.pack(">HHHHB", 15, 190, 270, 38, 101) + "Foto: Atardecer AMOLED".encode('utf-8')
    buf.extend(build_tlv_node(TYPE_ABS_LINK, val_btn1))

    # Boton Foto 2
    val_btn2 = struct.pack(">HHHHB", 15, 235, 270, 38, 102) + "Foto: Noche Estrellada".encode('utf-8')
    buf.extend(build_tlv_node(TYPE_ABS_LINK, val_btn2))

    buf.append(TYPE_END)
    return bytes(buf)

def build_news_page() -> bytes:
    buf = bytearray()
    buf.extend(b"PH")
    buf.extend(build_tlv_node(TYPE_ABS_PAGE, b""))
    
    val_title = struct.pack(">HHHHB", 15, 10, 270, 30, 1) + "Portal de Noticias Mesh".encode('utf-8')
    buf.extend(build_tlv_node(TYPE_ABS_TEXT, val_title))
    
    val_sub = struct.pack(">HHHHB", 15, 45, 270, 25, 0) + "Red Operativa a 60 FPS".encode('utf-8')
    buf.extend(build_tlv_node(TYPE_ABS_TEXT, val_sub))
    
    val_pr = struct.pack(">hhh", 0, 100, 100)
    buf.extend(build_tlv_node(TYPE_ABS_PROGRESS, val_pr))
    
    val_btn1 = struct.pack(">HHHHB", 15, 140, 270, 38, 201) + "Ultima hora en la Red".encode('utf-8')
    buf.extend(build_tlv_node(TYPE_ABS_LINK, val_btn1))
    
    val_btn2 = struct.pack(">HHHHB", 15, 185, 270, 38, 202) + "Estado de Nodos OSPF".encode('utf-8')
    buf.extend(build_tlv_node(TYPE_ABS_LINK, val_btn2))

    buf.append(TYPE_END)
    return bytes(buf)

def build_weather_page() -> bytes:
    buf = bytearray()
    buf.extend(b"PH")
    buf.extend(build_tlv_node(TYPE_ABS_PAGE, b""))
    
    val_title = struct.pack(">HHHHB", 15, 10, 270, 30, 1) + "Estado del Clima".encode('utf-8')
    buf.extend(build_tlv_node(TYPE_ABS_TEXT, val_title))
    
    val_sub = struct.pack(">HHHHB", 15, 45, 270, 25, 0) + "Temperatura: 24 C - Soleado".encode('utf-8')
    buf.extend(build_tlv_node(TYPE_ABS_TEXT, val_sub))
    
    val_sl = struct.pack(">Bhhh", 3, 0, 100, 45)
    buf.extend(build_tlv_node(TYPE_ABS_SLIDER, val_sl))
    
    val_cb = struct.pack(">BB", 1, 1) + "Alertas activadas".encode('utf-8')
    buf.extend(build_tlv_node(TYPE_ABS_CHECKBOX, val_cb))

    buf.append(TYPE_END)
    return bytes(buf)

def build_sample_tlv_page(url_requested: str) -> bytes:
    url_lower = url_requested.lower()
    if "galeria" in url_lower:
        return build_gallery_page()
    elif "clima" in url_lower:
        return build_weather_page()
    elif "noticias" in url_lower:
        return build_news_page()
    
    # Generica
    buf = bytearray()
    buf.extend(b"PH")
    buf.extend(build_tlv_node(TYPE_ABS_PAGE, b""))
    
    title = f"Pagina: {url_requested}".encode('utf-8')
    val_title = struct.pack(">HHHHB", 15, 15, 270, 30, 1) + title
    buf.extend(build_tlv_node(TYPE_ABS_TEXT, val_title))
    
    val_sub = struct.pack(">HHHHB", 15, 50, 270, 30, 0) + "Respuesta Gateway Python".encode('utf-8')
    buf.extend(build_tlv_node(TYPE_ABS_TEXT, val_sub))
    
    val_cb = struct.pack(">BB", 1, 1) + "Conexion Mesh Ok".encode('utf-8')
    buf.extend(build_tlv_node(TYPE_ABS_CHECKBOX, val_cb))
    
    val_sw = struct.pack(">BB", 2, 1)
    buf.extend(build_tlv_node(TYPE_ABS_SWITCH, val_sw))
    
    val_sl = struct.pack(">Bhhh", 3, 0, 100, 75)
    buf.extend(build_tlv_node(TYPE_ABS_SLIDER, val_sl))
    
    val_pr = struct.pack(">hhh", 0, 100, 90)
    buf.extend(build_tlv_node(TYPE_ABS_PROGRESS, val_pr))
    
    val_link = struct.pack(">HHHHB", 15, 240, 200, 40, 10) + "Refrescar Pagina".encode('utf-8')
    buf.extend(build_tlv_node(TYPE_ABS_LINK, val_link))
    
    buf.append(TYPE_END)
    return bytes(buf)

def build_mesh_response_header(payload: bytes) -> bytes:
    ctrl = 0x08 | 0x08
    hdr = struct.pack(">BH", ctrl, 0x0001)
    return hdr + payload

def handle_client(conn, addr):
    print(f"[+] Cliente conectado desde {addr}")
    try:
        while True:
            data = conn.recv(1024)
            if not data:
                break
            
            print(f"[<-] Recibidos {len(data)} bytes desde ESP32: {data.hex()}")
            
            ctrl = data[0]
            print(f"     Control Byte: 0x{ctrl:02X}")
            
            hdr_offset = 3 if (ctrl & 0x08) else 9
            
            if len(data) > hdr_offset:
                tlv_payload = data[hdr_offset:]
                tag = tlv_payload[0]
                length = (tlv_payload[1] << 8) | tlv_payload[2]
                val = tlv_payload[3:3+length]
                
                requested_url = "http://home.mesh"
                if tag == TYPE_REQ_URL:
                    requested_url = val.decode('utf-8', errors='ignore')
                    print(f"     Petición URL recibida: '{requested_url}'")
                elif tag == TYPE_REQ_LINK_CLICK:
                    link_id = val[0] if len(val) > 0 else 0
                    print(f"     Clic en Enlace recibido: ID {link_id}")
                    if link_id == 101 or link_id == 102:
                        requested_url = "http://galeria.mesh"
                    elif link_id == 201 or link_id == 202:
                        requested_url = "http://noticias.mesh"
                elif tag == TYPE_REQ_INPUT_SUBMIT:
                    elem_id = val[0]
                    txt = val[1:].decode('utf-8', errors='ignore')
                    print(f"     Formulario Submit: Elem {elem_id} -> '{txt}'")
                    requested_url = txt
                
                # Generar respuesta TLV binaria dinámica
                page_tlv = build_sample_tlv_page(requested_url)
                response_packet = build_mesh_response_header(page_tlv)
                
                conn.sendall(response_packet)
                print(f"[->] Enviada respuesta TLV ({len(response_packet)} bytes) para '{requested_url}' al ESP32.")
    except Exception as e:
        print(f"[-] Error con el cliente: {e}")
    finally:
        conn.close()
        print(f"[-] Cliente desconectado {addr}")

def main():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind((HOST, PORT))
    s.listen(5)
    print(f"=== Pasarela TLVGL Python Servidor iniciada en el puerto {PORT} ===")
    print("Esperando peticiones del cliente ESP32...")
    
    while True:
        conn, addr = s.accept()
        handle_client(conn, addr)

if __name__ == "__main__":
    main()
