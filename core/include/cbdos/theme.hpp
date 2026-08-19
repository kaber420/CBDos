#pragma once
#include <cstdint>

namespace cbdos {
namespace theme {

enum class ThemeType {
    Cyberpunk = 0,
    Matrix,
    SolarAmber
};

struct ThemePalette {
    uint32_t bg;
    uint32_t panel;
    uint32_t panelBorder;
    uint32_t primary;
    uint32_t secondary;
    uint32_t accent;
    uint32_t textPrimary;
    uint32_t textSecondary;
    uint32_t success;
    uint32_t warning;
    uint32_t error;
};

ThemeType getCurrentTheme();
void setTheme(ThemeType theme);
const ThemePalette& getPalette();
const ThemePalette& getPalette(ThemeType theme);
const char* getThemeName(ThemeType theme);

} // namespace theme
} // namespace cbdos
