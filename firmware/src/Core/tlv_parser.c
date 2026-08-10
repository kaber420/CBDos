#include "tlv_parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define TYPE_ABS_PAGE      0x10
#define TYPE_ABS_TEXT      0x11
#define TYPE_ABS_LINK      0x12
#define TYPE_ABS_INPUT     0x13
#define TYPE_ABS_IMAGE     0x14
#define TYPE_END           0xFE

#define MAX_DEPTH 32

static void btn_event_cb(lv_event_t * e) {
    int link_id = (int)(intptr_t)lv_event_get_user_data(e);
    (void)link_id;
}

lv_obj_t* tlv_browser_render(lv_obj_t* root_parent, const uint8_t* data, size_t length) {
    if (!root_parent || !data || length == 0) return NULL;

    size_t offset = 0;
    
    // Magic Number 'PH' (0x50 0x48) check
    if (length >= 2 && data[0] == 0x50 && data[1] == 0x48) {
        offset = 2;
    }

    lv_obj_t* parent_stack[MAX_DEPTH];
    int depth = 0;
    lv_obj_t* current_parent = root_parent; 
    
    while (offset < length) {
        if (offset + 1 > length) break;
        
        uint8_t node_type = data[offset++];
        
        if (node_type == TYPE_END) {
            if (depth > 0) {
                depth--;
                current_parent = parent_stack[depth];
            }
            continue;
        }
        
        if (offset + 2 > length) break;
        uint16_t node_length = (data[offset] << 8) | data[offset+1];
        offset += 2;
        
        if (offset + node_length > length) break; // Safety check
        
        const uint8_t* value = &data[offset];
        offset += node_length;
        
        switch (node_type) {
            case TYPE_ABS_PAGE: {
                lv_obj_t* page = lv_obj_create(current_parent);
                lv_obj_set_size(page, LV_PCT(100), LV_PCT(100));
                lv_obj_set_flex_flow(page, LV_FLEX_FLOW_COLUMN);
                if (depth + 1 < MAX_DEPTH) {
                    parent_stack[depth] = current_parent;
                    depth++;
                    current_parent = page;
                }
                break;
            }
            case TYPE_ABS_TEXT: {
                if (node_length >= 9) {
                    uint16_t x = (value[0] << 8) | value[1];
                    uint16_t y = (value[2] << 8) | value[3];
                    uint16_t w = (value[4] << 8) | value[5];
                    uint16_t h = (value[6] << 8) | value[7];
                    (void)h;
                    uint8_t style_id = value[8];
                    
                    lv_obj_t* label = lv_label_create(current_parent);
                    lv_obj_set_pos(label, x, y);
                    lv_obj_set_width(label, w);
                    
                    size_t text_len = node_length - 9;
                    char* txt = (char*)malloc(text_len + 1);
                    if (txt) {
                        memcpy(txt, value + 9, text_len);
                        txt[text_len] = '\0';
                        lv_label_set_text(label, txt);
                        free(txt);
                    }
                    
                    if (style_id & 1) { // Bold
                        lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0); 
                    } else {
                        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
                    }
                }
                break;
            }
            case TYPE_ABS_LINK: {
                if (node_length >= 9) {
                    uint16_t x = (value[0] << 8) | value[1];
                    uint16_t y = (value[2] << 8) | value[3];
                    uint16_t w = (value[4] << 8) | value[5];
                    uint16_t h = (value[6] << 8) | value[7];
                    uint8_t link_id = value[8];
                    
                    lv_obj_t* btn = lv_button_create(current_parent);
                    lv_obj_set_pos(btn, x, y);
                    lv_obj_set_size(btn, w, h);
                    
                    lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)link_id);
                    
                    lv_obj_t* label = lv_label_create(btn);
                    lv_obj_center(label);
                    size_t text_len = node_length - 9;
                    char* txt = (char*)malloc(text_len + 1);
                    if (txt) {
                        memcpy(txt, value + 9, text_len);
                        txt[text_len] = '\0';
                        lv_label_set_text(label, txt);
                        free(txt);
                    }
                }
                break;
            }
            case TYPE_ABS_INPUT: {
                if (node_length >= 8) {
                    uint16_t x = (value[0] << 8) | value[1];
                    uint16_t y = (value[2] << 8) | value[3];
                    uint16_t w = (value[4] << 8) | value[5];
                    uint16_t h = (value[6] << 8) | value[7];
                    
                    lv_obj_t* ta = lv_textarea_create(current_parent);
                    lv_obj_set_pos(ta, x, y);
                    lv_obj_set_size(ta, w, h);
                    lv_textarea_set_one_line(ta, true);
                    
                    size_t str_len = node_length - 8;
                    if (str_len > 0) {
                        char* txt = (char*)malloc(str_len + 1);
                        if (txt) {
                            memcpy(txt, value + 8, str_len);
                            txt[str_len] = '\0';
                            
                            char* param = txt;
                            while (*param && param < txt + str_len) param++;
                            if (param < txt + str_len) param++;
                            
                            char* placeholder = param;
                            while (*placeholder && placeholder < txt + str_len) placeholder++;
                            if (placeholder < txt + str_len) placeholder++;
                            
                            if (placeholder < txt + str_len) {
                                lv_textarea_set_placeholder_text(ta, placeholder);
                            }
                            free(txt);
                        }
                    }
                }
                break;
            }
            default:
                break;
        }
    }
    
    return root_parent;
}
