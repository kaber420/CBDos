#include "tlv_dictionary.hpp"
#include <string.h>
#include <stdio.h>

// ==========================================
// 1. RANGO VIP (0x80 - 0xBF): 64 Valores (1 Byte)
// ==========================================
static const char* const s_vip_dict[TLV_VIP_COUNT] = {
    " que ",       // 0x80
    " de ",        // 0x81
    " la ",        // 0x82
    " y ",         // 0x83
    " el ",        // 0x84
    " en ",        // 0x85
    " a ",         // 0x86
    " los ",       // 0x87
    " se ",        // 0x88
    " del ",       // 0x89
    " las ",       // 0x8A
    " un ",        // 0x8B
    " por ",       // 0x8C
    " con ",       // 0x8D
    " no ",        // 0x8E
    " una ",       // 0x8F
    " su ",        // 0x90
    " para ",      // 0x91
    " es ",        // 0x92
    " al ",        // 0x93
    " lo ",        // 0x94
    " como ",      // 0x95
    " más ",       // 0x96
    " pero ",      // 0x97
    " sus ",       // 0x98
    " le ",        // 0x99
    " ya ",        // 0x9A
    " o ",         // 0x9B
    " este ",      // 0x9C
    " sí ",        // 0x9D
    " porque ",    // 0x9E
    " esta ",      // 0x9F
    " entre ",     // 0xA0
    " cuando ",    // 0xA1
    " muy ",       // 0xA2
    " sin ",       // 0xA3
    " sobre ",     // 0xA4
    " también ",   // 0xA5
    " me ",        // 0xA6
    " hasta ",     // 0xA7
    " hay ",       // 0xA8
    " donde ",     // 0xA9
    " quien ",     // 0xAA
    " desde ",     // 0xAB
    " todo ",      // 0xAC
    " nos ",       // 0xAD
    " durante ",   // 0xAE
    " todos ",     // 0xAF
    " uno ",       // 0xB0
    " les ",       // 0xB1
    " ni ",        // 0xB2
    " contra ",    // 0xB3
    " otros ",     // 0xB4
    " ese ",       // 0xB5
    " eso ",       // 0xB6
    " ante ",      // 0xB7
    " ellos ",     // 0xB8
    " e ",         // 0xB9
    " esto ",      // 0xBA
    " mí ",        // 0xBB
    "https://",    // 0xBC
    "http://",     // 0xBD
    ".com",        // 0xBE
    ".mesh"        // 0xBF
};

// ==========================================
// 2. RANGO CORE LOCAL (0xC0 - 0xDF): Banco 0 (Vocabulario UI / Telemetría)
// ==========================================
static const char* const s_core_bank0[64] = {
    "Temperatura",    // 0xC0, 0x00
    "Humedad",        // 0xC0, 0x01
    "Presion",        // 0xC0, 0x02
    "Bateria",        // 0xC0, 0x03
    "Voltaje",        // 0xC0, 0x04
    "Telemetria",     // 0xC0, 0x05
    "Configuracion",  // 0xC0, 0x06
    "Estacion",       // 0xC0, 0x07
    "Noticias",       // 0xC0, 0x08
    "Sensores",       // 0xC0, 0x09
    "Radio LoRa",     // 0xC0, 0x0A
    "Radio WiFi",     // 0xC0, 0x0B
    "Bluetooth",      // 0xC0, 0x0C
    "Conectado",      // 0xC0, 0x0D
    "Desconectado",   // 0xC0, 0x0E
    "Activo",         // 0xC0, 0x0F
    "Inactivo",       // 0xC0, 0x10
    "Encendido",      // 0xC0, 0x11
    "Apagado",        // 0xC0, 0x12
    "Guardar",        // 0xC0, 0x13
    "Cancelar",       // 0xC0, 0x14
    "Aceptar",        // 0xC0, 0x15
    "Volver",         // 0xC0, 0x16
    "Inicio",         // 0xC0, 0x17
    "Detalle",        // 0xC0, 0x18
    "Actualizar",     // 0xC0, 0x19
    "Servidor",       // 0xC0, 0x1A
    "Cliente",        // 0xC0, 0x1B
    "Gateway",        // 0xC0, 0x1C
    "Nodo",           // 0xC0, 0x1D
    "Canal",          // 0xC0, 0x1E
    "Potencia",       // 0xC0, 0x1F
    "Frecuencia",     // 0xC0, 0x20
    "Sensibilidad",   // 0xC0, 0x21
    "Mensaje",        // 0xC0, 0x22
    "Alerta",         // 0xC0, 0x23
    "Dispositivo",    // 0xC0, 0x24
    "Memoria",        // 0xC0, 0x25
    "Almacenamiento", // 0xC0, 0x26
    "Clima",          // 0xC0, 0x27
    "Precipitacion",  // 0xC0, 0x28
    "Viento",         // 0xC0, 0x29
    "Calidad Aire",   // 0xC0, 0x2A
    "Radiacion",      // 0xC0, 0x2B
    "Luz",            // 0xC0, 0x2C
    "Sonido",         // 0xC0, 0x2D
    "Nivel",          // 0xC0, 0x2E
    "Porcentaje",     // 0xC0, 0x2F
    "Velocidad",      // 0xC0, 0x30
    "Tiempo",         // 0xC0, 0x31
    "Hora",           // 0xC0, 0x32
    "Fecha",          // 0xC0, 0x33
    "Historial",      // 0xC0, 0x34
    "Grafica",        // 0xC0, 0x35
    "Tabla",          // 0xC0, 0x36
    "Red Mesh",       // 0xC0, 0x37
    "Enlace",         // 0xC0, 0x38
    "Ruta",           // 0xC0, 0x39
    "Metrica",        // 0xC0, 0x3A
    "IP Local",       // 0xC0, 0x3B
    "Mascara",        // 0xC0, 0x3C
    "Puerta Enlace",  // 0xC0, 0x3D
    "DNS",            // 0xC0, 0x3E
    "Seguridad"       // 0xC0, 0x3F
};

const char* tlv_dict_get_vip(uint8_t code) {
    if (code >= TLV_DICT_VIP_START && code <= TLV_DICT_VIP_END) {
        return s_vip_dict[code - TLV_DICT_VIP_START];
    }
    return NULL;
}

const char* tlv_dict_get_core(uint8_t bank_code, uint8_t index) {
    if (bank_code == TLV_DICT_CORE_START && index < 64) {
        return s_core_bank0[index];
    }
    return NULL;
}

size_t tlv_decode_hybrid_text(const uint8_t* in, size_t in_len, char* out, size_t max_out) {
    if (!in || !out || max_out == 0) return 0;

    size_t in_idx = 0;
    size_t out_idx = 0;

    while (in_idx < in_len && out_idx < max_out - 1) {
        uint8_t b = in[in_idx++];

        // 1. Caracter Crudo ASCII (0x00 - 0x7F): Letras, numeros, signos sin diccionario
        if (b <= TLV_DICT_RAW_MAX) {
            out[out_idx++] = (char)b;
        }
        // 2. Rango VIP (0x80 - 0xBF): 64 Atajos de 1 Byte
        else if (b >= TLV_DICT_VIP_START && b <= TLV_DICT_VIP_END) {
            const char* str = s_vip_dict[b - TLV_DICT_VIP_START];
            if (str) {
                while (*str && out_idx < max_out - 1) {
                    out[out_idx++] = *str++;
                }
            }
        }
        // 3. Rango Core Local (0xC0 - 0xDF): 2 Bytes (Banco + Indice)
        else if (b >= TLV_DICT_CORE_START && b <= TLV_DICT_CORE_END) {
            if (in_idx < in_len) {
                uint8_t idx = in[in_idx++];
                const char* str = tlv_dict_get_core(b, idx);
                if (str) {
                    while (*str && out_idx < max_out - 1) {
                        out[out_idx++] = *str++;
                    }
                } else {
                    // Fallback representacion
                    char tmp[32];
                    int n = snprintf(tmp, sizeof(tmp), "[Token:0x%02X%02X]", b, idx);
                    for (int k = 0; k < n && out_idx < max_out - 1; k++) {
                        out[out_idx++] = tmp[k];
                    }
                }
            }
        }
        // 4. Rango Diccionarios Especializados (0xE0 - 0xFF): 3 Bytes (Selector + ID 16-bit)
        else if (b >= TLV_DICT_SPEC_START) {
            if (in_idx + 1 < in_len) {
                uint8_t hi = in[in_idx++];
                uint8_t lo = in[in_idx++];
                uint16_t param_id = ((uint16_t)hi << 8) | lo;
                
                // Fallback / Expansion de macro-parametro
                char tmp[32];
                int n = snprintf(tmp, sizeof(tmp), "[Dict%02X:%u]", b - TLV_DICT_SPEC_START, param_id);
                for (int k = 0; k < n && out_idx < max_out - 1; k++) {
                    out[out_idx++] = tmp[k];
                }
            }
        }
    }

    out[out_idx] = '\0';
    return out_idx;
}

size_t tlv_encode_hybrid_text(const char* text, uint8_t* out, size_t max_out) {
    if (!text || !out || max_out == 0) return 0;

    size_t in_len = strlen(text);
    size_t in_idx = 0;
    size_t out_idx = 0;

    while (in_idx < in_len && out_idx < max_out) {
        bool match = false;

        // Intentar emparejar tokens VIP de 1 byte
        for (int v = 0; v < TLV_VIP_COUNT; v++) {
            const char* vip = s_vip_dict[v];
            size_t vlen = strlen(vip);
            if (in_idx + vlen <= in_len && strncmp(text + in_idx, vip, vlen) == 0) {
                out[out_idx++] = (uint8_t)(TLV_DICT_VIP_START + v);
                in_idx += vlen;
                match = true;
                break;
            }
        }

        if (!match) {
            // Intentar emparejar banco Core 0 (2 bytes)
            for (int c = 0; c < 64; c++) {
                const char* core = s_core_bank0[c];
                size_t clen = strlen(core);
                if (in_idx + clen <= in_len && strncmp(text + in_idx, core, clen) == 0) {
                    if (out_idx + 1 < max_out) {
                        out[out_idx++] = TLV_DICT_CORE_START;
                        out[out_idx++] = (uint8_t)c;
                        in_idx += clen;
                        match = true;
                        break;
                    }
                }
            }
        }

        // Si no esta en ningun diccionario, se emite en crudo como 1 byte literal
        if (!match) {
            out[out_idx++] = (uint8_t)text[in_idx++];
        }
    }

    return out_idx;
}
