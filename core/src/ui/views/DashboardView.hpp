#pragma once
#include "BaseView.hpp"
#include <vector>
#include <string>

namespace cbdos {
namespace ui {

struct AppItem {
    std::string id;
    std::string title;
    std::string icon;
    uint32_t accentColor;
    bool isLuapp = false;
    std::string luappPath = "";
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
