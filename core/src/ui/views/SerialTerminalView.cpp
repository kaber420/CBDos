#include "SerialTerminalView.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include "cbdos/storage.hpp"
#include "cbdos/display.hpp"
#include "cbdos/system.hpp"
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace cbdos {
namespace ui {

static const uint32_t s_baudRates[] = {
    9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600
};

static const int s_txPins[] = {32, 38, 50, 52, 15};
static const int s_rxPins[] = {28, 37, 49, 51, 16};

SerialTerminalView::SerialTerminalView()
    : BaseView("SerialTerminal"),
      m_isConnected(false),
      m_kbVisible(false),
      m_currentTxPin(32),
      m_currentRxPin(28),
      m_currentControlPin(34),
      m_currentBaud(115200) {
    m_ports = cbdos::serial::getAvailablePorts();
    if (!m_ports.empty()) {
        m_selectedPortId = m_ports[0].id;
        if (m_ports[0].defaultTx >= 0) m_currentTxPin = m_ports[0].defaultTx;
        if (m_ports[0].defaultRx >= 0) m_currentRxPin = m_ports[0].defaultRx;
        if (m_ports[0].controlPin >= 0) m_currentControlPin = m_ports[0].controlPin;
    }
}

SerialTerminalView::~SerialTerminalView() {
    onDestroy();
}

bool SerialTerminalView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;

    // Contenedor principal
    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(m_container, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(m_container, DefaultTheme::getBgColor(), 0);
    lv_obj_set_style_border_width(m_container, 0, 0);
    lv_obj_set_style_radius(m_container, 0, 0);
    lv_obj_set_style_pad_all(m_container, 6, 0);
    lv_obj_set_flex_flow(m_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(m_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(m_container, LV_SCROLLBAR_MODE_OFF);

    // Cero auto-inicialización: el hardware arranca en reposo total
    createToolbar(m_container);
    createTerminalDisplay(m_container);
    createCommandBar(m_container);
    createKeyboard(m_container);

    // Registro de eventos de hotplug por hardware (CERO polling)
    cbdos::serial::setHotplugCallback([this](bool connected, const std::string& portId) {
        this->onHotplugEvent(connected, portId);
    });

    // Timer reactivo para renderizado seguro (50 ms)
    m_pollTimer = lv_timer_create(timerPollCb, 50, this);

    return true;
}

void SerialTerminalView::onDestroy() {
    cbdos::serial::setHotplugCallback(nullptr);
    if (m_pollTimer) {
        lv_timer_delete(m_pollTimer);
        m_pollTimer = nullptr;
    }
    if (m_isConnected) {
        cbdos::serial::close();
        m_isConnected = false;
    }
    BaseView::onDestroy();
}

void SerialTerminalView::onShow() {
    BaseView::onShow();
    HeaderBar& hb = UIManager::getInstance().getHeaderBar();
    hb.setTitle("Terminal Serial");
    hb.showWifi(false);
    if (m_pollTimer) {
        lv_timer_resume(m_pollTimer);
    }
}

void SerialTerminalView::onHide() {
    if (m_pollTimer) {
        lv_timer_pause(m_pollTimer);
    }
    BaseView::onHide();
}

void SerialTerminalView::createToolbar(lv_obj_t* parent) {
    // 1. Fila Principal: Puerto, Baudrate, Conectar, Botón ⚡ RST, Limpiar, Guardar
    lv_obj_t* toolbar = lv_obj_create(parent);
    lv_obj_set_size(toolbar, LV_PCT(100), LV_SIZE_CONTENT);
    DefaultTheme::applyRaisedCard(toolbar, 8);
    lv_obj_set_style_pad_all(toolbar, 4, 0);
    lv_obj_set_flex_flow(toolbar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(toolbar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    DefaultTheme::disableScroll(toolbar);

    // Selector de Puerto
    m_ddPort = lv_dropdown_create(toolbar);
    updatePortList();
    lv_obj_set_width(m_ddPort, 135);
    lv_obj_set_style_text_font(m_ddPort, &lv_font_montserrat_12, 0);
    lv_obj_add_event_cb(m_ddPort, portChangedCb, LV_EVENT_VALUE_CHANGED, this);

    // Selector de Baudrate
    m_ddBaud = lv_dropdown_create(toolbar);
    lv_dropdown_set_options(m_ddBaud, "9600\n19200\n38400\n57600\n115200\n230400\n460800\n921600");
    lv_dropdown_set_selected(m_ddBaud, 4); // 115200
    lv_obj_set_width(m_ddBaud, 80);
    lv_obj_set_style_text_font(m_ddBaud, &lv_font_montserrat_12, 0);
    lv_obj_add_event_cb(m_ddBaud, baudChangedCb, LV_EVENT_VALUE_CHANGED, this);

    // Botón Conectar / Desconectar
    m_btnConnect = lv_button_create(toolbar);
    DefaultTheme::applyButton(m_btnConnect, 8);
    lv_obj_set_size(m_btnConnect, 80, 32);
    lv_obj_set_style_bg_color(m_btnConnect, lv_palette_main(LV_PALETTE_GREEN), 0);
    m_lblConnect = lv_label_create(m_btnConnect);
    lv_label_set_text(m_lblConnect, LV_SYMBOL_PLAY " Abrir");
    lv_obj_set_style_text_font(m_lblConnect, &lv_font_montserrat_12, 0);
    lv_obj_center(m_lblConnect);
    lv_obj_add_event_cb(m_btnConnect, connectBtnCb, LV_EVENT_CLICKED, this);

    // Botón ⚡ RST (SIEMPRE VISIBLE Y DISPONIBLE)
    m_btnReset = lv_button_create(toolbar);
    DefaultTheme::applyButton(m_btnReset, 8);
    lv_obj_set_size(m_btnReset, 54, 32);
    lv_obj_set_style_bg_color(m_btnReset, lv_palette_main(LV_PALETTE_ORANGE), 0);
    lv_obj_t* lblReset = lv_label_create(m_btnReset);
    lv_label_set_text(lblReset, "⚡ RST");
    lv_obj_set_style_text_font(lblReset, &lv_font_montserrat_12, 0);
    lv_obj_center(lblReset);
    lv_obj_add_event_cb(m_btnReset, resetBtnCb, LV_EVENT_CLICKED, this);

    // Botón Limpiar (Clear)
    m_btnClear = lv_button_create(toolbar);
    DefaultTheme::applyButton(m_btnClear, 8);
    lv_obj_set_size(m_btnClear, 36, 32);
    lv_obj_t* lblClear = lv_label_create(m_btnClear);
    lv_label_set_text(lblClear, LV_SYMBOL_TRASH);
    lv_obj_center(lblClear);
    lv_obj_add_event_cb(m_btnClear, clearBtnCb, LV_EVENT_CLICKED, this);

    // Botón Guardar en SD
    m_btnSave = lv_button_create(toolbar);
    DefaultTheme::applyButton(m_btnSave, 8);
    lv_obj_set_size(m_btnSave, 36, 32);
    lv_obj_t* lblSave = lv_label_create(m_btnSave);
    lv_label_set_text(lblSave, LV_SYMBOL_SD_CARD);
    lv_obj_center(lblSave);
    lv_obj_add_event_cb(m_btnSave, saveBtnCb, LV_EVENT_CLICKED, this);

    // 2. Fila Secundaria: Pines GPIO TX / RX y Presets rápidos
    m_pinToolbar = lv_obj_create(parent);
    lv_obj_set_size(m_pinToolbar, LV_PCT(100), 34);
    DefaultTheme::applySunkenCard(m_pinToolbar, 6);
    lv_obj_set_style_pad_all(m_pinToolbar, 2, 0);
    lv_obj_set_flex_flow(m_pinToolbar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(m_pinToolbar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(m_pinToolbar, 6, 0);
    DefaultTheme::disableScroll(m_pinToolbar);

    lv_obj_t* lblPines = lv_label_create(m_pinToolbar);
    lv_label_set_text(lblPines, "Pines:");
    lv_obj_set_style_text_font(lblPines, &lv_font_montserrat_12, 0);

    // Selector Dropdown TX
    m_ddTxPin = lv_dropdown_create(m_pinToolbar);
    lv_dropdown_set_options(m_ddTxPin, "TX: 32\nTX: 38\nTX: 50\nTX: 52\nTX: 15");
    lv_dropdown_set_selected(m_ddTxPin, 0);
    lv_obj_set_width(m_ddTxPin, 88);
    lv_obj_set_style_text_font(m_ddTxPin, &lv_font_montserrat_12, 0);
    lv_obj_add_event_cb(m_ddTxPin, txPinChangedCb, LV_EVENT_VALUE_CHANGED, this);

    // Selector Dropdown RX
    m_ddRxPin = lv_dropdown_create(m_pinToolbar);
    lv_dropdown_set_options(m_ddRxPin, "RX: 28\nRX: 37\nRX: 49\nRX: 51\nRX: 16");
    lv_dropdown_set_selected(m_ddRxPin, 0);
    lv_obj_set_width(m_ddRxPin, 88);
    lv_obj_set_style_text_font(m_ddRxPin, &lv_font_montserrat_12, 0);
    lv_obj_add_event_cb(m_ddRxPin, rxPinChangedCb, LV_EVENT_VALUE_CHANGED, this);

    // Botón Preset JP1
    lv_obj_t* btnJp1 = lv_button_create(m_pinToolbar);
    DefaultTheme::applyButton(btnJp1, 6);
    lv_obj_set_size(btnJp1, 64, 26);
    lv_obj_t* lblJp1 = lv_label_create(btnJp1);
    lv_label_set_text(lblJp1, "JP1 (32)");
    lv_obj_set_style_text_font(lblJp1, &lv_font_montserrat_12, 0);
    lv_obj_center(lblJp1);
    lv_obj_add_event_cb(btnJp1, presetJp1BtnCb, LV_EVENT_CLICKED, this);

    // Botón Preset UART0 MX
    lv_obj_t* btnU0 = lv_button_create(m_pinToolbar);
    DefaultTheme::applyButton(btnU0, 6);
    lv_obj_set_size(btnU0, 68, 26);
    lv_obj_t* lblU0 = lv_label_create(btnU0);
    lv_label_set_text(lblU0, "MX (38)");
    lv_obj_set_style_text_font(lblU0, &lv_font_montserrat_12, 0);
    lv_obj_center(lblU0);
    lv_obj_add_event_cb(btnU0, presetUart0BtnCb, LV_EVENT_CLICKED, this);

    updateControlPinUi();
}

void SerialTerminalView::onHotplugEvent(bool connected, const std::string& portId) {
    (void)connected;
    (void)portId;
    m_portsDirty = true;
}

void SerialTerminalView::updatePortList() {
    if (!m_ddPort) return;
    std::string previousId = m_selectedPortId;
    m_ports = cbdos::serial::getAvailablePorts();
    std::string portOptions = "";
    int selectIdx = 0;
    for (size_t i = 0; i < m_ports.size(); i++) {
        portOptions += m_ports[i].displayName;
        if (m_ports[i].id == previousId) {
            selectIdx = (int)i;
        }
        if (i + 1 < m_ports.size()) portOptions += "\n";
    }
    if (portOptions.empty()) portOptions = "Sin Puertos";
    lv_dropdown_set_options(m_ddPort, portOptions.c_str());
    lv_dropdown_set_selected(m_ddPort, selectIdx);
    if (!m_ports.empty() && selectIdx < (int)m_ports.size()) {
        m_selectedPortId = m_ports[selectIdx].id;
        m_currentControlPin = m_ports[selectIdx].controlPin;
        if (m_ports[selectIdx].defaultTx >= 0) m_currentTxPin = m_ports[selectIdx].defaultTx;
        if (m_ports[selectIdx].defaultRx >= 0) m_currentRxPin = m_ports[selectIdx].defaultRx;
        updateControlPinUi();
    }
}

void SerialTerminalView::updateControlPinUi() {
    bool isUsb = (m_selectedPortId.rfind("usb", 0) == 0);
    if (m_pinToolbar) {
        if (isUsb) {
            lv_obj_add_flag(m_pinToolbar, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_remove_flag(m_pinToolbar, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void SerialTerminalView::updateConnectionUi(bool connected) {
    m_isConnected = connected;
    if (m_btnConnect && m_lblConnect) {
        if (connected) {
            lv_obj_set_style_bg_color(m_btnConnect, lv_palette_main(LV_PALETTE_RED), 0);
            lv_label_set_text(m_lblConnect, LV_SYMBOL_STOP " Cerrar");
        } else {
            lv_obj_set_style_bg_color(m_btnConnect, lv_palette_main(LV_PALETTE_GREEN), 0);
            lv_label_set_text(m_lblConnect, LV_SYMBOL_PLAY " Abrir");
        }
    }
}

void SerialTerminalView::createTerminalDisplay(lv_obj_t* parent) {
    m_taTerminal = lv_textarea_create(parent);
    lv_obj_set_size(m_taTerminal, LV_PCT(100), 0);
    lv_obj_set_flex_grow(m_taTerminal, 1);
    
    DefaultTheme::applySunkenCard(m_taTerminal, 8);
    lv_obj_set_style_bg_color(m_taTerminal, lv_color_hex(0x0A0D14), 0);
    lv_obj_set_style_text_color(m_taTerminal, lv_color_hex(0x00FF66), 0);
    lv_obj_set_style_text_font(m_taTerminal, &lv_font_montserrat_12, 0);
    lv_obj_set_style_pad_all(m_taTerminal, 8, 0);
    
    const char* initMsg = "[CBDos Terminal Serial]\n> Hardware en reposo. Selecciona puerto y pulsa Conectar.\n";
    lv_textarea_set_text(m_taTerminal, initMsg);
    m_terminalBuffer = initMsg;
    lv_textarea_set_cursor_click_pos(m_taTerminal, false);
}

void SerialTerminalView::createCommandBar(lv_obj_t* parent) {
    lv_obj_t* cmdContainer = lv_obj_create(parent);
    lv_obj_set_size(cmdContainer, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(cmdContainer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cmdContainer, 0, 0);
    lv_obj_set_style_pad_all(cmdContainer, 0, 0);
    lv_obj_set_flex_flow(cmdContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(cmdContainer, 4, 0);
    DefaultTheme::disableScroll(cmdContainer);

    // Fila 1: Botones rápidos
    lv_obj_t* quickRow = lv_obj_create(cmdContainer);
    lv_obj_set_size(quickRow, LV_PCT(100), 32);
    lv_obj_set_style_bg_opa(quickRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(quickRow, 0, 0);
    lv_obj_set_style_pad_all(quickRow, 0, 0);
    lv_obj_set_flex_flow(quickRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(quickRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    DefaultTheme::disableScroll(quickRow);

    struct QuickKey {
        const char* label;
        const char* bytes;
        size_t len;
    };

    static const QuickKey s_quickKeys[] = {
        {"ENTER", "\r\n", 2},
        {"^C", "\x03", 1},
        {"^Z", "\x1A", 1},
        {"SPACE", " ", 1},
        {"TAB", "\t", 1}
    };

    for (const auto& qk : s_quickKeys) {
        lv_obj_t* btn = lv_button_create(quickRow);
        DefaultTheme::applyButton(btn, 6);
        lv_obj_set_size(btn, 54, 28);
        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, qk.label);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_center(lbl);
        lv_obj_add_event_cb(btn, quickKeyBtnCb, LV_EVENT_CLICKED, (void*)&qk);
    }

    // Fila 2: Input + Enviar + Teclado
    lv_obj_t* inputRow = lv_obj_create(cmdContainer);
    lv_obj_set_size(inputRow, LV_PCT(100), 38);
    lv_obj_set_style_bg_opa(inputRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(inputRow, 0, 0);
    lv_obj_set_style_pad_all(inputRow, 0, 0);
    lv_obj_set_flex_flow(inputRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(inputRow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(inputRow, 4, 0);
    DefaultTheme::disableScroll(inputRow);

    m_inputCmd = lv_textarea_create(inputRow);
    lv_obj_set_flex_grow(m_inputCmd, 1);
    lv_obj_set_height(m_inputCmd, 36);
    DefaultTheme::applyTextArea(m_inputCmd, 8);
    lv_textarea_set_one_line(m_inputCmd, true);
    lv_textarea_set_placeholder_text(m_inputCmd, "Comando...");
    lv_obj_set_style_text_font(m_inputCmd, &lv_font_montserrat_12, 0);
    lv_obj_add_event_cb(m_inputCmd, inputFocusedCb, LV_EVENT_FOCUSED, this);

    m_btnSend = lv_button_create(inputRow);
    DefaultTheme::applyButton(m_btnSend, 8);
    lv_obj_set_size(m_btnSend, 50, 36);
    lv_obj_t* lblSend = lv_label_create(m_btnSend);
    lv_label_set_text(lblSend, LV_SYMBOL_PLAY);
    lv_obj_center(lblSend);
    lv_obj_add_event_cb(m_btnSend, sendBtnCb, LV_EVENT_CLICKED, this);

    m_btnToggleKb = lv_button_create(inputRow);
    DefaultTheme::applyButton(m_btnToggleKb, 8);
    lv_obj_set_size(m_btnToggleKb, 42, 36);
    lv_obj_t* lblKb = lv_label_create(m_btnToggleKb);
    lv_label_set_text(lblKb, LV_SYMBOL_KEYBOARD);
    lv_obj_center(lblKb);
    lv_obj_add_event_cb(m_btnToggleKb, kbToggleBtnCb, LV_EVENT_CLICKED, this);
}

void SerialTerminalView::createKeyboard(lv_obj_t* parent) {
    m_keyboard = lv_keyboard_create(parent);
    lv_obj_set_size(m_keyboard, LV_PCT(100), 160);
    lv_keyboard_set_textarea(m_keyboard, m_inputCmd);
    lv_obj_add_flag(m_keyboard, LV_OBJ_FLAG_HIDDEN);
    m_kbVisible = false;
}

void SerialTerminalView::appendText(const char* text, size_t len) {
    if (!text || len == 0) return;

    // Sanitización RAW limpia: permitir todo ASCII imprimible, \r, \n, \t
    std::string cleanText;
    cleanText.reserve(len);
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)text[i];
        if (c == '\r' || c == '\n' || c == '\t') {
            cleanText.push_back((char)c);
        } else if (c >= 0x20 && c <= 0x7E) {
            cleanText.push_back((char)c);
        } else if (c == 0x1B) {
            // Omitir byte de control ESC para evitar romper el renderizador de fuentes tipográficas
            continue;
        }
    }

    if (cleanText.empty()) return;

    m_terminalBuffer.append(cleanText);
    if (m_terminalBuffer.size() > MAX_TERMINAL_BUFFER_SIZE) {
        size_t excess = m_terminalBuffer.size() - MAX_TERMINAL_BUFFER_SIZE;
        size_t cutPos = m_terminalBuffer.find('\n', excess);
        if (cutPos != std::string::npos) {
            m_terminalBuffer = m_terminalBuffer.substr(cutPos + 1);
        } else {
            m_terminalBuffer = m_terminalBuffer.substr(excess);
        }
    }

    if (m_taTerminal && lv_obj_is_valid(m_taTerminal)) {
        lv_textarea_add_text(m_taTerminal, cleanText.c_str());
        lv_textarea_set_cursor_pos(m_taTerminal, LV_TEXTAREA_CURSOR_LAST);
    }
}

void SerialTerminalView::sendCommand(const std::string& cmd) {
    if (cmd.empty()) return;

    if (!m_isConnected) {
        std::string warn = "\n[WARN] Puerto cerrado. Pulsa Conectar primero.\n";
        appendText(warn.c_str(), warn.size());
        return;
    }

    std::string toSend = cmd + "\r\n";
    cbdos::serial::writeString(toSend);

    std::string echo = "\n[TX] > " + cmd + "\n";
    appendText(echo.c_str(), echo.size());
}

void SerialTerminalView::sendSpecialKey(const char* keyBytes, size_t len) {
    if (!keyBytes || len == 0 || !m_isConnected) return;
    cbdos::serial::write((const uint8_t*)keyBytes, len);
}

void SerialTerminalView::saveLogToSd() {
    if (!cbdos::storage::isSdMounted()) {
        std::string msg = "\n[ERR] MicroSD no insertada o no montada.\n";
        appendText(msg.c_str(), msg.size());
        return;
    }

    char filename[64];
    snprintf(filename, sizeof(filename), "/sdcard/serial_log_%u.txt", (unsigned)cbdos::system::getTimeMs());
    
    bool ok = cbdos::storage::writeFile(filename, m_terminalBuffer);
    if (ok) {
        std::string msg = "\n[OK] Log guardado en: " + std::string(filename) + "\n";
        appendText(msg.c_str(), msg.size());
    } else {
        std::string msg = "\n[ERR] Fallo al escribir en MicroSD.\n";
        appendText(msg.c_str(), msg.size());
    }
}

void SerialTerminalView::timerPollCb(lv_timer_t* timer) {
    SerialTerminalView* self = static_cast<SerialTerminalView*>(lv_timer_get_user_data(timer));
    if (!self) return;

    if (self->m_portsDirty.exchange(false)) {
        self->updatePortList();
        if (self->m_isConnected && !cbdos::serial::isOpen()) {
            self->updateConnectionUi(false);
            std::string msg = "\n[SYS] Dispositivo USB desconectado fisicamente.\n";
            self->appendText(msg.c_str(), msg.size());
        }
    }

    if (!self->m_isConnected) return;

    size_t avail = cbdos::serial::available();
    if (avail > 0) {
        uint8_t buffer[256];
        size_t toRead = std::min(avail, sizeof(buffer) - 1);
        size_t readBytes = cbdos::serial::read(buffer, toRead);
        if (readBytes > 0) {
            buffer[readBytes] = '\0';
            self->appendText((const char*)buffer, readBytes);
        }
    }
}

void SerialTerminalView::connectBtnCb(lv_event_t* e) {
    SerialTerminalView* self = static_cast<SerialTerminalView*>(lv_event_get_user_data(e));
    if (!self) return;

    if (!self->m_isConnected) {
        cbdos::serial::SerialConfig cfg;
        cfg.portId = self->m_selectedPortId;
        cfg.baudrate = self->m_currentBaud;
        cfg.txPin = self->m_currentTxPin;
        cfg.rxPin = self->m_currentRxPin;
        cfg.controlPin = self->m_currentControlPin;

        bool ok = cbdos::serial::open(cfg);
        if (ok) {
            self->updateConnectionUi(true);
            std::string msg = "\n[SYS] Conectado en '" + self->m_selectedPortId + "' (TX:" +
                              std::to_string(self->m_currentTxPin) + " RX:" +
                              std::to_string(self->m_currentRxPin) + ") @ " +
                              std::to_string(self->m_currentBaud) + " bps.\n";
            self->appendText(msg.c_str(), msg.size());
        } else {
            std::string err = "\n[ERR] Fallo al abrir '" + self->m_selectedPortId + "'.\n";
            self->appendText(err.c_str(), err.size());
        }
    } else {
        cbdos::serial::close();
        self->updateConnectionUi(false);
        std::string msg = "\n[SYS] Puerto cerrado. Hardware liberado.\n";
        self->appendText(msg.c_str(), msg.size());
    }
}

void SerialTerminalView::portChangedCb(lv_event_t* e) {
    SerialTerminalView* self = static_cast<SerialTerminalView*>(lv_event_get_user_data(e));
    if (!self) return;

    uint32_t sel = lv_dropdown_get_selected(self->m_ddPort);
    if (sel < self->m_ports.size()) {
        self->m_selectedPortId = self->m_ports[sel].id;
        self->m_currentControlPin = self->m_ports[sel].controlPin;
        if (self->m_ports[sel].defaultTx >= 0) self->m_currentTxPin = self->m_ports[sel].defaultTx;
        if (self->m_ports[sel].defaultRx >= 0) self->m_currentRxPin = self->m_ports[sel].defaultRx;
        self->updateControlPinUi();

        if (self->m_isConnected) {
            cbdos::serial::close();
            cbdos::serial::SerialConfig cfg;
            cfg.portId = self->m_selectedPortId;
            cfg.baudrate = self->m_currentBaud;
            cfg.txPin = self->m_currentTxPin;
            cfg.rxPin = self->m_currentRxPin;
            cfg.controlPin = self->m_currentControlPin;
            cbdos::serial::open(cfg);
        }
    }
}

void SerialTerminalView::baudChangedCb(lv_event_t* e) {
    SerialTerminalView* self = static_cast<SerialTerminalView*>(lv_event_get_user_data(e));
    if (!self) return;

    uint32_t sel = lv_dropdown_get_selected(self->m_ddBaud);
    if (sel < sizeof(s_baudRates) / sizeof(s_baudRates[0])) {
        self->m_currentBaud = s_baudRates[sel];
        if (self->m_isConnected) {
            cbdos::serial::setBaudrate(self->m_currentBaud);
        }
        char msg[64];
        snprintf(msg, sizeof(msg), "\n[SYS] Baudrate configurado: %u bps\n", (unsigned)self->m_currentBaud);
        self->appendText(msg, strlen(msg));
    }
}

void SerialTerminalView::txPinChangedCb(lv_event_t* e) {
    SerialTerminalView* self = static_cast<SerialTerminalView*>(lv_event_get_user_data(e));
    if (!self || !self->m_ddTxPin) return;
    uint32_t sel = lv_dropdown_get_selected(self->m_ddTxPin);
    if (sel < sizeof(s_txPins) / sizeof(s_txPins[0])) {
        self->m_currentTxPin = s_txPins[sel];
        char msg[64];
        snprintf(msg, sizeof(msg), "\n[CONFIG] TX fijado en GPIO %d\n", self->m_currentTxPin);
        self->appendText(msg, strlen(msg));
        if (self->m_isConnected) {
            cbdos::serial::close();
            cbdos::serial::SerialConfig cfg;
            cfg.portId = self->m_selectedPortId;
            cfg.baudrate = self->m_currentBaud;
            cfg.txPin = self->m_currentTxPin;
            cfg.rxPin = self->m_currentRxPin;
            cfg.controlPin = self->m_currentControlPin;
            cbdos::serial::open(cfg);
        }
    }
}

void SerialTerminalView::rxPinChangedCb(lv_event_t* e) {
    SerialTerminalView* self = static_cast<SerialTerminalView*>(lv_event_get_user_data(e));
    if (!self || !self->m_ddRxPin) return;
    uint32_t sel = lv_dropdown_get_selected(self->m_ddRxPin);
    if (sel < sizeof(s_rxPins) / sizeof(s_rxPins[0])) {
        self->m_currentRxPin = s_rxPins[sel];
        char msg[64];
        snprintf(msg, sizeof(msg), "\n[CONFIG] RX fijado en GPIO %d\n", self->m_currentRxPin);
        self->appendText(msg, strlen(msg));
        if (self->m_isConnected) {
            cbdos::serial::close();
            cbdos::serial::SerialConfig cfg;
            cfg.portId = self->m_selectedPortId;
            cfg.baudrate = self->m_currentBaud;
            cfg.txPin = self->m_currentTxPin;
            cfg.rxPin = self->m_currentRxPin;
            cfg.controlPin = self->m_currentControlPin;
            cbdos::serial::open(cfg);
        }
    }
}

void SerialTerminalView::presetJp1BtnCb(lv_event_t* e) {
    SerialTerminalView* self = static_cast<SerialTerminalView*>(lv_event_get_user_data(e));
    if (!self) return;
    self->m_currentTxPin = 32;
    self->m_currentRxPin = 28;
    self->m_currentControlPin = 34;
    if (self->m_ddTxPin) lv_dropdown_set_selected(self->m_ddTxPin, 0);
    if (self->m_ddRxPin) lv_dropdown_set_selected(self->m_ddRxPin, 0);
    const char* msg = "\n[PRESET] JP1 aplicado: TX=32, RX=28, RST=34\n";
    self->appendText(msg, strlen(msg));
    if (self->m_isConnected) {
        cbdos::serial::close();
        cbdos::serial::SerialConfig cfg;
        cfg.portId = self->m_selectedPortId;
        cfg.baudrate = self->m_currentBaud;
        cfg.txPin = 32;
        cfg.rxPin = 28;
        cfg.controlPin = 34;
        cbdos::serial::open(cfg);
    }
}

void SerialTerminalView::presetUart0BtnCb(lv_event_t* e) {
    SerialTerminalView* self = static_cast<SerialTerminalView*>(lv_event_get_user_data(e));
    if (!self) return;
    self->m_currentTxPin = 38;
    self->m_currentRxPin = 37;
    self->m_currentControlPin = -1;
    if (self->m_ddTxPin) lv_dropdown_set_selected(self->m_ddTxPin, 1);
    if (self->m_ddRxPin) lv_dropdown_set_selected(self->m_ddRxPin, 1);
    const char* msg = "\n[PRESET] UART0 MX aplicado: TX=38, RX=37\n";
    self->appendText(msg, strlen(msg));
    if (self->m_isConnected) {
        cbdos::serial::close();
        cbdos::serial::SerialConfig cfg;
        cfg.portId = self->m_selectedPortId;
        cfg.baudrate = self->m_currentBaud;
        cfg.txPin = 38;
        cfg.rxPin = 37;
        cfg.controlPin = -1;
        cbdos::serial::open(cfg);
    }
}

void SerialTerminalView::resetBtnCb(lv_event_t* e) {
    SerialTerminalView* self = static_cast<SerialTerminalView*>(lv_event_get_user_data(e));
    if (!self) return;

    cbdos::serial::pulseControlPin(100);
    std::string msg = "\n[⚡ RST] Pulso de Reset enviado (100ms).\n";
    self->appendText(msg.c_str(), msg.size());
}

void SerialTerminalView::clearBtnCb(lv_event_t* e) {
    SerialTerminalView* self = static_cast<SerialTerminalView*>(lv_event_get_user_data(e));
    if (!self) return;

    self->m_terminalBuffer.clear();
    if (self->m_taTerminal && lv_obj_is_valid(self->m_taTerminal)) {
        lv_textarea_set_text(self->m_taTerminal, "");
    }
}



void SerialTerminalView::saveBtnCb(lv_event_t* e) {
    SerialTerminalView* self = static_cast<SerialTerminalView*>(lv_event_get_user_data(e));
    if (!self) return;
    self->saveLogToSd();
}

void SerialTerminalView::sendBtnCb(lv_event_t* e) {
    SerialTerminalView* self = static_cast<SerialTerminalView*>(lv_event_get_user_data(e));
    if (!self || !self->m_inputCmd) return;

    const char* txt = lv_textarea_get_text(self->m_inputCmd);
    if (txt && strlen(txt) > 0) {
        self->sendCommand(txt);
        lv_textarea_set_text(self->m_inputCmd, "");
    }
}

void SerialTerminalView::kbToggleBtnCb(lv_event_t* e) {
    SerialTerminalView* self = static_cast<SerialTerminalView*>(lv_event_get_user_data(e));
    if (!self || !self->m_keyboard) return;

    self->m_kbVisible = !self->m_kbVisible;
    if (self->m_kbVisible) {
        lv_obj_remove_flag(self->m_keyboard, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(self->m_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}

void SerialTerminalView::quickKeyBtnCb(lv_event_t* e) {
    SerialTerminalView* self = static_cast<SerialTerminalView*>(lv_event_get_user_data(e));
    if (!self) return;

    lv_obj_t* target = (lv_obj_t*)lv_event_get_current_target(e);
    lv_obj_t* label = lv_obj_get_child(target, 0);
    if (label) {
        const char* txt = lv_label_get_text(label);
        if (strcmp(txt, "ENTER") == 0) {
            self->sendSpecialKey("\r\n", 2);
        } else if (strcmp(txt, "^C") == 0) {
            self->sendSpecialKey("\x03", 1);
        } else if (strcmp(txt, "^Z") == 0) {
            self->sendSpecialKey("\x1A", 1);
        } else if (strcmp(txt, "SPACE") == 0) {
            self->sendSpecialKey(" ", 1);
        } else if (strcmp(txt, "TAB") == 0) {
            self->sendSpecialKey("\t", 1);
        }
    }
}

void SerialTerminalView::inputFocusedCb(lv_event_t* e) {
    SerialTerminalView* self = static_cast<SerialTerminalView*>(lv_event_get_user_data(e));
    if (self && self->m_keyboard && !self->m_kbVisible) {
        self->m_kbVisible = true;
        lv_obj_remove_flag(self->m_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}

void SerialTerminalView::onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) {
    if (!m_container || !lv_obj_is_valid(m_container)) return;
    lv_obj_set_style_bg_color(m_container, DefaultTheme::getBgColor(), 0);
}

} // namespace ui
} // namespace cbdos
