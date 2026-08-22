#pragma once

#include "BaseView.hpp"
#include <lvgl.h>
#include <string>
#include <vector>

namespace cbdos {
namespace ui {

class LuaRunnerView : public BaseView {
public:
    explicit LuaRunnerView(const std::string& initialScript = "");
    virtual ~LuaRunnerView() = default;

    bool onCreate(lv_obj_t* parent) override;
    void onDestroy() override;
    void onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) override;

    void setScript(const std::string& path);

private:
    static void timerCb(lv_timer_t* timer);
    static void btnRunCb(lv_event_t* e);
    static void btnStopCb(lv_event_t* e);
    static void btnEditCb(lv_event_t* e);
    static void btnSdCb(lv_event_t* e);
    static void btnClearCb(lv_event_t* e);
    static void btnCreateDemoCb(lv_event_t* e);
    static void fileSelectCb(lv_event_t* e);
    static void modalCloseCb(lv_event_t* e);

    void scanLuaFilesSD();
    void createDemoScripts();
    void showFilePickerModal();
    void appendLogLine(const std::string& line);
    void updateStatusBadge();

    lv_obj_t* m_logContainer;
    lv_obj_t* m_statusBadge;
    lv_obj_t* m_scriptLabel;
    lv_timer_t* m_refreshTimer;
    lv_obj_t* m_modalMask;
    std::string m_activeScript;
    std::vector<std::string> m_foundLuaFiles;
};

} // namespace ui
} // namespace cbdos
