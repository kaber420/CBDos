#include "py/runtime.h"
#include "py/obj.h"
#include "lvgl.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#define TYPE_ABS_PAGE      0x10
#define TYPE_ABS_TEXT      0x11
#define TYPE_ABS_LINK      0x12
#define TYPE_ABS_INPUT     0x13
#define TYPE_ABS_IMAGE     0x14
#define TYPE_END           0xFE

// MAX nesting depth
#define MAX_DEPTH 32

// We will use a dummy event cb for buttons created from C for now, 
// ideally this should send an event back to python or handle it directly
static void btn_event_cb(lv_event_t * e) {
    // int link_id = (int)lv_event_get_user_data(e);
    // printf("Link clicked ID: %d\n", link_id);
}

typedef struct {
    mp_obj_base_t base;
    void *data;
} mp_lv_struct_t;

static mp_obj_t tlv_browser_render(mp_obj_t parent_obj, mp_obj_t tlv_bytes_obj) {
    mp_lv_struct_t *mp_lv = (mp_lv_struct_t *)MP_OBJ_TO_PTR(parent_obj);
    lv_obj_t* root_parent = (lv_obj_t*)mp_lv->data;

    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(tlv_bytes_obj, &bufinfo, MP_BUFFER_READ);
    
    uint8_t* data = (uint8_t*)bufinfo.buf;
    size_t length = bufinfo.len;

    size_t offset = 0;
    
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
                    char* txt = (char*)m_malloc(text_len + 1);
                    if (txt) {
                        memcpy(txt, value + 9, text_len);
                        txt[text_len] = '\0';
                        lv_label_set_text(label, txt);
                        m_free(txt);
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
                    char* txt = (char*)m_malloc(text_len + 1);
                    if (txt) {
                        memcpy(txt, value + 9, text_len);
                        txt[text_len] = '\0';
                        lv_label_set_text(label, txt);
                        m_free(txt);
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
                        char* txt = (char*)m_malloc(str_len + 1);
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
                            m_free(txt);
                        }
                    }
                }
                break;
            }
            default:
                break;
        }
    }
    
    return mp_const_none;
}

static MP_DEFINE_CONST_FUN_OBJ_2(tlv_browser_render_obj, tlv_browser_render);

static const mp_rom_map_elem_t tlv_browser_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_tlv_browser) },
    { MP_ROM_QSTR(MP_QSTR_render), MP_ROM_PTR(&tlv_browser_render_obj) },
};
static MP_DEFINE_CONST_DICT(tlv_browser_globals, tlv_browser_globals_table);

const mp_obj_module_t tlv_browser_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&tlv_browser_globals,
};

MP_REGISTER_MODULE(MP_QSTR_tlv_browser, tlv_browser_user_cmodule);
