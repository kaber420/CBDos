#!/usr/bin/env python3
"""
Módulo de compresión de diccionarios dinámicos híbridos (SIMD Plan 3)
para el Gateway y Compilador TLVGL de CBDos.
"""

# 1. Rango VIP (0x80 - 0xBF): 64 Valores | 1 Byte Total
VIP_WORDS = [
    " que ",       # 0x80
    " de ",        # 0x81
    " la ",        # 0x82
    " y ",         # 0x83
    " el ",        # 0x84
    " en ",        # 0x85
    " a ",         # 0x86
    " los ",       # 0x87
    " se ",        # 0x88
    " del ",       # 0x89
    " las ",       # 0x8A
    " un ",        # 0x8B
    " por ",       # 0x8C
    " con ",       # 0x8D
    " no ",        # 0x8E
    " una ",       # 0x8F
    " su ",        # 0x90
    " para ",      # 0x91
    " es ",        # 0x92
    " al ",        # 0x93
    " lo ",        # 0x94
    " como ",      # 0x95
    " más ",       # 0x96
    " pero ",      # 0x97
    " sus ",       # 0x98
    " le ",        # 0x99
    " ya ",        # 0x9A
    " o ",         # 0x9B
    " este ",      # 0x9C
    " sí ",        # 0x9D
    " porque ",    # 0x9E
    " esta ",      # 0x9F
    " entre ",     # 0xA0
    " cuando ",    # 0xA1
    " muy ",       # 0xA2
    " sin ",       # 0xA3
    " sobre ",     # 0xA4
    " también ",   # 0xA5
    " me ",        # 0xA6
    " hasta ",     # 0xA7
    " hay ",       # 0xA8
    " donde ",     # 0xA9
    " quien ",     # 0xAA
    " desde ",     # 0xAB
    " todo ",      # 0xAC
    " nos ",       # 0xAD
    " durante ",   # 0xAE
    " todos ",     # 0xAF
    " uno ",       # 0xB0
    " les ",       # 0xB1
    " ni ",        # 0xB2
    " contra ",    # 0xB3
    " otros ",     # 0xB4
    " ese ",       # 0xB5
    " eso ",       # 0xB6
    " ante ",      # 0xB7
    " ellos ",     # 0xB8
    " e ",         # 0xB9
    " esto ",      # 0xBA
    " mí ",        # 0xBB
    "https://",    # 0xBC
    "http://",     # 0xBD
    ".com",        # 0xBE
    ".mesh"        # 0xBF
]

# 2. Rango Core Local (0xC0 - 0xDF): Banco 0 (Vocabulario UI / Telemetría)
CORE_BANK0 = [
    "Temperatura",    # 0xC0, 0x00
    "Humedad",        # 0xC0, 0x01
    "Presion",        # 0xC0, 0x02
    "Bateria",        # 0xC0, 0x03
    "Voltaje",        # 0xC0, 0x04
    "Telemetria",     # 0xC0, 0x05
    "Configuracion",  # 0xC0, 0x06
    "Estacion",       # 0xC0, 0x07
    "Noticias",       # 0xC0, 0x08
    "Sensores",       # 0xC0, 0x09
    "Radio LoRa",     # 0xC0, 0x0A
    "Radio WiFi",     # 0xC0, 0x0B
    "Bluetooth",      # 0xC0, 0x0C
    "Conectado",      # 0xC0, 0x0D
    "Desconectado",   # 0xC0, 0x0E
    "Activo",         # 0xC0, 0x0F
    "Inactivo",       # 0xC0, 0x10
    "Encendido",      # 0xC0, 0x11
    "Apagado",        # 0xC0, 0x12
    "Guardar",        # 0xC0, 0x13
    "Cancelar",       # 0xC0, 0x14
    "Aceptar",        # 0xC0, 0x15
    "Volver",         # 0xC0, 0x16
    "Inicio",         # 0xC0, 0x17
    "Detalle",        # 0xC0, 0x18
    "Actualizar",     # 0xC0, 0x19
    "Servidor",       # 0xC0, 0x1A
    "Cliente",        # 0xC0, 0x1B
    "Gateway",        # 0xC0, 0x1C
    "Nodo",           # 0xC0, 0x1D
    "Canal",          # 0xC0, 0x1E
    "Potencia",       # 0xC0, 0x1F
    "Frecuencia",     # 0xC0, 0x20
    "Sensibilidad",   # 0xC0, 0x21
    "Mensaje",        # 0xC0, 0x22
    "Alerta",         # 0xC0, 0x23
    "Dispositivo",    # 0xC0, 0x24
    "Memoria",        # 0xC0, 0x25
    "Almacenamiento", # 0xC0, 0x26
    "Clima",          # 0xC0, 0x27
    "Precipitacion",  # 0xC0, 0x28
    "Viento",         # 0xC0, 0x29
    "Calidad Aire",   # 0xC0, 0x2A
    "Radiacion",      # 0xC0, 0x2B
    "Luz",            # 0xC0, 0x2C
    "Sonido",         # 0xC0, 0x2D
    "Nivel",          # 0xC0, 0x2E
    "Porcentaje",     # 0xC0, 0x2F
    "Velocidad",      # 0xC0, 0x30
    "Tiempo",         # 0xC0, 0x31
    "Hora",           # 0xC0, 0x32
    "Fecha",          # 0xC0, 0x33
    "Historial",      # 0xC0, 0x34
    "Grafica",        # 0xC0, 0x35
    "Tabla",          # 0xC0, 0x36
    "Red Mesh",       # 0xC0, 0x37
    "Enlace",         # 0xC0, 0x38
    "Ruta",           # 0xC0, 0x39
    "Metrica",        # 0xC0, 0x3A
    "IP Local",       # 0xC0, 0x3B
    "Mascara",        # 0xC0, 0x3C
    "Puerta Enlace",  # 0xC0, 0x3D
    "DNS",            # 0xC0, 0x3E
    "Seguridad"       # 0xC0, 0x3F
]

_VIP_MAP = {word: bytes([0x80 + i]) for i, word in enumerate(VIP_WORDS)}
_CORE_MAP = {word: bytes([0xC0, i]) for i, word in enumerate(CORE_BANK0)}

def encode_hybrid_text(text: str) -> bytes:
    """
    Codifica un texto aplicando el diccionario híbrido:
    - Palabras VIP -> 1 Byte (0x80..0xBF)
    - Palabras Core -> 2 Bytes (0xC0, idx)
    - Caracteres crudos ASCII -> 1 Byte directo (0x00..0x7F)
    """
    if not text:
        return b""

    output = bytearray()
    i = 0
    text_len = len(text)

    while i < text_len:
        match_found = False

        # 1. Probar tokens VIP (1 Byte)
        for word, code in _VIP_MAP.items():
            if text.startswith(word, i):
                output.extend(code)
                i += len(word)
                match_found = True
                break

        if match_found:
            continue

        # 2. Probar palabras Core (2 Bytes)
        for word, code in _CORE_MAP.items():
            if text.startswith(word, i):
                output.extend(code)
                i += len(word)
                match_found = True
                break

        if match_found:
            continue

        # 3. Caracter crudo (ASCII directo 1B o UTF-8)
        char_bytes = text[i].encode('utf-8')
        output.extend(char_bytes)
        i += 1

    return bytes(output)
