#pragma once
#include "BaseView.hpp"
#include <vector>

namespace cbdos {
namespace ui {

struct AppItem {
    const char* id;
    const char* title;
    const char* icon;
    uint32_t accentColor;
};

class DashboardView : public BaseView {
public:
    DashboardView();
    ~DashboardView() override = default;

    bool onCreate(lv_obj_t* parent) override;
    void onDestroy() override;
    void onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) override;

private:
    void setupLayout();
    void createCards();
    static void cardClickedEventCb(lv_event_t* e);

    std::vector<AppItem> m_apps;
    std::vector<lv_obj_t*> m_cardObjs;
};

} // namespace ui
} // namespace cbdos
