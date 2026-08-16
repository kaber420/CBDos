import struct
import sys
import os
import zipfile
import xml.etree.ElementTree as ET
from html.parser import HTMLParser

# Intentar importar el encoder TLV del proyecto (si está en la ruta)
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '../../gateway')))
try:
    from tlvgl.simd_dict import encode_hybrid_text
    HAS_TLV = True
except ImportError:
    HAS_TLV = False

MAGIC_NUMBER = b'CBDB'
VERSION = 1

class EpubTextExtractor(HTMLParser):
    def __init__(self):
        super().__init__()
        self.text_chunks = []

    def handle_data(self, data):
        text = data.strip()
        if text:
            self.text_chunks.append(text)
            
    def get_text(self):
        return '\n'.join(self.text_chunks)

def get_epub_content_files(epub_path):
    """Extrae la lista de archivos HTML/XHTML del EPUB en orden de lectura."""
    html_files = []
    with zipfile.ZipFile(epub_path, 'r') as z:
        # Simplificación: En un parser robusto, se lee META-INF/container.xml 
        # y luego el .opf. Por ahora, tomaremos todos los HTML/XHTML.
        for name in z.namelist():
            if name.endswith(('.html', '.xhtml', '.htm')):
                html_files.append(name)
    return html_files

def parse_epub(epub_path):
    """Extrae todo el texto plano de un EPUB."""
    print(f"Extrayendo texto de: {epub_path}...")
    full_text = ""
    with zipfile.ZipFile(epub_path, 'r') as z:
        files = get_epub_content_files(epub_path)
        for file_name in files:
            html_content = z.read(file_name).decode('utf-8', errors='ignore')
            parser = EpubTextExtractor()
            parser.feed(html_content)
            full_text += parser.get_text() + "\n\n"
    return full_text

def create_cbd_file(input_path, output_path):
    if not input_path.endswith('.epub'):
        print("Error: El archivo de entrada debe ser .epub")
        return

    text_content = parse_epub(input_path)
    print(f"Texto extraído: {len(text_content)} caracteres.")
    
    print(f"Creando archivo CBD: {output_path}")
    with open(output_path, 'wb') as f:
        # 1. Escribir Cabecera (16 bytes)
        f.write(MAGIC_NUMBER)                  # 4 bytes
        f.write(struct.pack('B', VERSION))     # 1 byte
        f.write(b'\x00\x00\x00')               # 3 bytes padding
        f.write(struct.pack('<I', 0))          # Offset Diccionario (0 = usar Universal de CBDos)
        f.write(struct.pack('<I', 0))          # Offset Indice de Páginas (placeholder)
        
        # 2. Codificar y escribir datos 
        if HAS_TLV:
            print("Aplicando compresión Híbrida SIMD (Diccionario Universal)...")
            compressed_bytes = encode_hybrid_text(text_content)
            f.write(struct.pack('B', 0x01)) # Tipo: Texto Comprimido
            f.write(struct.pack('<I', len(compressed_bytes))) # Longitud
            f.write(compressed_bytes)
            print(f"Texto comprimido de {len(text_content)} a {len(compressed_bytes)} bytes.")
        else:
            # Fallback temporal: Escribir como texto UTF-8 con bloque tipo 0x01 (Texto puro)
            print("Guardando como texto plano (simd_dict no encontrado)...")
            text_bytes = text_content.encode('utf-8')
            f.write(struct.pack('B', 0x01)) # Tipo: Texto
            f.write(struct.pack('<I', len(text_bytes))) # Longitud
            f.write(text_bytes)
        
        print("Archivo CBD generado exitosamente.")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Uso: python compiler.py <entrada.epub> <salida.cbd>")
        sys.exit(1)
        
    create_cbd_file(sys.argv[1], sys.argv[2])
