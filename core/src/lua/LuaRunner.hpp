#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

enum class LuaRunnerState {
    IDLE,
    RUNNING,
    FINISHED,
    STOPPED,
    ERROR
};

class LuaRunner {
public:
    static LuaRunner& getInstance();

    bool startScript(const std::string& filePath);
    bool startString(const std::string& code);
    void stop();

    LuaRunnerState getState() const { return _state.load(); }
    const char* getStateString() const;
    std::string getCurrentScript() const;

    void appendLog(const std::string& log);
    bool drainLogs(std::vector<std::string>& outLogs);
    void clearLogs();

    bool isAbortRequested() const { return _abortRequested.load(); }

private:
    LuaRunner();
    ~LuaRunner();
    LuaRunner(const LuaRunner&) = delete;
    LuaRunner& operator=(const LuaRunner&) = delete;

    static void luaTask(void* param);
    static void hookCb(struct lua_State* L, struct lua_Debug* ar);

    std::atomic<LuaRunnerState> _state;
    std::atomic<bool> _abortRequested;
    TaskHandle_t _taskHandle;
    SemaphoreHandle_t _logMutex;

    std::string _targetScript;
    std::string _targetCode;
    bool _isInlineCode;

    std::vector<std::string> _logBuffer;
};
