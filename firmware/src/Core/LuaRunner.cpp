#include "LuaRunner.h"
#include "LuaEngine.h"
#include <Arduino.h>

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
    _logMutex = xSemaphoreCreateMutex();
    _logBuffer.reserve(64);

    // Conectar el callback de print de Lua con el buffer de logs de LuaRunner
    LuaEngine::getInstance().setPrintCallback([this](const std::string& text) {
        this->appendLog(text);
    });
}

LuaRunner::~LuaRunner() {
    stop();
    if (_logMutex) {
        vSemaphoreDelete(_logMutex);
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
    if (xSemaphoreTake(_logMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (_logBuffer.size() >= 200) {
            _logBuffer.erase(_logBuffer.begin()); // Descartar los más viejos si se satura
        }
        _logBuffer.push_back(log);
        xSemaphoreGive(_logMutex);
    }
}

bool LuaRunner::drainLogs(std::vector<std::string>& outLogs) {
    if (!_logMutex) return false;
    if (xSemaphoreTake(_logMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (_logBuffer.empty()) {
            xSemaphoreGive(_logMutex);
            return false;
        }
        outLogs.insert(outLogs.end(), _logBuffer.begin(), _logBuffer.end());
        _logBuffer.clear();
        xSemaphoreGive(_logMutex);
        return true;
    }
    return false;
}

void LuaRunner::clearLogs() {
    if (!_logMutex) return;
    if (xSemaphoreTake(_logMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        _logBuffer.clear();
        xSemaphoreGive(_logMutex);
    }
}

void LuaRunner::hookCb(lua_State* L, lua_Debug* ar) {
    (void)ar;
    if (LuaRunner::getInstance().isAbortRequested()) {
        luaL_error(L, "Ejecución cancelada por el usuario.");
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

    BaseType_t res = xTaskCreatePinnedToCore(
        luaTask,
        "LuaRunnerTask",
        16384,
        this,
        1,
        &_taskHandle,
        0 // Core 0
    );

    if (res != pdPASS) {
        _state = LuaRunnerState::ERROR;
        appendLog("[Error] No se pudo crear la tarea de FreeRTOS.");
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

    BaseType_t res = xTaskCreatePinnedToCore(
        luaTask,
        "LuaRunnerTask",
        16384,
        this,
        1,
        &_taskHandle,
        0 // Core 0
    );

    if (res != pdPASS) {
        _state = LuaRunnerState::ERROR;
        appendLog("[Error] No se pudo crear la tarea de FreeRTOS.");
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
        // Instalar hook de interrupción cada 500 instrucciones de bytecode
        lua_sethook(L, hookCb, LUA_MASKCOUNT, 500);

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

    runner->_taskHandle = nullptr;
    vTaskDelete(NULL);
}
