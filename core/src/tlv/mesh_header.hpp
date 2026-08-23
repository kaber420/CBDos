#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Service IDs (bits 4..0 of Control byte)
#define MESH_SVC_CHAT          0x01
#define MESH_SVC_PROXY         0x05
#define MESH_SVC_TLVGL_REQUEST 0x07
#define MESH_SVC_TLVGL_RESPONSE 0x08

// Flags in Control byte
#define MESH_CTRL_GLOBAL_BIT   (1 << 7) // 1 = Global Inter-ASN (21B header), 0 = Local/Intra-ASN
#define MESH_CTRL_SIGNAL_BIT   (1 << 6) // 1 = Signaling packet, 0 = Data
#define MESH_CTRL_INTRA_ZONE   (1 << 5) // 1 = Inter-Zone/Torre OSPF present (13B header)
#define MESH_CTRL_SHORT_ID     (1 << 4) // 1 = Short ID (2B), 0 = Full UUID (4B)
#define MESH_CTRL_DST_ONLY     (1 << 3) // 1 = Send only Dst Short ID (3B ultra-light header)

typedef struct {
    uint8_t  control;      // Control byte (Flags + Service ID)
    
    // Pseudo-BGP fields (Present if MESH_CTRL_GLOBAL_BIT set -> 2B src + 2B dst)
    uint16_t src_asn;
    uint16_t dst_asn;
    
    // Pseudo-OSPF fields (Present if MESH_CTRL_INTRA_ZONE set -> 2B Zone + 2B Torre)
    uint16_t src_zone;
    uint16_t dst_zone;
    uint16_t src_tower;
    uint16_t dst_tower;
    
    // Node Identifiers
    uint32_t src_id;       // 4B UUID or 2B Short ID
    uint32_t dst_id;       // 4B UUID or 2B Short ID
    
    bool is_short_id;
    bool is_dst_only;      // True if ultra-light 3-byte local header
} MeshHeader;

/**
 * @brief Decodifica la cabecera binaria de malla según la especificación RFC
 * @return Tamaño de la cabecera consumida (3B, 9B, 13B o 21B), o 0 si es inválido
 */
size_t parse_mesh_header(const uint8_t* buffer, size_t len, MeshHeader* out_hdr);

/**
 * @brief Construye la cabecera binaria de malla (3B, 9B, 13B o 21B)
 */
size_t build_mesh_header(uint8_t* buffer, size_t max_len, const MeshHeader* hdr);

#ifdef __cplusplus
}
#endif
