#pragma once

#include "BaseView.hpp"
#include <lvgl.h>

namespace cbdos {
namespace ui {

class UtilitiesView : public BaseView {
public:
    UtilitiesView();
    virtual ~UtilitiesView() = default;

    bool onCreate(lv_obj_t* parent) override;
    void onDestroy() override;
    void onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) override;

private:
    lv_obj_t* m_tabview;
};

} // namespace ui
} // namespace cbdos
