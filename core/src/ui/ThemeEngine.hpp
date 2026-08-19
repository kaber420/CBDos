#pragma once
#include "cbdos/theme.hpp"
#include <vector>
#include <functional>
#include <lvgl.h>

namespace cbdos {
namespace ui {

class ThemeEngine {
public:
    using ThemeChangeCallback = std::function<void(cbdos::theme::ThemeType, const cbdos::theme::ThemePalette&)>;

    static ThemeEngine& getInstance();

    void init();
    void setTheme(cbdos::theme::ThemeType theme);
    cbdos::theme::ThemeType getCurrentTheme() const;
    const cbdos::theme::ThemePalette& getPalette() const;
    const cbdos::theme::ThemePalette& getPalette(cbdos::theme::ThemeType theme) const;
    const char* getThemeName(cbdos::theme::ThemeType theme) const;

    void registerCallback(ThemeChangeCallback callback);
    
    // Helpers para LVGL
    lv_color_t colorBg() const { return lv_color_hex(getPalette().bg); }
    lv_color_t colorPanel() const { return lv_color_hex(getPalette().panel); }
    lv_color_t colorPanelBorder() const { return lv_color_hex(getPalette().panelBorder); }
    lv_color_t colorPrimary() const { return lv_color_hex(getPalette().primary); }
    lv_color_t colorSecondary() const { return lv_color_hex(getPalette().secondary); }
    lv_color_t colorAccent() const { return lv_color_hex(getPalette().accent); }
    lv_color_t colorTextPrimary() const { return lv_color_hex(getPalette().textPrimary); }
    lv_color_t colorTextSecondary() const { return lv_color_hex(getPalette().textSecondary); }
    lv_color_t colorSuccess() const { return lv_color_hex(getPalette().success); }
    lv_color_t colorWarning() const { return lv_color_hex(getPalette().warning); }
    lv_color_t colorError() const { return lv_color_hex(getPalette().error); }

private:
    ThemeEngine();
    ~ThemeEngine() = default;
    ThemeEngine(const ThemeEngine&) = delete;
    ThemeEngine& operator=(const ThemeEngine&) = delete;

    cbdos::theme::ThemeType m_currentTheme;
    std::vector<ThemeChangeCallback> m_callbacks;
};

} // namespace ui
} // namespace cbdos
