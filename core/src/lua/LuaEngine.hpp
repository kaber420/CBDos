#pragma once

#include <string>
#include <functional>
#include <cstdint>

struct lua_State;

using LuaPrintCallback = std::function<void(const std::string&)>;

class LuaEngine {
public:
    static LuaEngine& getInstance();

    bool init(bool usePsram = true);
    void deinit();
    bool isInitialized() const;

    // Ejecuta una cadena de código Lua
    bool executeString(const std::string& code, std::string* outResult = nullptr);

    // Ejecuta un archivo de script (.lua) desde el sistema de archivos
    bool executeFile(const std::string& filePath, std::string* outResult = nullptr);

    // Obtener el último mensaje de error
    std::string getLastError() const;

    // Configurar callback para redireccionar print() de Lua a la UI / Serie
    void setPrintCallback(LuaPrintCallback cb);

    // Acceso al estado nativo de Lua para registrar APIs/módulos
    lua_State* getRawState();

    // Invocar el callback de print interno
    void emitPrint(const std::string& text);

private:
    LuaEngine();
    ~LuaEngine();
    LuaEngine(const LuaEngine&) = delete;
    LuaEngine& operator=(const LuaEngine&) = delete;

    static void* psramAlloc(void* ud, void* ptr, size_t osize, size_t nsize);

    lua_State* L;
    bool initialized;
    bool usingPsram;
    std::string lastError;
    LuaPrintCallback printCallback;
};
