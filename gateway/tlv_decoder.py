import struct

# Definición de tipos (Igual que en el ESP32)
TYPE_PAGE = 0x01
TYPE_CONTAINER = 0x02
TYPE_TEXT = 0x03
TYPE_LINK = 0x04
TYPE_IMAGE = 0x05
TYPE_END = 0xFE

def decode_tlv(binary_data):
    """
    Simula lo que hará el microcontrolador ESP32 en C.
    Lee el flujo binario byte por byte.
    """
    print("--- INICIANDO RENDERIZADO (SIMULACIÓN LVGL) ---")
    
    offset = 0
    total_length = len(binary_data)
    indent_level = 0
    
    def get_indent():
        return "  " * indent_level

    while offset < total_length:
        # 1. Leer el TIPO (1 byte)
        node_type = binary_data[offset]
        offset += 1
        
        # 2. Leer la LONGITUD (2 bytes, big-endian)
        node_length = struct.unpack_from(">H", binary_data, offset)[0]
        offset += 2
        
        # 3. Extraer el VALOR
        value = binary_data[offset : offset + node_length]
        offset += node_length
        
        # 4. Construir la UI según el tipo
        if node_type == TYPE_PAGE:
            print(f"{get_indent()}[C_API] lv_obj_clean(lv_scr_act()); // Limpiando pantalla")
            print(f"{get_indent()}[C_API] current_parent = lv_scr_act();")
            indent_level += 1
            
        elif node_type == TYPE_CONTAINER:
            layout_dir = value[0]
            dir_str = "LV_FLEX_ALIGN_COLUMN" if layout_dir == 0 else "LV_FLEX_ALIGN_ROW"
            print(f"{get_indent()}[C_API] cont = lv_obj_create(current_parent);")
            print(f"{get_indent()}[C_API] lv_obj_set_layout(cont, {dir_str});")
            print(f"{get_indent()}[C_API] current_parent = cont; // Anidando")
            indent_level += 1
            
        elif node_type == TYPE_TEXT:
            style = value[0]
            text_str = value[1:].decode("utf-8")
            style_str = "normal"
            if style == 1: style_str = "H1 (Fuente Grande)"
            elif style == 2: style_str = "H2"
            print(f"{get_indent()}[C_API] label = lv_label_create(current_parent);")
            print(f"{get_indent()}[C_API] lv_label_set_text(label, \"{text_str}\");")
            print(f"{get_indent()}[C_API] lv_obj_add_style(label, style_{style_str});")
            
        elif node_type == TYPE_LINK:
            link_id = value[0]
            text_str = value[1:].decode("utf-8")
            print(f"{get_indent()}[C_API] btn = lv_btn_create(current_parent);")
            print(f"{get_indent()}[C_API] lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, (void*){link_id});")
            print(f"{get_indent()}[C_API] label = lv_label_create(btn);")
            print(f"{get_indent()}[C_API] lv_label_set_text(label, \"{text_str}\");")
            
        elif node_type == TYPE_END:
            indent_level -= 1
            print(f"{get_indent()}[C_API] current_parent = lv_obj_get_parent(current_parent); // Saliendo del contenedor")
            
    print("--- RENDERIZADO COMPLETADO ---")


if __name__ == "__main__":
    # Esta es exactamente la secuencia de bytes que generó nuestro tlv_encoder.py
    # para el HTML: <body><div><h1>Alternet</h1><p>Hola mundo</p><a>OK</a></div></body>
    
    paquete_simulado_desde_radio = bytes.fromhex(
        "01 00 00 "
        "02 00 01 00 "
        "03 00 09 01 41 6C 74 65 72 6E 65 74 "
        "03 00 0B 00 48 6F 6C 61 20 6D 75 6E 64 6F "
        "04 00 03 01 4F 4B "
        "FE 00 00 "
        "FE 00 00"
    )
    
    print("Recibiendo paquete por FLRC (39 bytes)...\n")
    decode_tlv(paquete_simulado_desde_radio)
