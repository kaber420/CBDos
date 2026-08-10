#include "ThemeEngine.h"

void ThemeEngine::applyTheme(ThemeType type) {
    switch (type) {
        case ThemeType::LIGHT:
            primaryColor = lv_color_hex(0x007AFF);
            bgColor = lv_color_hex(0xF2F2F7);
            textColor = lv_color_hex(0x1C1C1E);
            cardColor = lv_color_hex(0xFFFFFF);
            radius = 12;
            break;
        case ThemeType::DARK:
            primaryColor = lv_color_hex(0x0A84FF);
            bgColor = lv_color_hex(0x000000);
            textColor = lv_color_hex(0xFFFFFF);
            cardColor = lv_color_hex(0x1C1C1E);
            radius = 12;
            break;
        case ThemeType::BRANDED_RED:
            primaryColor = lv_color_hex(0xE53935);
            bgColor = lv_color_hex(0x121212);
            textColor = lv_color_hex(0xFFFFFF);
            cardColor = lv_color_hex(0x1E1E1E);
            radius = 8;
            break;
        case ThemeType::BRANDED_BLUE:
            primaryColor = lv_color_hex(0x1E88E5);
            bgColor = lv_color_hex(0xFAFAFA);
            textColor = lv_color_hex(0x212121);
            cardColor = lv_color_hex(0xFFFFFF);
            radius = 16;
            break;
    }
}
