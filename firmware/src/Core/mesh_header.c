#include "mesh_header.h"
#include <string.h>

size_t parse_mesh_header(const uint8_t* buffer, size_t len, MeshHeader* out_hdr) {
    if (!buffer || !out_hdr || len < 1) return 0;
    
    memset(out_hdr, 0, sizeof(MeshHeader));
    size_t offset = 0;
    
    uint8_t ctrl = buffer[offset++];
    out_hdr->control = ctrl;
    
    // Nivel 1: Cabecera ultra-ligera (Misma torre con tabla local: 3 Bytes totales)
    if (ctrl & MESH_CTRL_DST_ONLY) {
        if (offset + 2 > len) return 0;
        out_hdr->dst_id = (buffer[offset] << 8) | buffer[offset + 1];
        out_hdr->is_short_id = true;
        out_hdr->is_dst_only = true;
        return offset + 2; // Total 3 bytes
    }
    
    // Nivel 4: Inter-ASN Global (21 Bytes totales: 1B Control + 10B Origen + 10B Destino)
    if (ctrl & MESH_CTRL_GLOBAL_BIT) {
        if (offset + 20 > len) return 0;
        out_hdr->src_asn   = (buffer[offset] << 8) | buffer[offset + 1];
        out_hdr->src_zone  = (buffer[offset + 2] << 8) | buffer[offset + 3];
        out_hdr->src_tower = (buffer[offset + 4] << 8) | buffer[offset + 5];
        out_hdr->src_id    = ((uint32_t)buffer[offset + 6] << 24) | ((uint32_t)buffer[offset + 7] << 16) |
                             ((uint32_t)buffer[offset + 8] << 8) | (uint32_t)buffer[offset + 9];
        offset += 10;
        
        out_hdr->dst_asn   = (buffer[offset] << 8) | buffer[offset + 1];
        out_hdr->dst_zone  = (buffer[offset + 2] << 8) | buffer[offset + 3];
        out_hdr->dst_tower = (buffer[offset + 4] << 8) | buffer[offset + 5];
        out_hdr->dst_id    = ((uint32_t)buffer[offset + 6] << 24) | ((uint32_t)buffer[offset + 7] << 16) |
                             ((uint32_t)buffer[offset + 8] << 8) | (uint32_t)buffer[offset + 9];
        offset += 10;
        return offset; // Total 21 bytes
    }
    
    // Nivel 2 y 3: Intra-Zona OSPF (Torre Origen 2B + Torre Destino 2B)
    if (ctrl & MESH_CTRL_INTRA_ZONE) {
        if (ctrl & MESH_CTRL_SHORT_ID) {
            // Nivel 2: Intra-Zona con Short IDs (9 Bytes totales: 1B Control + 2B Torre Src + 2B Torre Dst + 2B Short ID Src + 2B Short ID Dst)
            if (offset + 8 > len) return 0;
            out_hdr->src_tower = (buffer[offset] << 8) | buffer[offset + 1];
            out_hdr->dst_tower = (buffer[offset + 2] << 8) | buffer[offset + 3];
            out_hdr->src_id    = (buffer[offset + 4] << 8) | buffer[offset + 5];
            out_hdr->dst_id    = (buffer[offset + 6] << 8) | buffer[offset + 7];
            out_hdr->is_short_id = true;
            offset += 8;
            return offset; // Total 9 bytes
        } else {
            // Nivel 3: Intra-Zona con UUIDs completos (13 Bytes totales: 1B Control + 2B Torre Src + 2B Torre Dst + 4B UUID Src + 4B UUID Dst)
            if (offset + 12 > len) return 0;
            out_hdr->src_tower = (buffer[offset] << 8) | buffer[offset + 1];
            out_hdr->dst_tower = (buffer[offset + 2] << 8) | buffer[offset + 3];
            out_hdr->src_id    = ((uint32_t)buffer[offset + 4] << 24) | ((uint32_t)buffer[offset + 5] << 16) |
                                 ((uint32_t)buffer[offset + 6] << 8) | (uint32_t)buffer[offset + 7];
            out_hdr->dst_id    = ((uint32_t)buffer[offset + 8] << 24) | ((uint32_t)buffer[offset + 9] << 16) |
                                 ((uint32_t)buffer[offset + 10] << 8) | (uint32_t)buffer[offset + 11];
            offset += 12;
            return offset; // Total 13 bytes
        }
    }
    
    // Nivel 2 Estándar Local (9 Bytes totales: 1B Control + 4B UUID Dst + 4B UUID Src)
    if (offset + 8 > len) return 0;
    out_hdr->dst_id = ((uint32_t)buffer[offset] << 24) | ((uint32_t)buffer[offset + 1] << 16) |
                      ((uint32_t)buffer[offset + 2] << 8) | (uint32_t)buffer[offset + 3];
    out_hdr->src_id = ((uint32_t)buffer[offset + 4] << 24) | ((uint32_t)buffer[offset + 5] << 16) |
                      ((uint32_t)buffer[offset + 6] << 8) | (uint32_t)buffer[offset + 7];
    offset += 8;
    return offset; // Total 9 bytes
}

size_t build_mesh_header(uint8_t* buffer, size_t max_len, const MeshHeader* hdr) {
    if (!buffer || !hdr || max_len < 1) return 0;
    
    size_t offset = 0;
    buffer[offset++] = hdr->control;
    
    // Nivel 1: Ultra-ligero (3 Bytes)
    if (hdr->control & MESH_CTRL_DST_ONLY) {
        if (offset + 2 > max_len) return 0;
        buffer[offset++] = (hdr->dst_id >> 8) & 0xFF;
        buffer[offset++] = hdr->dst_id & 0xFF;
        return offset;
    }
    
    // Nivel 4: Inter-ASN Global (21 Bytes)
    if (hdr->control & MESH_CTRL_GLOBAL_BIT) {
        if (offset + 20 > max_len) return 0;
        buffer[offset++] = (hdr->src_asn >> 8) & 0xFF;
        buffer[offset++] = hdr->src_asn & 0xFF;
        buffer[offset++] = (hdr->src_zone >> 8) & 0xFF;
        buffer[offset++] = hdr->src_zone & 0xFF;
        buffer[offset++] = (hdr->src_tower >> 8) & 0xFF;
        buffer[offset++] = hdr->src_tower & 0xFF;
        buffer[offset++] = (hdr->src_id >> 24) & 0xFF;
        buffer[offset++] = (hdr->src_id >> 16) & 0xFF;
        buffer[offset++] = (hdr->src_id >> 8) & 0xFF;
        buffer[offset++] = hdr->src_id & 0xFF;
        
        buffer[offset++] = (hdr->dst_asn >> 8) & 0xFF;
        buffer[offset++] = hdr->dst_asn & 0xFF;
        buffer[offset++] = (hdr->dst_zone >> 8) & 0xFF;
        buffer[offset++] = hdr->dst_zone & 0xFF;
        buffer[offset++] = (hdr->dst_tower >> 8) & 0xFF;
        buffer[offset++] = hdr->dst_tower & 0xFF;
        buffer[offset++] = (hdr->dst_id >> 24) & 0xFF;
        buffer[offset++] = (hdr->dst_id >> 16) & 0xFF;
        buffer[offset++] = (hdr->dst_id >> 8) & 0xFF;
        buffer[offset++] = hdr->dst_id & 0xFF;
        return offset;
    }
    
    // Nivel 2 y 3: Intra-Zona OSPF
    if (hdr->control & MESH_CTRL_INTRA_ZONE) {
        if (hdr->control & MESH_CTRL_SHORT_ID) {
            // Nivel 2: 9 Bytes con Short IDs
            if (offset + 8 > max_len) return 0;
            buffer[offset++] = (hdr->src_tower >> 8) & 0xFF;
            buffer[offset++] = hdr->src_tower & 0xFF;
            buffer[offset++] = (hdr->dst_tower >> 8) & 0xFF;
            buffer[offset++] = hdr->dst_tower & 0xFF;
            buffer[offset++] = (hdr->src_id >> 8) & 0xFF;
            buffer[offset++] = hdr->src_id & 0xFF;
            buffer[offset++] = (hdr->dst_id >> 8) & 0xFF;
            buffer[offset++] = hdr->dst_id & 0xFF;
            return offset;
        } else {
            // Nivel 3: 13 Bytes con UUIDs
            if (offset + 12 > max_len) return 0;
            buffer[offset++] = (hdr->src_tower >> 8) & 0xFF;
            buffer[offset++] = hdr->src_tower & 0xFF;
            buffer[offset++] = (hdr->dst_tower >> 8) & 0xFF;
            buffer[offset++] = hdr->dst_tower & 0xFF;
            buffer[offset++] = (hdr->src_id >> 24) & 0xFF;
            buffer[offset++] = (hdr->src_id >> 16) & 0xFF;
            buffer[offset++] = (hdr->src_id >> 8) & 0xFF;
            buffer[offset++] = hdr->src_id & 0xFF;
            buffer[offset++] = (hdr->dst_id >> 24) & 0xFF;
            buffer[offset++] = (hdr->dst_id >> 16) & 0xFF;
            buffer[offset++] = (hdr->dst_id >> 8) & 0xFF;
            buffer[offset++] = hdr->dst_id & 0xFF;
            return offset;
        }
    }
    
    // Nivel 2 Local Estándar (9 Bytes)
    if (offset + 8 > max_len) return 0;
    buffer[offset++] = (hdr->dst_id >> 24) & 0xFF;
    buffer[offset++] = (hdr->dst_id >> 16) & 0xFF;
    buffer[offset++] = (hdr->dst_id >> 8) & 0xFF;
    buffer[offset++] = hdr->dst_id & 0xFF;
    buffer[offset++] = (hdr->src_id >> 24) & 0xFF;
    buffer[offset++] = (hdr->src_id >> 16) & 0xFF;
    buffer[offset++] = (hdr->src_id >> 8) & 0xFF;
    buffer[offset++] = hdr->src_id & 0xFF;
    return offset;
}
