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
    
    lv_obj_t* m_ddTxPin = nullptr;
    lv_obj_t* m_ddRxPin = nullptr;
    lv_obj_t* m_ddBootPin = nullptr;
    lv_obj_t* m_ddRstPin = nullptr;
    lv_obj_t* m_ddBaud = nullptr;
    lv_obj_t* m_lblFirmwareSource = nullptr;
    lv_obj_t* m_btnPickFile = nullptr;

    lv_obj_t* m_barProgress = nullptr;
    lv_obj_t* m_lblStatus = nullptr;
    lv_obj_t* m_btnStart = nullptr;
    lv_obj_t* m_lblBtn = nullptr;
    lv_obj_t* m_lblLog = nullptr;

    // Selector modal de archivos
    lv_obj_t* m_filePickerMask = nullptr;
    lv_obj_t* m_pickerListCont = nullptr;
    lv_obj_t* m_pickerPathLabel = nullptr;
    std::string m_pickerPath = "/sdcard";

    void updateUIFromConfig();
    void openFilePickerModal(const std::string& startPath = "/sdcard");
    void closeFilePickerModal();
    void renderPickerFileList();

    static void presetChangedCb(lv_event_t* e);
    static void baudChangedCb(lv_event_t* e);
    static void pinDropdownChangedCb(lv_event_t* e);
    static void pickFileBtnCb(lv_event_t* e);
    static void pickerCloseBtnCb(lv_event_t* e);
    static void pickerUpBtnCb(lv_event_t* e);
    static void pickerItemClickCb(lv_event_t* e);
    static void startFlashCb(lv_event_t* e);
};

} // namespace ui
} // namespace cbdos


