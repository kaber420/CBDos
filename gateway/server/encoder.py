import struct

# Tipos Absolutos Definidos en Alternet TLV
TYPE_ABS_TEXT = 0x11
TYPE_ABS_LINK = 0x12
TYPE_ABS_INPUT = 0x13
TYPE_ABS_IMAGE = 0x14
TYPE_ABS_PAGE = 0x10

def encode_elements_to_tlv(elements: list) -> bytes:
    tlv_output = bytearray()
    
    # 1. Enviar dimensiones de la página (opcional, por si el ESP32 quiere hacer scroll exacto)
    # Aquí podríamos calcular el max Y, pero por ahora solo mandaremos los elementos
    
    for el in elements:
        x = el.get('x', 0)
        y = el.get('y', 0)
        w = el.get('w', 0)
        h = el.get('h', 0)

        # Evitar overflow en struct.pack(">H") que solo soporta hasta 65535
        if x < 0 or x > 65000 or y < 0 or y > 65000 or w < 0 or w > 65000 or h < 0 or h > 65000:
            continue
        
        if el['type'] == 'text':
            style = el.get('style', 0)
            text_bytes = el['text'].encode('utf-8', errors='replace')
            # Payload: [X:2][Y:2][W:2][H:2][STYLE:1] + text
            val = struct.pack(">HHHHB", x, y, w, h, style) + text_bytes
            
            length = len(val)
            tlv_output.extend(struct.pack(f">BH{length}s", TYPE_ABS_TEXT, length, val))
            
        elif el['type'] == 'link':
            link_id = el.get('link_id', 0)
            text_bytes = el['text'].encode('utf-8', errors='replace')
            # Payload: [X:2][Y:2][W:2][H:2][LINK_ID:1] + text
            val = struct.pack(">HHHHB", x, y, w, h, link_id) + text_bytes
            
            length = len(val)
            tlv_output.extend(struct.pack(f">BH{length}s", TYPE_ABS_LINK, length, val))
            
        elif el['type'] == 'input':
            action = el.get('action', "")
            param = el.get('name', "")
            placeholder = el.get('placeholder', "")
            str_payload = f"{action}\x00{param}\x00{placeholder}".encode('utf-8', errors='replace')
            # Payload: [X:2][Y:2][W:2][H:2] + action\0param\0placeholder
            val = struct.pack(">HHHH", x, y, w, h) + str_payload
            
            length = len(val)
            tlv_output.extend(struct.pack(f">BH{length}s", TYPE_ABS_INPUT, length, val))
            
    return bytes(tlv_output)
