#pragma once
#include <lvgl.h>

enum class ThemeType {
    LIGHT,
    DARK,
    BRANDED_RED,
    BRANDED_BLUE
};

class ThemeEngine {
public:
    static ThemeEngine& getInstance() {
        static ThemeEngine instance;
        return instance;
    }

    void applyTheme(ThemeType type);

    lv_color_t getPrimaryColor() const { return primaryColor; }
    lv_color_t getBgColor() const { return bgColor; }
    lv_color_t getTextColor() const { return textColor; }
    lv_color_t getCardColor() const { return cardColor; }
    int32_t getRadius() const { return radius; }

private:
    ThemeEngine() { applyTheme(ThemeType::DARK); }
    
    lv_color_t primaryColor;
    lv_color_t bgColor;
    lv_color_t textColor;
    lv_color_t cardColor;
    int32_t radius;
};
