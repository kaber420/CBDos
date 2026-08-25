#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Rangos de Prefijos de Bits
#define TLV_DICT_RAW_MAX       0x7F // 0x00..0x7F: Caracteres crudos ASCII (1 Byte)
#define TLV_DICT_VIP_START     0x80 // 0x80..0xBF: Rango VIP Ultra-Corto (64 valores, 1 Byte)
#define TLV_DICT_VIP_END       0xBF
#define TLV_DICT_CORE_START    0xC0 // 0xC0..0xDF: Rango Core Local (32 bloques x 256, 2 Bytes)
#define TLV_DICT_CORE_END      0xDF
#define TLV_DICT_SPEC_START    0xE0 // 0xE0..0xFF: Rango Diccionarios Especializados (32 x 65,536, 3 Bytes)
#define TLV_DICT_SPEC_END      0xFF

#define TLV_VIP_COUNT          64
#define TLV_CORE_BANKS         32
#define TLV_CORE_BANK_SIZE     256
#define TLV_SPEC_DICT_COUNT    32

/**
 * @brief Decodifica un flujo binario comprimido con diccionarios híbridos a texto legible UTF-8.
 *        Si un byte es < 0x80 se emite directamente como caracter crudo ASCII.
 *        Si es 0x80..0xBF expande el token VIP de 1 byte.
 *        Si es 0xC0..0xDF lee 2 bytes y expande el token Core.
 *        Si es 0xE0..0xFF lee 3 bytes y expande la entrada de diccionario especializado.
 * 
 * @param in Puntero al buffer comprimido
 * @param in_len Longitud del buffer de entrada
 * @param out Puntero al buffer de salida de texto
 * @param max_out Tamaño máximo del buffer de salida
 * @return Número de bytes escritos en `out` (sin incluir '\0')
 */
size_t tlv_decode_hybrid_text(const uint8_t* in, size_t in_len, char* out, size_t max_out);

/**
 * @brief Codifica un texto plano a formato comprimido con tokens VIP (1B) y caracteres crudos (1B).
 * 
 * @param text Texto de entrada terminado en nulo
 * @param out Buffer de salida binario
 * @param max_out Tamaño máximo del buffer de salida
 * @return Número de bytes generados
 */
size_t tlv_encode_hybrid_text(const char* text, uint8_t* out, size_t max_out);

/**
 * @brief Obtiene el string correspondiente a un token VIP de 1 byte (0x80..0xBF)
 */
const char* tlv_dict_get_vip(uint8_t code);

/**
 * @brief Obtiene el string correspondiente a un token Core de 2 bytes (0xC0..0xDF, index)
 */
const char* tlv_dict_get_core(uint8_t bank_code, uint8_t index);

#ifdef __cplusplus
}
#endif
