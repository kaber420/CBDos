#pragma once
#include "BaseView.hpp"
#include "cbdos/uart.hpp"
#include <lvgl.h>
#include <string>
#include <vector>

namespace cbdos {
namespace ui {

class SerialTerminalView : public BaseView {
public:
    SerialTerminalView();
    ~SerialTerminalView() override;

    bool onCreate(lv_obj_t* parent) override;
    void onDestroy() override;
    void onShow() override;
    void onHide() override;
    void onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) override;

private:
    void createToolbar(lv_obj_t* parent);
    void createTerminalDisplay(lv_obj_t* parent);
    void createCommandBar(lv_obj_t* parent);
    void createKeyboard(lv_obj_t* parent);

    void appendText(const char* text, size_t len);
    void sendCommand(const std::string& cmd);
    void sendSpecialKey(const char* keyBytes, size_t len);
    void saveLogToSd();

    static void timerPollCb(lv_timer_t* timer);
    static void baudChangedCb(lv_event_t* e);
    static void presetChangedCb(lv_event_t* e);
    static void clearBtnCb(lv_event_t* e);
    static void pauseBtnCb(lv_event_t* e);
    static void saveBtnCb(lv_event_t* e);
    static void sendBtnCb(lv_event_t* e);
    static void kbToggleBtnCb(lv_event_t* e);
    static void quickKeyBtnCb(lv_event_t* e);
    static void inputFocusedCb(lv_event_t* e);

    // UI Widgets
    lv_obj_t* m_ddBaud = nullptr;
    lv_obj_t* m_ddPreset = nullptr;
    lv_obj_t* m_btnPause = nullptr;
    lv_obj_t* m_lblPause = nullptr;
    lv_obj_t* m_btnClear = nullptr;
    lv_obj_t* m_btnSave = nullptr;

    lv_obj_t* m_taTerminal = nullptr;
    lv_obj_t* m_inputCmd = nullptr;
    lv_obj_t* m_btnSend = nullptr;
    lv_obj_t* m_btnToggleKb = nullptr;
    lv_obj_t* m_keyboard = nullptr;
    lv_timer_t* m_pollTimer = nullptr;

    // Estado interno
    bool m_isPaused = false;
    bool m_kbVisible = false;
    uint32_t m_currentBaud = 115200;
    int m_currentTxPin = -1;
    int m_currentRxPin = -1;
    std::string m_terminalBuffer;
    static constexpr size_t MAX_TERMINAL_BUFFER_SIZE = 8192;
};

} // namespace ui
} // namespace cbdos
