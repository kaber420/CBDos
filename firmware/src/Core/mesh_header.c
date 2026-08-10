#include "mesh_header.h"
#include <string.h>

size_t parse_mesh_header(const uint8_t* buffer, size_t len, MeshHeader* out_hdr) {
    if (!buffer || !out_hdr || len < 1) return 0;
    
    memset(out_hdr, 0, sizeof(MeshHeader));
    size_t offset = 0;
    
    uint8_t ctrl = buffer[offset++];
    out_hdr->control = ctrl;
    
    // Check Bit 7: Pseudo-BGP (Inter-ASN)
    if (ctrl & MESH_CTRL_GLOBAL_BIT) {
        if (offset + 4 > len) return 0;
        out_hdr->src_asn = (buffer[offset] << 8) | buffer[offset + 1];
        out_hdr->dst_asn = (buffer[offset + 2] << 8) | buffer[offset + 3];
        offset += 4;
    }
    
    // Check Bit 5: Pseudo-OSPF Inter-Zone
    if (ctrl & MESH_CTRL_INTRA_ZONE) {
        if (offset + 4 > len) return 0;
        out_hdr->src_zone = (buffer[offset] << 8) | buffer[offset + 1];
        out_hdr->dst_zone = (buffer[offset + 2] << 8) | buffer[offset + 3];
        offset += 4;
    }
    
    // Check Bit 4: Pseudo-OSPF Inter-Tower
    if (ctrl & MESH_CTRL_INTRA_TOWER) {
        if (offset + 4 > len) return 0;
        out_hdr->src_tower = (buffer[offset] << 8) | buffer[offset + 1];
        out_hdr->dst_tower = (buffer[offset + 2] << 8) | buffer[offset + 3];
        offset += 4;
    }
    
    // Check Bit 3: Short ID vs UUID
    if (ctrl & MESH_CTRL_SHORT_ID) {
        if (offset + 4 > len) return 0;
        out_hdr->src_id = (buffer[offset] << 8) | buffer[offset + 1];
        out_hdr->dst_id = (buffer[offset + 2] << 8) | buffer[offset + 3];
        out_hdr->is_short_id = true;
        offset += 4;
    } else {
        if (offset + 8 > len) return 0;
        out_hdr->src_id = ((uint32_t)buffer[offset] << 24) | ((uint32_t)buffer[offset + 1] << 16) |
                          ((uint32_t)buffer[offset + 2] << 8) | (uint32_t)buffer[offset + 3];
        out_hdr->dst_id = ((uint32_t)buffer[offset + 4] << 24) | ((uint32_t)buffer[offset + 5] << 16) |
                          ((uint32_t)buffer[offset + 6] << 8) | (uint32_t)buffer[offset + 7];
        out_hdr->is_short_id = false;
        offset += 8;
    }
    
    return offset;
}

size_t build_mesh_header(uint8_t* buffer, size_t max_len, const MeshHeader* hdr) {
    if (!buffer || !hdr || max_len < 1) return 0;
    
    size_t offset = 0;
    buffer[offset++] = hdr->control;
    
    if (hdr->control & MESH_CTRL_GLOBAL_BIT) {
        if (offset + 4 > max_len) return 0;
        buffer[offset++] = (hdr->src_asn >> 8) & 0xFF;
        buffer[offset++] = hdr->src_asn & 0xFF;
        buffer[offset++] = (hdr->dst_asn >> 8) & 0xFF;
        buffer[offset++] = hdr->dst_asn & 0xFF;
    }
    
    if (hdr->control & MESH_CTRL_INTRA_ZONE) {
        if (offset + 4 > max_len) return 0;
        buffer[offset++] = (hdr->src_zone >> 8) & 0xFF;
        buffer[offset++] = hdr->src_zone & 0xFF;
        buffer[offset++] = (hdr->dst_zone >> 8) & 0xFF;
        buffer[offset++] = hdr->dst_zone & 0xFF;
    }
    
    if (hdr->control & MESH_CTRL_INTRA_TOWER) {
        if (offset + 4 > max_len) return 0;
        buffer[offset++] = (hdr->src_tower >> 8) & 0xFF;
        buffer[offset++] = hdr->src_tower & 0xFF;
        buffer[offset++] = (hdr->dst_tower >> 8) & 0xFF;
        buffer[offset++] = hdr->dst_tower & 0xFF;
    }
    
    if (hdr->control & MESH_CTRL_SHORT_ID) {
        if (offset + 4 > max_len) return 0;
        buffer[offset++] = (hdr->src_id >> 8) & 0xFF;
        buffer[offset++] = hdr->src_id & 0xFF;
        buffer[offset++] = (hdr->dst_id >> 8) & 0xFF;
        buffer[offset++] = hdr->dst_id & 0xFF;
    } else {
        if (offset + 8 > max_len) return 0;
        buffer[offset++] = (hdr->src_id >> 24) & 0xFF;
        buffer[offset++] = (hdr->src_id >> 16) & 0xFF;
        buffer[offset++] = (hdr->src_id >> 8) & 0xFF;
        buffer[offset++] = hdr->src_id & 0xFF;
        buffer[offset++] = (hdr->dst_id >> 24) & 0xFF;
        buffer[offset++] = (hdr->dst_id >> 16) & 0xFF;
        buffer[offset++] = (hdr->dst_id >> 8) & 0xFF;
        buffer[offset++] = hdr->dst_id & 0xFF;
    }
    
    return offset;
}
