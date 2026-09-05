#pragma once
#include <lvgl.h>
#include <string>
#include <vector>
#include <functional>
#include "cbdos/serial.hpp"

namespace cbdos {
namespace ui {

class SerialControlBar {
public:
    using ConnectCallback = std::function<void(bool doConnect, const cbdos::serial::SerialConfig& cfg)>;
    using ResetCallback = std::function<void(bool enterBootloader)>;
    using ActionCallback = std::function<void()>;
    using LogCallback = std::function<void(const std::string& msg)>;

    SerialControlBar() = default;
    ~SerialControlBar() = default;

    bool create(lv_obj_t* parent,
                ConnectCallback onConnect,
                ResetCallback onReset,
                ActionCallback onHold,
                ActionCallback onClear,
                ActionCallback onSave,
                LogCallback onLog);

    void setConnected(bool connected);
    void setHoldActive(bool active);
    void updatePortList();
    void show(bool visible);
    bool isConnected() const { return m_isConnected; }
    cbdos::serial::SerialConfig getCurrentConfig() const;

private:
    void updateControlPinUi();

    static void connectBtnCb(lv_event_t* e);
    static void portChangedCb(lv_event_t* e);
    static void baudChangedCb(lv_event_t* e);
    static void resetBtnCb(lv_event_t* e);
    static void dfuBtnCb(lv_event_t* e);
    static void holdBtnCb(lv_event_t* e);
    static void clearBtnCb(lv_event_t* e);
    static void saveBtnCb(lv_event_t* e);
    static void txPinChangedCb(lv_event_t* e);
    static void rxPinChangedCb(lv_event_t* e);
    static void presetJp1BtnCb(lv_event_t* e);
    static void presetUart0BtnCb(lv_event_t* e);

    lv_obj_t* m_toolbar = nullptr;
    lv_obj_t* m_pinToolbar = nullptr;

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

    lv_obj_t* m_ddTxPin = nullptr;
    lv_obj_t* m_ddRxPin = nullptr;

    bool m_isConnected = false;
    bool m_isHoldActive = false;
    int m_currentTxPin = 32;
    int m_currentRxPin = 28;
    int m_currentControlPin = 34;
    uint32_t m_currentBaud = 115200;
    std::string m_selectedPortId;
    std::vector<cbdos::serial::SerialPortDescriptor> m_ports;

    ConnectCallback m_onConnect;
    ResetCallback m_onReset;
    ActionCallback m_onHold;
    ActionCallback m_onClear;
    ActionCallback m_onSave;
    LogCallback m_onLog;
};

} // namespace ui
} // namespace cbdos
