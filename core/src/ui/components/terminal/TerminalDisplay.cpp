#include "TerminalDisplay.hpp"
#include "../../themes/DefaultTheme.h"
#include "cbdos/storage.hpp"
#include "cbdos/system.hpp"
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace cbdos {
namespace ui {

static std::string s_persistentTerminalBuffer = "";

bool TerminalDisplay::create(lv_obj_t* parent) {
    if (!parent) return false;

    m_taTerminal = lv_textarea_create(parent);
    lv_obj_set_size(m_taTerminal, LV_PCT(100), 0);
    lv_obj_set_flex_grow(m_taTerminal, 1);

    DefaultTheme::applySunkenCard(m_taTerminal, 8);
    lv_obj_set_style_bg_color(m_taTerminal, lv_color_hex(0x0A0D14), 0);
    lv_obj_set_style_text_color(m_taTerminal, lv_color_hex(0x00FF66), 0);
    lv_obj_set_style_text_font(m_taTerminal, &lv_font_montserrat_12, 0);
    lv_obj_set_style_pad_all(m_taTerminal, 8, 0);

    if (!s_persistentTerminalBuffer.empty()) {
        lv_textarea_set_text(m_taTerminal, s_persistentTerminalBuffer.c_str());
        m_terminalBuffer = s_persistentTerminalBuffer;
    } else {
        const char* initMsg = "[CBDos Terminal Universal]\n> Selecciona el modo de transporte para comenzar.\n";
        lv_textarea_set_text(m_taTerminal, initMsg);
        m_terminalBuffer = initMsg;
        s_persistentTerminalBuffer = initMsg;
    }

    lv_textarea_set_cursor_pos(m_taTerminal, LV_TEXTAREA_CURSOR_LAST);
    lv_textarea_set_cursor_click_pos(m_taTerminal, false);
    return true;
}

std::string TerminalDisplay::sanitizeAndStripAnsi(const char* data, size_t len) {
    if (!data || len == 0) return "";

    std::string cleanText;
    cleanText.reserve(len);

    for (size_t i = 0; i < len; ) {
        unsigned char c = (unsigned char)data[i];
        if (c == 0x1B) { // Escape ANSI / VT100
            i++;
            if (i < len && data[i] == '[') { // Secuencia CSI
                i++;
                while (i < len && ((unsigned char)data[i] >= 0x20 && (unsigned char)data[i] <= 0x3F)) {
                    i++;
                }
                if (i < len && ((unsigned char)data[i] >= 0x40 && (unsigned char)data[i] <= 0x7E)) {
                    i++;
                }
            } else if (i < len && data[i] == ']') { // Secuencia OSC
                i++;
                while (i < len && data[i] != 0x07 && data[i] != 0x1B) {
                    i++;
                }
                if (i < len && data[i] == 0x07) i++;
            }
        } else if (c == '\r' || c == '\n' || c == '\t') {
            cleanText.push_back((char)c);
            i++;
        } else if (c >= 0x20 && c <= 0x7E) {
            cleanText.push_back((char)c);
            i++;
        } else {
            i++;
        }
    }
    return cleanText;
}

void TerminalDisplay::appendText(const char* text, size_t len) {
    if (!text || len == 0) return;

    std::string cleanText = sanitizeAndStripAnsi(text, len);
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
    s_persistentTerminalBuffer = m_terminalBuffer;

    if (m_isHoldActive) {
        m_holdPendingBuffer.append(cleanText);
    } else {
        if (m_taTerminal && lv_obj_is_valid(m_taTerminal)) {
            lv_textarea_add_text(m_taTerminal, cleanText.c_str());
            lv_textarea_set_cursor_pos(m_taTerminal, LV_TEXTAREA_CURSOR_LAST);
        }
    }
}

void TerminalDisplay::clear() {
    m_terminalBuffer.clear();
    m_holdPendingBuffer.clear();
    s_persistentTerminalBuffer.clear();
    if (m_taTerminal && lv_obj_is_valid(m_taTerminal)) {
        lv_textarea_set_text(m_taTerminal, "");
    }
}

bool TerminalDisplay::toggleHold() {
    m_isHoldActive = !m_isHoldActive;
    if (!m_isHoldActive && !m_holdPendingBuffer.empty()) {
        if (m_taTerminal && lv_obj_is_valid(m_taTerminal)) {
            lv_textarea_add_text(m_taTerminal, m_holdPendingBuffer.c_str());
            lv_textarea_set_cursor_pos(m_taTerminal, LV_TEXTAREA_CURSOR_LAST);
        }
        m_holdPendingBuffer.clear();
    }
    return m_isHoldActive;
}

bool TerminalDisplay::saveLogToSd(std::string& outMessage) {
    if (!cbdos::storage::isSdMounted()) {
        outMessage = "\n[ERR] MicroSD no insertada o no montada.\n";
        return false;
    }

    char filename[64];
    snprintf(filename, sizeof(filename), "/sdcard/terminal_log_%u.txt", (unsigned)cbdos::system::getTimeMs());

    bool ok = cbdos::storage::writeFile(filename, m_terminalBuffer);
    if (ok) {
        outMessage = "\n[OK] Log guardado en: " + std::string(filename) + "\n";
        return true;
    } else {
        outMessage = "\n[ERR] Fallo al escribir en MicroSD.\n";
        return false;
    }
}

} // namespace ui
} // namespace cbdos
