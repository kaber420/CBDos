# gateway/tlvgl/simd_dict.py

# ==========================================
# DICCIONARIO HÍBRIDO UNIFICADO (SIMD PLAN 3)
# ==========================================

# 1. Rango VIP (0x80 - 0xBF): 64 Valores | 1 Byte Total
# Palabras más comunes en español e inglés, y atajos web.
VIP_WORDS = [
    " que ", " de ", " la ", " y ", " el ", " en ", " a ", " los ", " se ", " del ",
    " las ", " un ", " por ", " con ", " no ", " una ", " su ", " para ", " es ", " al ",
    " lo ", " como ", " más ", " pero ", " sus ", " le ", " ya ", " o ", " este ", " sí ",
    " porque ", " esta ", " entre ", " cuando ", " muy ", " sin ", " sobre ", " también ", " me ", " hasta ",
    " hay ", " donde ", " quien ", " desde ", " todo ", " nos ", " durante ", " todos ", " uno ", " les ",
    " ni ", " contra ", " otros ", " ese ", " eso ", " ante ", " ellos ", " e ", " esto ", " mí ",
    "https://", "http://", ".com", ".mesh"
]

# Construir diccionario inverso para compresión rápida O(1)
_VIP_MAP = {}
for i, word in enumerate(VIP_WORDS):
    # El rango VIP empieza en 0x80
    _VIP_MAP[word] = bytes([0x80 + i])

# 2. Rango Core Local (0xC0 - 0xDF): 32 Bloques x 256
# (Por implementar: Se llenará leyendo un archivo JSON de corpus estadístico)

def encode_hybrid_text(text: str) -> bytes:
    """
    Toma un texto UTF-8 y aplica el algoritmo de compresión de diccionarios
    híbridos (VIP 1B, Core 2B).
    """
    output = bytearray()
    
    # Algoritmo de compresión básico (Greedy)
    # Buscamos primero las palabras del diccionario VIP.
    # En una implementación avanzada, esto usaría un Trie (Aho-Corasick)
    
    i = 0
    text_len = len(text)
    
    while i < text_len:
        match_found = False
        
        # Intentar coincidir con las palabras VIP
        for word, code in _VIP_MAP.items():
            word_len = len(word)
            if text.startswith(word, i):
                output.extend(code)
                i += word_len
                match_found = True
                break
                
        if not match_found:
            # Si no hay match, enviamos el caracter crudo UTF-8 (menor a 0x80, o multibyte standard)
            # Nota: Los bytes de UTF-8 de caracteres especiales pueden pisar >0x80.
            # En la descompresión, debemos saber diferenciar entre un token TLV y UTF-8.
            # Según tu plan, la app o gateway gestionará eso asegurando que UTF-8 multibyte
            # se procese completo antes de buscar tokens SIMD.
            char_bytes = text[i].encode('utf-8')
            output.extend(char_bytes)
            i += 1
            
    return bytes(output)

if __name__ == "__main__":
    # Prueba
    test = "Este es un texto para probar que la compresión funciona https://google.com"
    comp = encode_hybrid_text(test)
    print(f"Original: {len(test)} bytes")
    print(f"Comprimido: {len(comp)} bytes")
    print(f"Hex: {comp.hex()}")
