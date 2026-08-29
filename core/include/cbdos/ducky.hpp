#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace cbdos {
namespace ducky {

enum class ExecutionState {
    Idle,
    Running,
    Paused,
    Completed,
    Error
};

class DuckyInterpreter {
public:
    static DuckyInterpreter& getInstance();

    bool loadScript(const std::string& scriptPath);
    bool loadFromString(const std::string& scriptContent);

    void run();
    void pause();
    void resume();
    void stop();

    bool step(); // Ejecuta una instrucción o avanza el temporizador
    ExecutionState getState() const { return m_state; }
    size_t getCurrentLine() const { return m_currentLine; }
    size_t getTotalLines() const { return m_lines.size(); }
    const std::string& getLastError() const { return m_lastError; }

    void setDefaultDelay(uint32_t ms) { m_defaultDelayMs = ms; }
    uint32_t getDefaultDelay() const { return m_defaultDelayMs; }

private:
    DuckyInterpreter();
    ~DuckyInterpreter() = default;

    void parseAndExecuteLine(const std::string& line);

    std::vector<std::string> m_lines;
    size_t m_currentLine;
    ExecutionState m_state;
    uint32_t m_defaultDelayMs;
    uint32_t m_delayUntilMs;
    std::string m_lastError;
    std::string m_lastCommand;
};

// Funciones directas de conveniencia
bool loadFile(const std::string& path);
void run();
void stop();
bool isRunning();

} // namespace ducky
} // namespace cbdos
