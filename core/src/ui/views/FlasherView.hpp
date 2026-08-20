#pragma once
#include "BaseView.hpp"
#include "cbdos/flasher.hpp"
#include <lvgl.h>
#include <vector>

namespace cbdos {
namespace ui {

class FlasherView : public BaseView {
public:
    FlasherView();
    ~FlasherView() override = default;

    bool onCreate(lv_obj_t* parent) override;
    void onDestroy() override;
    void onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) override;

private:
    std::vector<cbdos::flasher::FlasherPreset> m_presets;
    int m_selectedPresetIndex = 0;
    cbdos::flasher::FlasherConfig m_currentConfig;

    // UI Widgets
    lv_obj_t* m_ddPresets = nullptr;
    lv_obj_t* m_lblWiring = nullptr;
    lv_obj_t* m_lblPresetDesc = nullptr;
    lv_obj_t* m_cardPinConfig = nullptr;
    
    lv_obj_t* m_lblTxPin = nullptr;
    lv_obj_t* m_lblRxPin = nullptr;
    lv_obj_t* m_lblBootPin = nullptr;
    lv_obj_t* m_lblRstPin = nullptr;
    lv_obj_t* m_ddBaud = nullptr;
    lv_obj_t* m_lblFirmwareSource = nullptr;

    lv_obj_t* m_barProgress = nullptr;
    lv_obj_t* m_lblStatus = nullptr;
    lv_obj_t* m_btnStart = nullptr;
    lv_obj_t* m_lblBtn = nullptr;
    lv_obj_t* m_lblLog = nullptr;

    void updateUIFromConfig();
    static void presetChangedCb(lv_event_t* e);
    static void baudChangedCb(lv_event_t* e);
    static void pinAdjustCb(lv_event_t* e);
    static void startFlashCb(lv_event_t* e);
};

} // namespace ui
} // namespace cbdos

