#pragma once
#include "BaseView.hpp"
#include "cbdos/serial.hpp"
#include <lvgl.h>
#include <string>
#include <vector>
#include <atomic>

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
    void updatePortList();
    void updateConnectionUi(bool connected);
    void updateControlPinUi();

    static void timerPollCb(lv_timer_t* timer);
    static void connectBtnCb(lv_event_t* e);
    static void portChangedCb(lv_event_t* e);
    static void baudChangedCb(lv_event_t* e);
    static void txPinChangedCb(lv_event_t* e);
    static void rxPinChangedCb(lv_event_t* e);
    static void presetJp1BtnCb(lv_event_t* e);
    static void presetUart0BtnCb(lv_event_t* e);
    static void resetBtnCb(lv_event_t* e);
    static void dfuBtnCb(lv_event_t* e);
    static void holdBtnCb(lv_event_t* e);
    static void clearBtnCb(lv_event_t* e);
    static void saveBtnCb(lv_event_t* e);
    static void lineEndingChangedCb(lv_event_t* e);
    static void echoToggleBtnCb(lv_event_t* e);
    static void sendBtnCb(lv_event_t* e);
    static void kbToggleBtnCb(lv_event_t* e);
    static void quickKeyBtnCb(lv_event_t* e);
    static void inputFocusedCb(lv_event_t* e);

    // UI Widgets - Barra Superior y Pines
    lv_obj_t* m_ddPort = nullptr;
    lv_obj_t* m_ddBaud = nullptr;
    lv_obj_t* m_btnConnect = nullptr;
    lv_obj_t* m_lblConnect = nullptr;
    lv_obj_t* m_btnReset = nullptr;
    lv_obj_t* m_btnDfu = nullptr;
    lv_obj_t* m_btnHold = nullptr;
    lv_obj_t* m_lblHold = nullptr;
    lv_obj_t* m_btnClear = nullptr;
    lv_obj_t* m_btnSave = nullptr;

    // Barra de Configuración de Pines GPIO
    lv_obj_t* m_pinToolbar = nullptr;
    lv_obj_t* m_ddTxPin = nullptr;
    lv_obj_t* m_ddRxPin = nullptr;

    // UI Widgets - Comandos, Protocolo y Teclado
    lv_obj_t* m_ddLineEnding = nullptr;
    lv_obj_t* m_btnEcho = nullptr;
    lv_obj_t* m_lblEcho = nullptr;
    lv_obj_t* m_taTerminal = nullptr;
    lv_obj_t* m_inputCmd = nullptr;
    lv_obj_t* m_btnSend = nullptr;
    lv_obj_t* m_btnToggleKb = nullptr;
    lv_obj_t* m_keyboard = nullptr;
    lv_timer_t* m_pollTimer = nullptr;

    // Estado interno
    bool m_isConnected = false;
    bool m_kbVisible = false;
    bool m_isHoldActive = false;
    bool m_localEcho = false;
    cbdos::serial::LineEnding m_lineEnding = cbdos::serial::LineEnding::CRLF;
    std::string m_holdPendingBuffer;
    int m_currentTxPin = 32;
    int m_currentRxPin = 28;
    int m_currentControlPin = 34;
    uint32_t m_currentBaud = 115200;
    std::string m_selectedPortId;
    std::vector<cbdos::serial::SerialPortDescriptor> m_ports;
    std::atomic<bool> m_portsDirty{false};

    void onHotplugEvent(bool connected, const std::string& portId);
    static std::string sanitizeAndStripAnsi(const char* data, size_t len);

    std::string m_terminalBuffer;
    static constexpr size_t MAX_TERMINAL_BUFFER_SIZE = 8192;
};

} // namespace ui
} // namespace cbdos
