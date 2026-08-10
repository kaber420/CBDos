#pragma once
#include <lvgl.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t* tlv_browser_render(lv_obj_t* root_parent, const uint8_t* data, size_t length);

#ifdef __cplusplus
}
#endif
