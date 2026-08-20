#ifndef DEFAULT_THEME_H
#define DEFAULT_THEME_H

#include <lvgl.h>

class DefaultTheme {
public:
    static void applyFlatBg(lv_obj_t* obj);
    static void applyRaisedCard(lv_obj_t* obj, int32_t radius = 16);
    static void applySunkenCard(lv_obj_t* obj, int32_t radius = 16);
    static void applyButton(lv_obj_t* obj, int32_t radius = 20);
    static void applyTextArea(lv_obj_t* obj, int32_t radius = 10);
    static void disableScroll(lv_obj_t* obj);

    static lv_color_t getBgColor() { return lv_color_hex(0x161821); }
    static lv_color_t getPrimaryAccent() { return lv_color_hex(0x00F5D4); }
    static lv_color_t getSecondaryAccent() { return lv_color_hex(0x9D4EDD); }
    static lv_color_t getTextColor() { return lv_color_hex(0xF1F5F9); }
    static lv_color_t getMutedTextColor() { return lv_color_hex(0x94A3B8); }
};

#endif
