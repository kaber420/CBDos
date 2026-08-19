#pragma once
#include <lvgl.h>
#include <functional>
#include "cbdos/theme.hpp"

namespace cbdos {
namespace ui {

class HeaderBar {
public:
    using ClickCallback = std::function<void()>;

    HeaderBar();
    ~HeaderBar();

    bool init(lv_obj_t* parent);
    void update();
    void onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette);
    void setOnClickCallback(ClickCallback cb);
    void setTitle(const char* title);
    void showBackButton(bool show, ClickCallback onBack = nullptr);

    lv_obj_t* getContainer() const { return m_container; }

private:
    static void eventHandler(lv_event_t* e);
    static void backBtnEventHandler(lv_event_t* e);

    lv_obj_t* m_container;
    lv_obj_t* m_labelTitle;
    lv_obj_t* m_btnBack;
    lv_obj_t* m_labelClock;
    lv_obj_t* m_labelWifi;

    ClickCallback m_onClickCb;
    ClickCallback m_onBackCb;
    uint32_t m_lastUpdateMs;
};

} // namespace ui
} // namespace cbdos
