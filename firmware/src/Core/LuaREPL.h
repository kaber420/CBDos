#pragma once

#include <string>

class LuaREPL {
public:
    static LuaREPL& getInstance();

    void init();
    void update();

    // Ejecuta una línea o comando de forma programática (útil también para terminal en pantalla)
    void processLine(const std::string& line);

private:
    LuaREPL();
    ~LuaREPL() = default;
    LuaREPL(const LuaREPL&) = delete;
    LuaREPL& operator=(const LuaREPL&) = delete;

    void printPrompt();
    void printHelp();
    void printMemory();
    void handleDotCommand(const std::string& cmd);

    std::string inputBuffer;
    bool initialized;
};
