#ifndef MESH_HEADER_H
#define MESH_HEADER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Service IDs (bits 5..0 of Control byte)
#define MESH_SVC_CHAT          0x01
#define MESH_SVC_PROXY         0x05
#define MESH_SVC_TLVGL_REQUEST 0x07
#define MESH_SVC_TLVGL_RESPONSE 0x08

// Flags in Control byte
#define MESH_CTRL_GLOBAL_BIT   (1 << 7) // 1 = Inter-ASN (Pseudo-BGP header present)
#define MESH_CTRL_SIGNAL_BIT   (1 << 6) // 1 = Signaling packet, 0 = Normal data
#define MESH_CTRL_INTRA_ZONE   (1 << 5) // 1 = Different Zone (Pseudo-OSPF Zone header present)
#define MESH_CTRL_INTRA_TOWER  (1 << 4) // 1 = Different Tower (Pseudo-OSPF Tower header present)
#define MESH_CTRL_SHORT_ID     (1 << 3) // 1 = 2-byte Short ID used, 0 = 4-byte UUID used

typedef struct {
    uint8_t  control;      // Control byte (Flags + Service ID)
    
    // Pseudo-BGP fields (Present if MESH_CTRL_GLOBAL_BIT is set)
    uint16_t src_asn;
    uint16_t dst_asn;
    
    // Pseudo-OSPF fields
    uint16_t src_zone;     // Present if MESH_CTRL_INTRA_ZONE is set
    uint16_t dst_zone;
    uint16_t src_tower;    // Present if MESH_CTRL_INTRA_TOWER is set
    uint16_t dst_tower;
    
    // Node Identifiers
    uint32_t src_id;       // 4-byte UUID or 2-byte Short ID (padded to uint32_t)
    uint32_t dst_id;
    
    bool is_short_id;
} MeshHeader;

/**
 * @brief Decodifica una cabecera binaria de malla desde un buffer
 * @param buffer Buffer de datos recibidos por la radio
 * @param len Longitud total del buffer
 * @param out_hdr Puntero a la estructura MeshHeader que se rellenará
 * @return Número de bytes consumidos por la cabecera (offset al payload), o 0 si el buffer es inválido
 */
size_t parse_mesh_header(const uint8_t* buffer, size_t len, MeshHeader* out_hdr);

/**
 * @brief Emite una cabecera binaria de malla hacia un buffer
 * @param buffer Buffer de salida
 * @param max_len Capacidad máxima del buffer
 * @param hdr Puntero a la estructura MeshHeader con los campos configurados
 * @return Número de bytes escritos en la cabecera, o 0 si falló
 */
size_t build_mesh_header(uint8_t* buffer, size_t max_len, const MeshHeader* hdr);

#ifdef __cplusplus
}
#endif

#endif // MESH_HEADER_H
