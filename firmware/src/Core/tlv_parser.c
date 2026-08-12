#include "tlv_parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static tlv_uplink_send_cb_t g_uplink_cb = NULL;

void tlv_browser_set_uplink_cb(tlv_uplink_send_cb_t cb) {
    g_uplink_cb = cb;
}

static void btn_event_cb(lv_event_t * e) {
    int link_id = (int)(intptr_t)lv_event_get_user_data(e);
    if (g_uplink_cb) {
        uint8_t frame[16];
        size_t len = tlv_build_req_click(frame, sizeof(frame), (uint8_t)link_id);
        if (len > 0) {
            g_uplink_cb(frame, len);
        }
    }
}

static void textarea_event_cb(lv_event_t * e) {
    lv_obj_t * ta = lv_event_get_target(e);
    int element_id = (int)(intptr_t)lv_event_get_user_data(e);
    const char * text = lv_textarea_get_text(ta);
    if (g_uplink_cb && text) {
        uint8_t frame[256];
        size_t len = tlv_build_req_submit(frame, sizeof(frame), (uint8_t)element_id, text);
        if (len > 0) {
            g_uplink_cb(frame, len);
        }
    }
}

size_t tlv_build_req_url(uint8_t* buf, size_t max_len, const char* url) {
    if (!buf || !url || max_len < 4) return 0;
    size_t url_len = strlen(url);
    if (3 + url_len > max_len) return 0;

    buf[0] = TYPE_REQ_URL;
    buf[1] = (url_len >> 8) & 0xFF;
    buf[2] = url_len & 0xFF;
    memcpy(buf + 3, url, url_len);
    return 3 + url_len;
}

size_t tlv_build_req_submit(uint8_t* buf, size_t max_len, uint8_t element_id, const char* text) {
    if (!buf || !text || max_len < 5) return 0;
    size_t text_len = strlen(text);
    size_t payload_len = 1 + text_len; // [Element_ID: 1B] [Text: N Bytes]
    if (3 + payload_len > max_len) return 0;

    buf[0] = TYPE_REQ_INPUT_SUBMIT;
    buf[1] = (payload_len >> 8) & 0xFF;
    buf[2] = payload_len & 0xFF;
    buf[3] = element_id;
    memcpy(buf + 4, text, text_len);
    return 3 + payload_len;
}

size_t tlv_build_req_click(uint8_t* buf, size_t max_len, uint8_t link_id) {
    if (!buf || max_len < 4) return 0;
    buf[0] = TYPE_REQ_LINK_CLICK;
    buf[1] = 0x00;
    buf[2] = 0x01; // Payload size: 1 byte
    buf[3] = link_id;
    return 4;
}

#define MAX_DEPTH 32

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
            case TYPE_ABS_PANEL: {
                lv_obj_t* panel = lv_obj_create(current_parent);
                if (node_length >= 8) {
                    uint16_t x = (value[0] << 8) | value[1];
                    uint16_t y = (value[2] << 8) | value[3];
                    uint16_t w = (value[4] << 8) | value[5];
                    uint16_t h = (value[6] << 8) | value[7];
                    lv_obj_set_pos(panel, x, y);
                    lv_obj_set_size(panel, w, h);
                } else {
                    lv_obj_set_size(panel, LV_PCT(100), LV_SIZE_CONTENT);
                }
                if (depth + 1 < MAX_DEPTH) {
                    parent_stack[depth] = current_parent;
                    depth++;
                    current_parent = panel;
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
                if (node_length >= 9) { // [x:2][y:2][w:2][h:2][element_id:1][placeholder...]
                    uint16_t x = (value[0] << 8) | value[1];
                    uint16_t y = (value[2] << 8) | value[3];
                    uint16_t w = (value[4] << 8) | value[5];
                    uint16_t h = (value[6] << 8) | value[7];
                    uint8_t elem_id = value[8];
                    
                    lv_obj_t* ta = lv_textarea_create(current_parent);
                    lv_obj_set_pos(ta, x, y);
                    lv_obj_set_size(ta, w, h);
                    lv_textarea_set_one_line(ta, true);
                    
                    lv_obj_add_event_cb(ta, textarea_event_cb, LV_EVENT_READY, (void*)(intptr_t)elem_id);
                    
                    size_t str_len = node_length - 9;
                    if (str_len > 0) {
                        char* txt = (char*)malloc(str_len + 1);
                        if (txt) {
                            memcpy(txt, value + 9, str_len);
                            txt[str_len] = '\0';
                            lv_textarea_set_placeholder_text(ta, txt);
                            free(txt);
                        }
                    }
                }
                break;
            }
            case TYPE_ABS_CHECKBOX: {
                if (node_length >= 2) { // [id:1][state:1][text...]
                    uint8_t state = value[1];
                    lv_obj_t* cb = lv_checkbox_create(current_parent);
                    if (state) {
                        lv_obj_add_state(cb, LV_STATE_CHECKED);
                    }
                    if (node_length > 2) {
                        size_t str_len = node_length - 2;
                        char* txt = (char*)malloc(str_len + 1);
                        if (txt) {
                            memcpy(txt, value + 2, str_len);
                            txt[str_len] = '\0';
                            lv_checkbox_set_text(cb, txt);
                            free(txt);
                        }
                    }
                }
                break;
            }
            case TYPE_ABS_SWITCH: {
                if (node_length >= 2) { // [id:1][state:1]
                    uint8_t state = value[1];
                    lv_obj_t* sw = lv_switch_create(current_parent);
                    if (state) {
                        lv_obj_add_state(sw, LV_STATE_CHECKED);
                    }
                }
                break;
            }
            case TYPE_ABS_SLIDER: {
                if (node_length >= 7) { // [id:1][min:2][max:2][val:2]
                    int16_t min_v = (value[1] << 8) | value[2];
                    int16_t max_v = (value[3] << 8) | value[4];
                    int16_t cur_v = (value[5] << 8) | value[6];
                    lv_obj_t* slider = lv_slider_create(current_parent);
                    lv_slider_set_range(slider, min_v, max_v);
                    lv_slider_set_value(slider, cur_v, LV_ANIM_OFF);
                }
                break;
            }
            case TYPE_ABS_PROGRESS: {
                if (node_length >= 6) { // [min:2][max:2][val:2]
                    int16_t min_v = (value[0] << 8) | value[1];
                    int16_t max_v = (value[2] << 8) | value[3];
                    int16_t cur_v = (value[4] << 8) | value[5];
                    lv_obj_t* bar = lv_bar_create(current_parent);
                    lv_bar_set_range(bar, min_v, max_v);
                    lv_bar_set_value(bar, cur_v, LV_ANIM_OFF);
                }
                break;
            }
            case TYPE_ABS_DROPDOWN: {
                if (node_length >= 2) { // [id:1][options string with \n]
                    lv_obj_t* dd = lv_dropdown_create(current_parent);
                    size_t str_len = node_length - 1;
                    char* txt = (char*)malloc(str_len + 1);
                    if (txt) {
                        memcpy(txt, value + 1, str_len);
                        txt[str_len] = '\0';
                        lv_dropdown_set_options(dd, txt);
                        free(txt);
                    }
                }
                break;
            }
            case TYPE_ABS_CHART: {
                if (node_length >= 11) { // [x:2][y:2][w:2][h:2][chart_type:1][num_pts:2] + [p0:2...]
                    uint16_t x = (value[0] << 8) | value[1];
                    uint16_t y = (value[2] << 8) | value[3];
                    uint16_t w = (value[4] << 8) | value[5];
                    uint16_t h = (value[6] << 8) | value[7];
                    uint8_t chart_type = value[8];
                    uint16_t num_pts = (value[9] << 8) | value[10];

                    lv_obj_t* chart = lv_chart_create(current_parent);
                    lv_obj_set_pos(chart, x, y);
                    lv_obj_set_size(chart, w, h);
                    if (chart_type == 1) {
                        lv_chart_set_type(chart, LV_CHART_TYPE_BAR);
                    } else {
                        lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
                    }
                    lv_chart_set_point_count(chart, num_pts);

                    lv_chart_series_t* ser = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);

                    size_t pt_offset = 11;
                    for (uint16_t i = 0; i < num_pts && pt_offset + 2 <= node_length; i++) {
                        int16_t val = (value[pt_offset] << 8) | value[pt_offset + 1];
                        lv_chart_set_next_value(chart, ser, val);
                        pt_offset += 2;
                    }
                    lv_chart_refresh(chart);
                }
                break;
            }
            default:
                break;
        }
    }
    
    return root_parent;
}
