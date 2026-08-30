#include "LuaRunner.hpp"
#include "LuaEngine.hpp"
#include "LuaBridge.hpp"
#include <cstdio>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

LuaRunner& LuaRunner::getInstance() {
    static LuaRunner instance;
    return instance;
}

LuaRunner::LuaRunner()
    : _state(LuaRunnerState::IDLE),
      _abortRequested(false),
      _taskHandle(nullptr),
      _isInlineCode(false) {
    _logMutex = cbdos::rtos::createMutex();
    _logBuffer.reserve(64);

    // Conectar el callback de print de Lua con el buffer de logs de LuaRunner
    LuaEngine::getInstance().setPrintCallback([this](const std::string& text) {
        this->appendLog(text);
    });
}

LuaRunner::~LuaRunner() {
    stop();
    if (_logMutex) {
        cbdos::rtos::deleteMutex(_logMutex);
        _logMutex = nullptr;
    }
}

const char* LuaRunner::getStateString() const {
    switch (_state.load()) {
        case LuaRunnerState::IDLE:     return "IDLE";
        case LuaRunnerState::RUNNING:  return "RUNNING";
        case LuaRunnerState::FINISHED: return "FINISHED";
        case LuaRunnerState::STOPPED:  return "STOPPED";
        case LuaRunnerState::ERROR:    return "ERROR";
        default:                       return "UNKNOWN";
    }
}

std::string LuaRunner::getCurrentScript() const {
    if (_isInlineCode) return "Inline Code";
    return _targetScript;
}

void LuaRunner::appendLog(const std::string& log) {
    if (!_logMutex) return;
    if (cbdos::rtos::lockMutex(_logMutex, 100)) {
        if (_logBuffer.size() >= 200) {
            _logBuffer.erase(_logBuffer.begin()); // Descartar los más viejos si se satura
        }
        _logBuffer.push_back(log);
        cbdos::rtos::unlockMutex(_logMutex);
    }
}

bool LuaRunner::drainLogs(std::vector<std::string>& outLogs) {
    if (!_logMutex) return false;
    if (cbdos::rtos::lockMutex(_logMutex, 50)) {
        if (_logBuffer.empty()) {
            cbdos::rtos::unlockMutex(_logMutex);
            return false;
        }
        outLogs.insert(outLogs.end(), _logBuffer.begin(), _logBuffer.end());
        _logBuffer.clear();
        cbdos::rtos::unlockMutex(_logMutex);
        return true;
    }
    return false;
}

void LuaRunner::clearLogs() {
    if (!_logMutex) return;
    if (cbdos::rtos::lockMutex(_logMutex, 100)) {
        _logBuffer.clear();
        cbdos::rtos::unlockMutex(_logMutex);
    }
}

void LuaRunner::hookCb(lua_State* L, lua_Debug* ar) {
    (void)ar;
    if (LuaRunner::getInstance().isAbortRequested()) {
        luaL_error(L, "Ejecución cancelada por el usuario.");
    }
    // Ceder 1 tick al sistema periódicamente para alimentar el Task Watchdog Timer
    static uint32_t s_hookCounter = 0;
    if (++s_hookCounter >= 50) { // Cada 50,000 instrucciones de Lua (~3-5ms de CPU)
        s_hookCounter = 0;
        cbdos::rtos::sleepMs(1);
    }
}

bool LuaRunner::startScript(const std::string& filePath) {
    if (_state.load() == LuaRunnerState::RUNNING) {
        appendLog("[Sistema] Hay un script en ejecución. Deténlo primero.");
        return false;
    }

    _targetScript = filePath;
    _isInlineCode = false;
    _abortRequested = false;
    _state = LuaRunnerState::RUNNING;

    appendLog(std::string("[Sistema] Iniciando script: ") + filePath);

    _taskHandle = cbdos::rtos::createTask(
        luaTask,
        "LuaRunnerTask",
        16384,
        this,
        1,
        0 // Core 0
    );

    if (!_taskHandle) {
        _state = LuaRunnerState::ERROR;
        appendLog("[Error] No se pudo crear la tarea del sistema.");
        return false;
    }

    return true;
}

bool LuaRunner::startString(const std::string& code) {
    if (_state.load() == LuaRunnerState::RUNNING) {
        appendLog("[Sistema] Hay un script en ejecución. Deténlo primero.");
        return false;
    }

    _targetCode = code;
    _isInlineCode = true;
    _abortRequested = false;
    _state = LuaRunnerState::RUNNING;

    appendLog("[Sistema] Ejecutando código dinámico...");

    _taskHandle = cbdos::rtos::createTask(
        luaTask,
        "LuaRunnerTask",
        16384,
        this,
        1,
        0 // Core 0
    );

    if (!_taskHandle) {
        _state = LuaRunnerState::ERROR;
        appendLog("[Error] No se pudo crear la tarea del sistema.");
        return false;
    }

    return true;
}

void LuaRunner::stop() {
    if (_state.load() == LuaRunnerState::RUNNING) {
        _abortRequested = true;
        appendLog("[Sistema] Solicitando parada de ejecución...");
    }
}

void LuaRunner::luaTask(void* param) {
    LuaRunner* runner = (LuaRunner*)param;
    lua_State* L = LuaEngine::getInstance().getRawState();

    if (!L) {
        LuaEngine::getInstance().init(true);
        L = LuaEngine::getInstance().getRawState();
    }

    if (L) {
        // Instalar hook de interrupción cada 1000 instrucciones de bytecode
        lua_sethook(L, hookCb, LUA_MASKCOUNT, 1000);

        bool success = false;
        std::string resultMsg;

        if (runner->_isInlineCode) {
            success = LuaEngine::getInstance().executeString(runner->_targetCode, &resultMsg);
        } else {
            success = LuaEngine::getInstance().executeFile(runner->_targetScript, &resultMsg);
        }

        // Remover hook
        lua_sethook(L, nullptr, 0, 0);

        if (runner->_abortRequested.load()) {
            runner->_state = LuaRunnerState::STOPPED;
            runner->appendLog("[Sistema] Script detenido.");
        } else if (success) {
            runner->_state = LuaRunnerState::FINISHED;
            runner->appendLog("[Sistema] Script finalizado con éxito.");
        } else {
            runner->_state = LuaRunnerState::ERROR;
            runner->appendLog(std::string("[Error] ") + LuaEngine::getInstance().getLastError());
        }
    } else {
        runner->_state = LuaRunnerState::ERROR;
        runner->appendLog("[Error] Motor Lua no inicializado.");
    }

    // Asegurar restauración de la interfaz de LVGL al terminar o fallar el script
    LuaBridge::resumeUI();

    runner->_taskHandle = nullptr;
    cbdos::rtos::deleteTask(nullptr);
}
