#pragma once
#include <lvgl.h>
#include <string>
#include <cstddef>

namespace cbdos {
namespace ui {

class TerminalDisplay {
public:
    TerminalDisplay() = default;
    ~TerminalDisplay() = default;

    bool create(lv_obj_t* parent);
    void appendText(const char* text, size_t len);
    void clear();
    bool toggleHold();
    bool isHoldActive() const { return m_isHoldActive; }
    bool saveLogToSd(std::string& outMessage);
    lv_obj_t* getObject() const { return m_taTerminal; }

    static std::string sanitizeAndStripAnsi(const char* data, size_t len);

private:
    lv_obj_t* m_taTerminal = nullptr;
    bool m_isHoldActive = false;
    std::string m_terminalBuffer;
    std::string m_holdPendingBuffer;

    static constexpr size_t MAX_TERMINAL_BUFFER_SIZE = 8192;
};

} // namespace ui
} // namespace cbdos
