#include "LuaREPL.h"
#include "LuaEngine.h"
#include "LuaBridge.h"
#include <Arduino.h>

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

LuaREPL& LuaREPL::getInstance() {
    static LuaREPL instance;
    return instance;
}

LuaREPL::LuaREPL() : initialized(false) {}

void LuaREPL::init() {
    if (initialized) return;

    inputBuffer.reserve(256);
    initialized = true;

    Serial.println("\n┌──────────────────────────────────────────────┐");
    Serial.println("│          CBDos Lua 5.4.7 REPL Console        │");
    Serial.println("│    Escribe '.help' para ver los comandos     │");
    Serial.println("└──────────────────────────────────────────────┘");
    printPrompt();
}

void LuaREPL::printPrompt() {
    Serial.print("lua> ");
}

void LuaREPL::printHelp() {
    Serial.println("\n=== Comandos Internos REPL ===");
    Serial.println("  .help             : Muestra esta ayuda");
    Serial.println("  .mem              : Muestra uso de memoria (Lua GC, PSRAM, Heap)");
    Serial.println("  .clear            : Limpia la terminal");
    Serial.println("  .reset            : Reinicia el motor de Lua y bindings");
    Serial.println("  .run <archivo>    : Ejecuta un script desde la SD (ej: .run /test.lua)");
    Serial.println("\n=== Catálogo de Funciones 'cbdos' ===");
    Serial.println("  [Audio]");
    Serial.println("    cbdos.beep(freq, ms)     - Emite tono en el altavoz nativo");
    Serial.println("    cbdos.play_mp3(path)     - Reproduce MP3/WAV desde SD");
    Serial.println("    cbdos.stop_audio()       - Detiene reproducción de audio");
    Serial.println("    cbdos.set_volume(0-100)  - Ajusta volumen I2S");
    Serial.println("    cbdos.get_volume()       - Obtiene volumen actual");
    Serial.println("  [Sistema]");
    Serial.println("    cbdos.delay(ms)          - Pausa sin bloquear el sistema");
    Serial.println("    cbdos.millis()           - Milisegundos desde el arranque");
    Serial.println("    cbdos.free_psram()       - Bytes libres en PSRAM");
    Serial.println("    cbdos.free_heap()        - Bytes libres en SRAM");
    Serial.println("    cbdos.get_battery()      - Nivel de batería");
    Serial.println("    cbdos.wifi_status()      - Estado de conexión WiFi");
    Serial.println("    cbdos.get_ip()           - Dirección IP asignada");
    Serial.println("  [GPIO]");
    Serial.println("    cbdos.pin_mode(p, mode)  - Configura pin ('input','output','pullup')");
    Serial.println("    cbdos.digital_write(p,v) - Escribe estado lógico (1/0)");
    Serial.println("    cbdos.digital_read(p)    - Lee estado lógico");
    Serial.println("    cbdos.analog_read(p)     - Lee ADC 12 bits");
    Serial.println("  [Archivos]");
    Serial.println("    cbdos.read_file(path)    - Lee archivo completo");
    Serial.println("    cbdos.write_file(p, d)   - Escribe datos en archivo");
    Serial.println("    cbdos.file_exists(path)  - Comprueba existencia");
    Serial.println("    cbdos.list_dir(path)     - Lista contenido de directorio");
    Serial.println();
}

void LuaREPL::printMemory() {
    lua_State* L = LuaEngine::getInstance().getRawState();
    int luaMemKb = L ? lua_gc(L, LUA_GCCOUNT, 0) : 0;

    Serial.println("\n=== Estado de Memoria ===");
    Serial.printf("  Lua GC Memory : %d KB\n", luaMemKb);
    Serial.printf("  Free PSRAM    : %u bytes (%.2f MB)\n", 
                  ESP.getFreePsram(), (float)ESP.getFreePsram() / (1024.0f * 1024.0f));
    Serial.printf("  Free Heap     : %u bytes (%.2f KB)\n\n", 
                  ESP.getFreeHeap(), (float)ESP.getFreeHeap() / 1024.0f);
}

void LuaREPL::handleDotCommand(const std::string& cmd) {
    if (cmd == ".help") {
        printHelp();
    } else if (cmd == ".mem") {
        printMemory();
    } else if (cmd == ".clear") {
        Serial.print("\033[2J\033[H");
    } else if (cmd == ".reset") {
        Serial.println("[LuaREPL] Reiniciando entorno de Lua...");
        LuaEngine::getInstance().deinit();
        if (LuaEngine::getInstance().init(true)) {
            LuaBridge::registerAll(LuaEngine::getInstance().getRawState());
            Serial.println("[LuaREPL] Entorno reiniciado con éxito.");
        } else {
            Serial.println("[LuaREPL] Error al reiniciar Lua.");
        }
    } else if (cmd.rfind(".run ", 0) == 0) {
        std::string path = cmd.substr(5);
        // Trim leading spaces
        size_t first = path.find_first_not_of(" \t");
        if (first != std::string::npos) {
            path = path.substr(first);
        }
        if (!path.empty()) {
            Serial.printf("[LuaREPL] Ejecutando script '%s'...\n", path.c_str());
            std::string result;
            if (!LuaEngine::getInstance().executeFile(path, &result)) {
                Serial.printf("[Error] %s\n", result.c_str());
            }
        } else {
            Serial.println("Uso: .run <ruta/al/archivo.lua>");
        }
    } else {
        Serial.printf("Comando desconocido: '%s'. Escribe '.help' para ver las opciones.\n", cmd.c_str());
    }
}

void LuaREPL::processLine(const std::string& line) {
    // Eliminar espacios iniciales y finales
    size_t start = line.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return; // Línea vacía
    }
    size_t end = line.find_last_not_of(" \t\r\n");
    std::string trimmed = line.substr(start, end - start + 1);

    if (trimmed.empty()) return;

    // Comprobar si es un comando especial con punto
    if (trimmed[0] == '.') {
        handleDotCommand(trimmed);
        return;
    }

    // Auto-evaluación de expresiones:
    // Si la línea no es una declaración de control (local, function, if, for, while, return)
    // intentamos evaluarla primero como "print(<linea>)"
    bool isStatement = (trimmed.rfind("local ", 0) == 0 ||
                        trimmed.rfind("function", 0) == 0 ||
                        trimmed.rfind("if ", 0) == 0 ||
                        trimmed.rfind("for ", 0) == 0 ||
                        trimmed.rfind("while ", 0) == 0 ||
                        trimmed.rfind("repeat", 0) == 0 ||
                        trimmed.rfind("return ", 0) == 0);

    bool evaluated = false;
    if (!isStatement) {
        std::string exprCode = "print(" + trimmed + ")";
        lua_State* L = LuaEngine::getInstance().getRawState();
        if (L) {
            // Intentamos cargar la expresión
            if (luaL_loadstring(L, exprCode.c_str()) == LUA_OK) {
                if (lua_pcall(L, 0, 0, 0) == LUA_OK) {
                    evaluated = true;
                } else {
                    // Limpiar mensaje de error generado por lua_pcall
                    lua_pop(L, 1);
                }
            } else {
                // Limpiar mensaje de error de sintaxis generado por luaL_loadstring
                lua_pop(L, 1);
            }
        }
    }

    if (!evaluated) {
        std::string result;
        if (!LuaEngine::getInstance().executeString(trimmed, &result)) {
            Serial.printf("[Error] %s\n", result.c_str());
        }
    }
}

void LuaREPL::update() {
    while (Serial.available() > 0) {
        char c = (char)Serial.read();

        if (c == '\r' || c == '\n') {
            // Echo newline
            Serial.println();

            if (!inputBuffer.empty()) {
                processLine(inputBuffer);
                inputBuffer.clear();
            }

            printPrompt();
        } else if (c == '\b' || c == 127 || c == 0x08) {
            // Backspace handling
            if (!inputBuffer.empty()) {
                inputBuffer.pop_back();
                Serial.print("\b \b");
            }
        } else if (c >= 32 && c <= 126) {
            // Caracteres imprimibles
            if (inputBuffer.size() < 512) {
                inputBuffer.push_back(c);
                Serial.print(c); // Local echo
            }
        }
    }
}
