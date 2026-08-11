import struct
from html.parser import HTMLParser

# Tipos definidos en Alternet TLV
TYPE_PAGE = 0x01
TYPE_CONTAINER = 0x02
TYPE_TEXT = 0x03
TYPE_LINK = 0x04
TYPE_IMAGE = 0x05
TYPE_END = 0xFE

# Estilos de texto (para enviar como el primer byte del VALUE)
TEXT_STYLE_P = 0x00
TEXT_STYLE_H1 = 0x01
TEXT_STYLE_H2 = 0x02
TEXT_STYLE_B = 0x03

class AlternetHTMLParser(HTMLParser):
    def __init__(self):
        super().__init__()
        self.tlv_output = bytearray()
        self.current_tag = None
        self.link_id_counter = 1
        self.current_link_id = 0

    def append_tlv(self, tlv_type, value_bytes=b""):
        length = len(value_bytes)
        # Empaqueta: 1 byte (Type), 2 bytes (Length, big-endian), y el Value
        self.tlv_output.extend(struct.pack(f">BH{length}s", tlv_type, length, value_bytes))

    def handle_starttag(self, tag, attrs):
        self.current_tag = tag
        if tag == "body":
            self.append_tlv(TYPE_PAGE)
        elif tag == "div":
            # 0x00 representa Container Vertical (Column)
            self.append_tlv(TYPE_CONTAINER, b"\x00")
        elif tag == "a":
            # Link asignado con un ID auto-incremental
            self.current_link_id = self.link_id_counter
            self.link_id_counter += 1
            
    def handle_endtag(self, tag):
        self.current_tag = None
        if tag in ["body", "div"]:
            self.append_tlv(TYPE_END)

    def handle_data(self, data):
        text = data.strip()
        if not text:
            return

        if self.current_tag in ["h1", "h2", "p", "span"]:
            style = TEXT_STYLE_P
            if self.current_tag == "h1": style = TEXT_STYLE_H1
            elif self.current_tag == "h2": style = TEXT_STYLE_H2
            
            value = struct.pack(">B", style) + text.encode("utf-8")
            self.append_tlv(TYPE_TEXT, value)
            
        elif self.current_tag == "a":
            value = struct.pack(">B", self.current_link_id) + text.encode("utf-8")
            self.append_tlv(TYPE_LINK, value)

import sys

if __name__ == "__main__":
    if len(sys.argv) > 1:
        with open(sys.argv[1], "r", encoding="utf-8") as f:
            html_test = f.read()
    else:
        html_test = """
    <body>
        <div>
            <h1>Alternet</h1>
            <p>Hola mundo</p>
            <a>OK</a>
        </div>
    </body>
    """
    
    print("HTML Original:")
    print(html_test.strip())
    print("-" * 40)
    
    parser = AlternetHTMLParser()
    parser.feed(html_test)
    
    print(f"Salida Binaria TLV (Total: {len(parser.tlv_output)} bytes):")
    hex_str = " ".join([f"{b:02X}" for b in parser.tlv_output])
    print(hex_str)
    
    print("\nDesglose de los bytes (Para LVGL):")
    print("01 00 00 -> PAGE (0 bytes data)")
    print("02 00 01 00 -> CONTAINER (1 byte data: Dir Column)")
    print("03 00 09 01 41 6C 74 65 72 6E 65 74 -> TEXT (H1: Alternet)")
    print("03 00 0B 00 48 6F 6C 61 20 6D 75 6E 64 6F -> TEXT (P: Hola mundo)")
    print("04 00 03 01 4F 4B -> LINK (ID 1: OK)")
    print("FE 00 00 -> END_NODE (Cierra div)")
    print("FE 00 00 -> END_NODE (Cierra body)")
