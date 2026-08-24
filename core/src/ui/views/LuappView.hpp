#pragma once

#include "BaseView.hpp"
#include <string>
#include <lvgl.h>

struct lua_State;

namespace cbdos {
namespace ui {

class LuappView : public BaseView {
public:
    explicit LuappView(const std::string& scriptPath, 
                       const std::string& appName = "App Lua",
                       const std::string& iconSymbol = LV_SYMBOL_FILE);
    ~LuappView() override;

    bool onCreate(lv_obj_t* parent) override;
    void onDestroy() override;
    void onShow() override;
    void onHide() override;
    void onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) override;

private:
    void renderError(const char* message);

    std::string m_scriptPath;
    std::string m_iconSymbol;
    lua_State* m_L;
    bool m_initialized;
};

} // namespace ui
} // namespace cbdos
