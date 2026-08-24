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

SerialTerminalView::SerialTerminalView()
    : BaseView("SerialTerminal"),
      m_isPaused(false),
      m_kbVisible(false),
      m_currentBaud(115200) {
    m_currentTxPin = cbdos::uart::getDefaultTxPin();
    m_currentRxPin = cbdos::uart::getDefaultRxPin();
    m_currentBaud = cbdos::uart::getDefaultBaudrate();
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

    // Inicializar UART si aún no está activa
    if (!cbdos::uart::isInitialized()) {
        cbdos::uart::init(m_currentTxPin, m_currentRxPin, m_currentBaud);
    }

    createToolbar(m_container);
    createTerminalDisplay(m_container);
    createCommandBar(m_container);
    createKeyboard(m_container);

    // Timer de polling a 25ms para recibir datos UART de forma fluida
    m_pollTimer = lv_timer_create(timerPollCb, 25, this);

    return true;
}

void SerialTerminalView::onDestroy() {
    if (m_pollTimer) {
        lv_timer_delete(m_pollTimer);
        m_pollTimer = nullptr;
    }
    cbdos::uart::deinit();
    BaseView::onDestroy();
}

void SerialTerminalView::onShow() {
    BaseView::onShow();
    HeaderBar& hb = UIManager::getInstance().getHeaderBar();
    hb.setTitle("Terminal UART");
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
    lv_obj_t* toolbar = lv_obj_create(parent);
    lv_obj_set_size(toolbar, LV_PCT(100), LV_SIZE_CONTENT);
    DefaultTheme::applyRaisedCard(toolbar, 10);
    lv_obj_set_style_pad_all(toolbar, 4, 0);
    lv_obj_set_flex_flow(toolbar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(toolbar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    DefaultTheme::disableScroll(toolbar);

    // 1. Selector de Baudrate
    m_ddBaud = lv_dropdown_create(toolbar);
    lv_dropdown_set_options(m_ddBaud, "9600\n19200\n38400\n57600\n115200\n230400\n460800\n921600");
    lv_dropdown_set_selected(m_ddBaud, 4); // 115200 por defecto
    lv_obj_set_width(m_ddBaud, 90);
    lv_obj_set_style_text_font(m_ddBaud, &lv_font_montserrat_12, 0);
    lv_obj_add_event_cb(m_ddBaud, baudChangedCb, LV_EVENT_VALUE_CHANGED, this);

    // 2. Selector de Presets de Pines
    const auto& presets = cbdos::uart::getPinPresets();
    m_ddPreset = lv_dropdown_create(toolbar);
    std::string presetOptions = "";
    for (size_t i = 0; i < presets.size(); i++) {
        presetOptions += presets[i].name;
        if (i + 1 < presets.size()) presetOptions += "\n";
    }
    if (presetOptions.empty()) presetOptions = "Default";
    lv_dropdown_set_options(m_ddPreset, presetOptions.c_str());
    lv_dropdown_set_selected(m_ddPreset, 0);
    lv_obj_set_width(m_ddPreset, 110);
    lv_obj_set_style_text_font(m_ddPreset, &lv_font_montserrat_12, 0);
    lv_obj_add_event_cb(m_ddPreset, presetChangedCb, LV_EVENT_VALUE_CHANGED, this);

    // 3. Botón Pausa / Hold
    m_btnPause = lv_button_create(toolbar);
    DefaultTheme::applyButton(m_btnPause, 8);
    lv_obj_set_size(m_btnPause, 50, 32);
    m_lblPause = lv_label_create(m_btnPause);
    lv_label_set_text(m_lblPause, LV_SYMBOL_PAUSE);
    lv_obj_center(m_lblPause);
    lv_obj_add_event_cb(m_btnPause, pauseBtnCb, LV_EVENT_CLICKED, this);

    // 4. Botón Limpiar (Clear)
    m_btnClear = lv_button_create(toolbar);
    DefaultTheme::applyButton(m_btnClear, 8);
    lv_obj_set_size(m_btnClear, 40, 32);
    lv_obj_t* lblClear = lv_label_create(m_btnClear);
    lv_label_set_text(lblClear, LV_SYMBOL_TRASH);
    lv_obj_center(lblClear);
    lv_obj_add_event_cb(m_btnClear, clearBtnCb, LV_EVENT_CLICKED, this);

    // 5. Botón Guardar en SD
    m_btnSave = lv_button_create(toolbar);
    DefaultTheme::applyButton(m_btnSave, 8);
    lv_obj_set_size(m_btnSave, 40, 32);
    lv_obj_t* lblSave = lv_label_create(m_btnSave);
    lv_label_set_text(lblSave, LV_SYMBOL_SD_CARD);
    lv_obj_center(lblSave);
    lv_obj_add_event_cb(m_btnSave, saveBtnCb, LV_EVENT_CLICKED, this);
}

void SerialTerminalView::createTerminalDisplay(lv_obj_t* parent) {
    // Área de Texto de la Terminal (Oscura estilo monitor serie)
    m_taTerminal = lv_textarea_create(parent);
    lv_obj_set_size(m_taTerminal, LV_PCT(100), 0);
    lv_obj_set_flex_grow(m_taTerminal, 1);
    
    DefaultTheme::applySunkenCard(m_taTerminal, 8);
    lv_obj_set_style_bg_color(m_taTerminal, lv_color_hex(0x0A0D14), 0);
    lv_obj_set_style_text_color(m_taTerminal, lv_color_hex(0x00FF66), 0); // Verde terminal clásico
    lv_obj_set_style_text_font(m_taTerminal, &lv_font_montserrat_12, 0);
    lv_obj_set_style_pad_all(m_taTerminal, 8, 0);
    
    lv_textarea_set_text(m_taTerminal, "[CBDos Serial Terminal Initialized]\n> Ready.\n");
    m_terminalBuffer = "[CBDos Serial Terminal Initialized]\n> Ready.\n";
    lv_textarea_set_cursor_click_pos(m_taTerminal, false);
}

void SerialTerminalView::createCommandBar(lv_obj_t* parent) {
    // Fila inferior de comandos y accesos directos
    lv_obj_t* cmdContainer = lv_obj_create(parent);
    lv_obj_set_size(cmdContainer, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(cmdContainer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cmdContainer, 0, 0);
    lv_obj_set_style_pad_all(cmdContainer, 0, 0);
    lv_obj_set_flex_flow(cmdContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(cmdContainer, 4, 0);
    DefaultTheme::disableScroll(cmdContainer);

    // Fila 1: Botones rápidos (Enter, Ctrl+C, Ctrl+Z, Space, Tab)
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

    // Fila 2: Input de texto + Botón Enviar + Botón Teclado
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
    lv_textarea_set_placeholder_text(m_inputCmd, "Comando / Payload...");
    lv_obj_set_style_text_font(m_inputCmd, &lv_font_montserrat_12, 0);
    lv_obj_add_event_cb(m_inputCmd, inputFocusedCb, LV_EVENT_FOCUSED, this);

    // Botón Enviar
    m_btnSend = lv_button_create(inputRow);
    DefaultTheme::applyButton(m_btnSend, 8);
    lv_obj_set_size(m_btnSend, 50, 36);
    lv_obj_t* lblSend = lv_label_create(m_btnSend);
    lv_label_set_text(lblSend, LV_SYMBOL_PLAY);
    lv_obj_center(lblSend);
    lv_obj_add_event_cb(m_btnSend, sendBtnCb, LV_EVENT_CLICKED, this);

    // Botón Teclado On/Off
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
    if (!text || len == 0 || m_isPaused) return;

    m_terminalBuffer.append(text, len);
    if (m_terminalBuffer.size() > MAX_TERMINAL_BUFFER_SIZE) {
        size_t excess = m_terminalBuffer.size() - MAX_TERMINAL_BUFFER_SIZE;
        // Cortar desde el inicio hasta el próximo salto de línea
        size_t cutPos = m_terminalBuffer.find('\n', excess);
        if (cutPos != std::string::npos) {
            m_terminalBuffer = m_terminalBuffer.substr(cutPos + 1);
        } else {
            m_terminalBuffer = m_terminalBuffer.substr(excess);
        }
    }

    if (m_taTerminal && lv_obj_is_valid(m_taTerminal)) {
        lv_textarea_set_text(m_taTerminal, m_terminalBuffer.c_str());
        lv_textarea_set_cursor_pos(m_taTerminal, LV_TEXTAREA_CURSOR_LAST);
    }
}

void SerialTerminalView::sendCommand(const std::string& cmd) {
    if (cmd.empty()) return;
    std::string toSend = cmd + "\r\n";
    cbdos::uart::writeString(toSend);

    // Eco local en la terminal
    std::string echo = "\n[TX] > " + cmd + "\n";
    appendText(echo.c_str(), echo.size());
}

void SerialTerminalView::sendSpecialKey(const char* keyBytes, size_t len) {
    if (!keyBytes || len == 0) return;
    cbdos::uart::write((const uint8_t*)keyBytes, len);
}

void SerialTerminalView::saveLogToSd() {
    if (!cbdos::storage::isSdMounted()) {
        std::string msg = "\n[ERR] MicroSD no insertada o no montada.\n";
        appendText(msg.c_str(), msg.size());
        return;
    }

    char filename[64];
    snprintf(filename, sizeof(filename), "/sdcard/uart_log_%u.txt", (unsigned)cbdos::system::getTimeMs());
    
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
    if (!self || self->m_isPaused) return;

    size_t avail = cbdos::uart::available();
    if (avail > 0) {
        uint8_t buffer[256];
        size_t toRead = std::min(avail, sizeof(buffer) - 1);
        size_t readBytes = cbdos::uart::read(buffer, toRead);
        if (readBytes > 0) {
            buffer[readBytes] = '\0';
            self->appendText((const char*)buffer, readBytes);
        }
    }
}

void SerialTerminalView::baudChangedCb(lv_event_t* e) {
    SerialTerminalView* self = static_cast<SerialTerminalView*>(lv_event_get_user_data(e));
    if (!self) return;

    uint32_t sel = lv_dropdown_get_selected(self->m_ddBaud);
    if (sel < sizeof(s_baudRates) / sizeof(s_baudRates[0])) {
        self->m_currentBaud = s_baudRates[sel];
        cbdos::uart::setBaudrate(self->m_currentBaud);
        
        char msg[64];
        snprintf(msg, sizeof(msg), "\n[UART] Baudrate cambiado a %u bps\n", (unsigned)self->m_currentBaud);
        self->appendText(msg, strlen(msg));
    }
}

void SerialTerminalView::presetChangedCb(lv_event_t* e) {
    SerialTerminalView* self = static_cast<SerialTerminalView*>(lv_event_get_user_data(e));
    if (!self) return;

    uint32_t sel = lv_dropdown_get_selected(self->m_ddPreset);
    const auto& presets = cbdos::uart::getPinPresets();
    if (sel < presets.size()) {
        self->m_currentTxPin = presets[sel].txPin;
        self->m_currentRxPin = presets[sel].rxPin;
        cbdos::uart::init(self->m_currentTxPin, self->m_currentRxPin, self->m_currentBaud);
        
        char msg[96];
        snprintf(msg, sizeof(msg), "\n[UART] Preset '%s' (TX:%d, RX:%d)\n", 
                 presets[sel].name.c_str(), self->m_currentTxPin, self->m_currentRxPin);
        self->appendText(msg, strlen(msg));
    }
}

void SerialTerminalView::clearBtnCb(lv_event_t* e) {
    SerialTerminalView* self = static_cast<SerialTerminalView*>(lv_event_get_user_data(e));
    if (!self) return;

    self->m_terminalBuffer.clear();
    if (self->m_taTerminal && lv_obj_is_valid(self->m_taTerminal)) {
        lv_textarea_set_text(self->m_taTerminal, "");
    }
}

void SerialTerminalView::pauseBtnCb(lv_event_t* e) {
    SerialTerminalView* self = static_cast<SerialTerminalView*>(lv_event_get_user_data(e));
    if (!self) return;

    self->m_isPaused = !self->m_isPaused;
    if (self->m_lblPause && lv_obj_is_valid(self->m_lblPause)) {
        lv_label_set_text(self->m_lblPause, self->m_isPaused ? LV_SYMBOL_PLAY : LV_SYMBOL_PAUSE);
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

    // Recuperar info del botón
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
