#include "tlv_parser.hpp"
#include "tlv_dictionary.hpp"
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
    lv_obj_t * ta = (lv_obj_t *)lv_event_get_target(e);
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

static void slider_event_cb(lv_event_t * e) {
    lv_obj_t * slider = (lv_obj_t *)lv_event_get_target(e);
    int elem_id = (int)(intptr_t)lv_event_get_user_data(e);
    int32_t val = lv_slider_get_value(slider);
    if (g_uplink_cb) {
        uint8_t frame[16];
        size_t len = tlv_build_req_control(frame, sizeof(frame), (uint8_t)elem_id, (int16_t)val);
        if (len > 0) {
            g_uplink_cb(frame, len);
        }
    }
}

static void switch_event_cb(lv_event_t * e) {
    lv_obj_t * sw = (lv_obj_t *)lv_event_get_target(e);
    int elem_id = (int)(intptr_t)lv_event_get_user_data(e);
    bool state = lv_obj_has_state(sw, LV_STATE_CHECKED);
    if (g_uplink_cb) {
        uint8_t frame[16];
        size_t len = tlv_build_req_control(frame, sizeof(frame), (uint8_t)elem_id, state ? 1 : 0);
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
    buf[2] = 0x01; // Longitud 1 Byte
    buf[3] = link_id;
    return 4;
}

size_t tlv_build_req_control(uint8_t* buf, size_t max_len, uint8_t elem_id, int16_t value) {
    if (!buf || max_len < 6) return 0;
    buf[0] = TYPE_REQ_CONTROL_EVT;
    buf[1] = 0x00;
    buf[2] = 0x03; // Longitud 3 Bytes [elem_id: 1B] [val: 2B]
    buf[3] = elem_id;
    buf[4] = (value >> 8) & 0xFF;
    buf[5] = value & 0xFF;
    return 6;
}

lv_obj_t* tlv_browser_render(lv_obj_t* root_parent, const uint8_t* data, size_t length) {
    if (!root_parent || !data || length < 2) return NULL;

    size_t offset = 0;

    // Verificar número mágico si existe ('P', 'H' -> 0x50, 0x48)
    if (data[0] == 0x50 && data[1] == 0x48) {
        offset += 2;
    }

    lv_obj_t* current_parent = root_parent;

    while (offset < length) {
        uint8_t type = data[offset++];
        if (type == TYPE_END) {
            break;
        }

        if (offset + 2 > length) break;
        uint16_t node_length = (data[offset] << 8) | data[offset + 1];
        offset += 2;

        if (offset + node_length > length) break;
        const uint8_t* value = data + offset;
        offset += node_length;

        switch (type) {
            case TYPE_ABS_PAGE: {
                lv_obj_clean(root_parent);
                current_parent = root_parent;
                break;
            }
            case TYPE_ABS_PANEL: {
                if (node_length >= 8) { // [x:2][y:2][w:2][h:2] + opcional [bg_color:2 RGB565]
                    uint16_t x = (value[0] << 8) | value[1];
                    uint16_t y = (value[2] << 8) | value[3];
                    uint16_t w = (value[4] << 8) | value[5];
                    uint16_t h = (value[6] << 8) | value[7];
                    
                    lv_obj_t* panel = lv_obj_create(current_parent);
                    lv_obj_set_pos(panel, x, y);
                    lv_obj_set_size(panel, w, h);
                    lv_obj_set_style_radius(panel, 8, 0);
                    lv_obj_set_style_border_width(panel, 1, 0);
                    lv_obj_set_style_border_color(panel, lv_palette_main(LV_PALETTE_GREY), 0);
                    lv_obj_set_style_pad_all(panel, 6, 0);

                    if (node_length >= 10) {
                        uint16_t c565 = (value[8] << 8) | value[9];
                        uint8_t r = (c565 >> 11) & 0x1F;
                        uint8_t g = (c565 >> 5) & 0x3F;
                        uint8_t b = c565 & 0x1F;
                        uint32_t rgb888 = ((uint32_t)(r * 255 / 31) << 16) | ((uint32_t)(g * 255 / 63) << 8) | (b * 255 / 31);
                        lv_obj_set_style_bg_color(panel, lv_color_hex(rgb888), 0);
                        lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
                    } else {
                        lv_obj_set_style_bg_opa(panel, LV_OPA_20, 0);
                    }
                }
                break;
            }
            case TYPE_ABS_TEXT: {
                if (node_length >= 9) { // [x:2][y:2][w:2][h:2][style:1][texto comprimido/crudo...]
                    uint16_t x = (value[0] << 8) | value[1];
                    uint16_t y = (value[2] << 8) | value[3];
                    uint16_t w = (value[4] << 8) | value[5];
                    uint16_t h = (value[6] << 8) | value[7];
                    uint8_t style_id = value[8];
                    
                    lv_obj_t* label = lv_label_create(current_parent);
                    lv_obj_set_pos(label, x, y);
                    if (w > 0) lv_obj_set_width(label, w);
                    if (h > 0) lv_obj_set_height(label, h);
                    
                    size_t raw_len = node_length - 9;
                    char decoded_text[512];
                    tlv_decode_hybrid_text(value + 9, raw_len, decoded_text, sizeof(decoded_text));
                    lv_label_set_text(label, decoded_text);
                    
                    if (style_id == 1) { // H1 / Titulo
                        lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0); 
                        lv_obj_set_style_text_color(label, lv_palette_main(LV_PALETTE_CYAN), 0);
                    } else if (style_id == 2) { // H2 / Subtitulo
                        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
                        lv_obj_set_style_text_color(label, lv_palette_main(LV_PALETTE_AMBER), 0);
                    } else {
                        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
                    }
                }
                break;
            }
            case TYPE_ABS_LINK: {
                if (node_length >= 9) { // [x:2][y:2][w:2][h:2][link_id:1][texto comprimido/crudo...]
                    uint16_t x = (value[0] << 8) | value[1];
                    uint16_t y = (value[2] << 8) | value[3];
                    uint16_t w = (value[4] << 8) | value[5];
                    uint16_t h = (value[6] << 8) | value[7];
                    uint8_t link_id = value[8];
                    
                    lv_obj_t* btn = lv_button_create(current_parent);
                    lv_obj_set_pos(btn, x, y);
                    lv_obj_set_size(btn, w, h);
                    lv_obj_set_style_radius(btn, 6, 0);
                    
                    lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)link_id);
                    
                    lv_obj_t* label = lv_label_create(btn);
                    lv_obj_center(label);
                    
                    size_t raw_len = node_length - 9;
                    char decoded_text[256];
                    tlv_decode_hybrid_text(value + 9, raw_len, decoded_text, sizeof(decoded_text));
                    lv_label_set_text(label, decoded_text);
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
                    
                    size_t raw_len = node_length - 9;
                    if (raw_len > 0) {
                        char decoded_ph[128];
                        tlv_decode_hybrid_text(value + 9, raw_len, decoded_ph, sizeof(decoded_ph));
                        lv_textarea_set_placeholder_text(ta, decoded_ph);
                    }
                }
                break;
            }
            case TYPE_ABS_CHECKBOX: {
                if (node_length >= 10) { // [x:2][y:2][w:2][h:2][id:1][state:1][texto...]
                    uint16_t x = (value[0] << 8) | value[1];
                    uint16_t y = (value[2] << 8) | value[3];
                    uint8_t elem_id = value[8];
                    uint8_t state = value[9];
                    
                    lv_obj_t* cb = lv_checkbox_create(current_parent);
                    lv_obj_set_pos(cb, x, y);
                    if (state) {
                        lv_obj_add_state(cb, LV_STATE_CHECKED);
                    }
                    if (node_length > 10) {
                        char decoded_text[128];
                        tlv_decode_hybrid_text(value + 10, node_length - 10, decoded_text, sizeof(decoded_text));
                        lv_checkbox_set_text(cb, decoded_text);
                    }
                    lv_obj_add_event_cb(cb, switch_event_cb, LV_EVENT_VALUE_CHANGED, (void*)(intptr_t)elem_id);
                } else if (node_length >= 2) { // Fallback legacy
                    uint8_t state = value[1];
                    lv_obj_t* cb = lv_checkbox_create(current_parent);
                    if (state) lv_obj_add_state(cb, LV_STATE_CHECKED);
                    if (node_length > 2) {
                        char decoded_text[128];
                        tlv_decode_hybrid_text(value + 2, node_length - 2, decoded_text, sizeof(decoded_text));
                        lv_checkbox_set_text(cb, decoded_text);
                    }
                }
                break;
            }
            case TYPE_ABS_SWITCH: {
                if (node_length >= 10) { // [x:2][y:2][w:2][h:2][id:1][state:1]
                    uint16_t x = (value[0] << 8) | value[1];
                    uint16_t y = (value[2] << 8) | value[3];
                    uint8_t elem_id = value[8];
                    uint8_t state = value[9];
                    
                    lv_obj_t* sw = lv_switch_create(current_parent);
                    lv_obj_set_pos(sw, x, y);
                    if (state) {
                        lv_obj_add_state(sw, LV_STATE_CHECKED);
                    }
                    lv_obj_add_event_cb(sw, switch_event_cb, LV_EVENT_VALUE_CHANGED, (void*)(intptr_t)elem_id);
                } else if (node_length >= 2) {
                    uint8_t elem_id = value[0];
                    uint8_t state = value[1];
                    lv_obj_t* sw = lv_switch_create(current_parent);
                    if (state) lv_obj_add_state(sw, LV_STATE_CHECKED);
                    lv_obj_add_event_cb(sw, switch_event_cb, LV_EVENT_VALUE_CHANGED, (void*)(intptr_t)elem_id);
                }
                break;
            }
            case TYPE_ABS_SLIDER: {
                if (node_length >= 15) { // [x:2][y:2][w:2][h:2][id:1][min:2][max:2][val:2]
                    uint16_t x = (value[0] << 8) | value[1];
                    uint16_t y = (value[2] << 8) | value[3];
                    uint16_t w = (value[4] << 8) | value[5];
                    uint16_t h = (value[6] << 8) | value[7];
                    uint8_t elem_id = value[8];
                    int16_t min_v = (value[9] << 8) | value[10];
                    int16_t max_v = (value[11] << 8) | value[12];
                    int16_t cur_v = (value[13] << 8) | value[14];
                    
                    lv_obj_t* slider = lv_slider_create(current_parent);
                    lv_obj_set_pos(slider, x, y);
                    lv_obj_set_size(slider, w, h);
                    lv_slider_set_range(slider, min_v, max_v);
                    lv_slider_set_value(slider, cur_v, LV_ANIM_OFF);
                    lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, (void*)(intptr_t)elem_id);
                } else if (node_length >= 7) {
                    uint8_t elem_id = value[0];
                    int16_t min_v = (value[1] << 8) | value[2];
                    int16_t max_v = (value[3] << 8) | value[4];
                    int16_t cur_v = (value[5] << 8) | value[6];
                    lv_obj_t* slider = lv_slider_create(current_parent);
                    lv_slider_set_range(slider, min_v, max_v);
                    lv_slider_set_value(slider, cur_v, LV_ANIM_OFF);
                    lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, (void*)(intptr_t)elem_id);
                }
                break;
            }
            case TYPE_ABS_PROGRESS: {
                if (node_length >= 14) { // [x:2][y:2][w:2][h:2][min:2][max:2][val:2]
                    uint16_t x = (value[0] << 8) | value[1];
                    uint16_t y = (value[2] << 8) | value[3];
                    uint16_t w = (value[4] << 8) | value[5];
                    uint16_t h = (value[6] << 8) | value[7];
                    int16_t min_v = (value[8] << 8) | value[9];
                    int16_t max_v = (value[10] << 8) | value[11];
                    int16_t cur_v = (value[12] << 8) | value[13];
                    
                    lv_obj_t* bar = lv_bar_create(current_parent);
                    lv_obj_set_pos(bar, x, y);
                    lv_obj_set_size(bar, w, h);
                    lv_bar_set_range(bar, min_v, max_v);
                    lv_bar_set_value(bar, cur_v, LV_ANIM_OFF);
                } else if (node_length >= 6) {
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
                if (node_length >= 9) { // [x:2][y:2][w:2][h:2][id:1][options...]
                    uint16_t x = (value[0] << 8) | value[1];
                    uint16_t y = (value[2] << 8) | value[3];
                    uint16_t w = (value[4] << 8) | value[5];
                    uint16_t h = (value[6] << 8) | value[7];
                    
                    lv_obj_t* dd = lv_dropdown_create(current_parent);
                    lv_obj_set_pos(dd, x, y);
                    if (w > 0) lv_obj_set_width(dd, w);
                    if (h > 0) lv_obj_set_height(dd, h);
                    
                    if (node_length > 9) {
                        char decoded_opts[256];
                        tlv_decode_hybrid_text(value + 9, node_length - 9, decoded_opts, sizeof(decoded_opts));
                        lv_dropdown_set_options(dd, decoded_opts);
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
                    lv_obj_set_style_radius(chart, 6, 0);

                    lv_color_t color = (chart_type == 1) ? lv_palette_main(LV_PALETTE_TEAL) : lv_palette_main(LV_PALETTE_CYAN);
                    lv_chart_series_t* ser = lv_chart_add_series(chart, color, LV_CHART_AXIS_PRIMARY_Y);

                    size_t pt_offset = 11;
                    int16_t min_v = 32767, max_v = -32768;
                    for (uint16_t i = 0; i < num_pts && pt_offset + 2 <= node_length; i++) {
                        int16_t val = (value[pt_offset] << 8) | value[pt_offset + 1];
                        lv_chart_set_value_by_id(chart, ser, i, val);
                        if (val < min_v) min_v = val;
                        if (val > max_v) max_v = val;
                        pt_offset += 2;
                    }
                    if (min_v < max_v) {
                        int16_t range_min = (min_v > 5) ? (min_v - 5) : 0;
                        int16_t range_max = max_v + 5;
                        lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, range_min, range_max);
                    }
                    lv_chart_refresh(chart);
                }
                break;
            }
            case TYPE_ABS_ARC: {
                if (node_length >= 15) { // [x:2][y:2][w:2][h:2][id:1][min:2][max:2][val:2]
                    uint16_t x = (value[0] << 8) | value[1];
                    uint16_t y = (value[2] << 8) | value[3];
                    uint16_t w = (value[4] << 8) | value[5];
                    uint16_t h = (value[6] << 8) | value[7];
                    int16_t min_v = (value[9] << 8) | value[10];
                    int16_t max_v = (value[11] << 8) | value[12];
                    int16_t cur_v = (value[13] << 8) | value[14];

                    lv_obj_t* arc = lv_arc_create(current_parent);
                    lv_obj_set_pos(arc, x, y);
                    lv_obj_set_size(arc, w, h);
                    lv_arc_set_range(arc, min_v, max_v);
                    lv_arc_set_value(arc, cur_v);
                }
                break;
            }
            case TYPE_ABS_SPINNER: {
                if (node_length >= 12) { // [x:2][y:2][w:2][h:2][spin_time:2][arc_length:2]
                    uint16_t x = (value[0] << 8) | value[1];
                    uint16_t y = (value[2] << 8) | value[3];
                    uint16_t w = (value[4] << 8) | value[5];
                    uint16_t h = (value[6] << 8) | value[7];
                    uint16_t spin_time = (value[8] << 8) | value[9];
                    uint16_t arc_length = (value[10] << 8) | value[11];

                    lv_obj_t* spinner = lv_spinner_create(current_parent);
                    lv_obj_set_pos(spinner, x, y);
                    lv_obj_set_size(spinner, w, h);
                    lv_spinner_set_anim_params(spinner, spin_time, arc_length);
                }
                break;
            }
            default:
                break;
        }
    }
    
    return root_parent;
}
