#include "DefaultTheme.h"

void DefaultTheme::disableScroll(lv_obj_t* obj) {
    if(!obj) return;
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
}

void DefaultTheme::applyFlatBg(lv_obj_t* obj) {
    lv_obj_set_style_bg_color(obj, getBgColor(), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    disableScroll(obj);
}

void DefaultTheme::applyRaisedCard(lv_obj_t* obj, int32_t radius) {
    disableScroll(obj);
    
    // Fondo oscuro sólido y plano (Dark Mode Flat)
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x1B1E29), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj, radius, 0);
    
    // Borde muy sutil para dar separación sin usar sombras o biseles
    lv_obj_set_style_border_color(obj, lv_color_hex(0x2E3444), 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_border_opa(obj, LV_OPA_COVER, 0);
}

void DefaultTheme::applySunkenCard(lv_obj_t* obj, int32_t radius) {
    disableScroll(obj);
    
    // Tono hendido/hundido en la superficie
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x11131A), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj, radius, 0);
    
    // Bisel interno cavado
    lv_obj_set_style_border_color(obj, lv_color_hex(0x0B0C11), 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_border_opa(obj, LV_OPA_COVER, 0);

    lv_obj_set_style_bg_color(obj, lv_color_hex(0x11131A), LV_STATE_PRESSED);
}

void DefaultTheme::applyButton(lv_obj_t* obj, int32_t radius) {
    applyRaisedCard(obj, radius);

    // Mantenemos idéntico al inactivo
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x1B1E29), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(obj, lv_color_hex(0x2E3444), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(obj, 1, LV_STATE_PRESSED);
    lv_obj_set_style_border_opa(obj, LV_OPA_COVER, LV_STATE_PRESSED);
}
