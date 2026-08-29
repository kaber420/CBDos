#include "cbdos/ducky.hpp"
#include "cbdos/hid.hpp"
#include "cbdos/storage.hpp"
#include "cbdos/system.hpp"
#include <sstream>
#include <algorithm>

namespace cbdos {
namespace ducky {

DuckyInterpreter& DuckyInterpreter::getInstance() {
    static DuckyInterpreter instance;
    return instance;
}

DuckyInterpreter::DuckyInterpreter()
    : m_currentLine(0),
      m_state(ExecutionState::Idle),
      m_defaultDelayMs(0),
      m_delayUntilMs(0) {}

bool DuckyInterpreter::loadScript(const std::string& scriptPath) {
    stop();
    m_lines.clear();
    m_lastError.clear();

    if (!cbdos::storage::fileExists(scriptPath.c_str())) {
        m_lastError = "Archivo no encontrado: " + scriptPath;
        m_state = ExecutionState::Error;
        return false;
    }

    std::string content = cbdos::storage::readFile(scriptPath.c_str());
    return loadFromString(content);
}

bool DuckyInterpreter::loadFromString(const std::string& scriptContent) {
    stop();
    m_lines.clear();
    m_lastError.clear();

    std::stringstream ss(scriptContent);
    std::string line;
    while (std::getline(ss, line)) {
        // Remover retorno de carro si existe
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        m_lines.push_back(line);
    }

    m_currentLine = 0;
    m_state = ExecutionState::Idle;
    return true;
}

void DuckyInterpreter::run() {
    if (m_lines.empty()) return;
    cbdos::hid::enable();
    m_currentLine = 0;
    m_state = ExecutionState::Running;
    m_delayUntilMs = 0;
}

void DuckyInterpreter::pause() {
    if (m_state == ExecutionState::Running) {
        m_state = ExecutionState::Paused;
    }
}

void DuckyInterpreter::resume() {
    if (m_state == ExecutionState::Paused) {
        m_state = ExecutionState::Running;
    }
}

void DuckyInterpreter::stop() {
    m_state = ExecutionState::Idle;
    m_currentLine = 0;
    m_delayUntilMs = 0;
    cbdos::hid::sendKeyRelease();
    cbdos::hid::disable();
}

bool DuckyInterpreter::step() {
    if (m_state != ExecutionState::Running) {
        return false;
    }

    uint32_t now = cbdos::system::getTimeMs();
    if (now < m_delayUntilMs) {
        return true; // Aún esperando delay
    }

    if (m_currentLine >= m_lines.size()) {
        m_state = ExecutionState::Completed;
        cbdos::hid::sendKeyRelease();
        cbdos::hid::disable();
        return false;
    }

    std::string line = m_lines[m_currentLine++];
    parseAndExecuteLine(line);

    if (m_defaultDelayMs > 0 && m_state == ExecutionState::Running) {
        m_delayUntilMs = cbdos::system::getTimeMs() + m_defaultDelayMs;
    }

    return (m_state == ExecutionState::Running);
}

void DuckyInterpreter::parseAndExecuteLine(const std::string& rawLine) {
    // Ignorar líneas vacías
    size_t firstNonSpace = rawLine.find_first_not_of(" \t");
    if (firstNonSpace == std::string::npos) return;

    std::string line = rawLine.substr(firstNonSpace);

    std::stringstream ss(line);
    std::string command;
    ss >> command;

    std::string upperCmd = command;
    std::transform(upperCmd.begin(), upperCmd.end(), upperCmd.begin(), ::toupper);

    if (upperCmd == "REM") {
        // Comentario, ignorar
        return;
    }

    if (upperCmd == "DEFAULT_DELAY" || upperCmd == "DEFAULTDELAY") {
        uint32_t ms = 0;
        if (ss >> ms) {
            m_defaultDelayMs = ms;
        }
        return;
    }

    if (upperCmd == "DELAY") {
        uint32_t ms = 0;
        if (ss >> ms) {
            m_delayUntilMs = cbdos::system::getTimeMs() + ms;
        }
        return;
    }

    if (upperCmd == "STRING") {
        size_t strPos = line.find_first_of(" \t");
        if (strPos != std::string::npos) {
            std::string payload = line.substr(strPos + 1);
            cbdos::hid::sendString(payload);
        }
        m_lastCommand = line;
        return;
    }

    if (upperCmd == "REPEAT") {
        uint32_t count = 0;
        if (ss >> count && !m_lastCommand.empty()) {
            for (uint32_t i = 0; i < count; ++i) {
                parseAndExecuteLine(m_lastCommand);
            }
        }
        return;
    }

    // Combinaciones y teclas especiales
    uint8_t mod = 0;
    std::vector<uint8_t> keys;

    std::stringstream tokenStream(line);
    std::string token;
    while (tokenStream >> token) {
        uint8_t m = cbdos::hid::nameToModifier(token);
        if (m != cbdos::hid::MOD_NONE) {
            mod |= m;
            continue;
        }

        uint8_t k = cbdos::hid::nameToKeycode(token);
        if (k != cbdos::hid::keycode::KEY_NONE) {
            keys.push_back(k);
        }
    }

    if (keys.empty() && mod != 0) {
        // Solo modificador presionado brevemente (ej. GUI)
        cbdos::hid::sendKeyPress(cbdos::hid::keycode::KEY_NONE, mod);
        cbdos::system::sleepMs(20);
        cbdos::hid::sendKeyRelease();
    } else if (!keys.empty()) {
        cbdos::hid::sendCombo(keys, mod);
    }

    m_lastCommand = line;
}

bool loadFile(const std::string& path) {
    return DuckyInterpreter::getInstance().loadScript(path);
}

void run() {
    DuckyInterpreter::getInstance().run();
}

void stop() {
    DuckyInterpreter::getInstance().stop();
}

bool isRunning() {
    return DuckyInterpreter::getInstance().getState() == ExecutionState::Running;
}

} // namespace ducky
} // namespace cbdos
