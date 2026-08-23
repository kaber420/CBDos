#pragma once
#include <lvgl.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Downlink TLV Tags
#define TYPE_ABS_PAGE      0x10
#define TYPE_ABS_TEXT      0x11
#define TYPE_ABS_LINK      0x12
#define TYPE_ABS_INPUT     0x13
#define TYPE_ABS_IMAGE     0x14
#define TYPE_ABS_CHECKBOX  0x15
#define TYPE_ABS_SWITCH    0x16
#define TYPE_ABS_SLIDER    0x17
#define TYPE_ABS_PROGRESS  0x18
#define TYPE_ABS_DROPDOWN  0x19
#define TYPE_ABS_PANEL     0x1A
#define TYPE_ABS_CHART     0x1B
#define TYPE_END           0xFE

// Uplink TLV Event Tags
#define TYPE_REQ_URL          0x01
#define TYPE_REQ_INPUT_SUBMIT 0x20
#define TYPE_REQ_LINK_CLICK   0x21

// Callback signature for sending uplink binary frames
typedef void (*tlv_uplink_send_cb_t)(const uint8_t* frame, size_t len);

void tlv_browser_set_uplink_cb(tlv_uplink_send_cb_t cb);

lv_obj_t* tlv_browser_render(lv_obj_t* root_parent, const uint8_t* data, size_t length);

// Builder functions for Uplink TLV frames
size_t tlv_build_req_url(uint8_t* buf, size_t max_len, const char* url);
size_t tlv_build_req_submit(uint8_t* buf, size_t max_len, uint8_t element_id, const char* text);
size_t tlv_build_req_click(uint8_t* buf, size_t max_len, uint8_t link_id);

#ifdef __cplusplus
}
#endif
