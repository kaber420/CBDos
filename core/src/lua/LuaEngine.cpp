#include "LuaEngine.hpp"
#include "LuaBridge.hpp"
#include <esp_heap_caps.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

// Implementación de print() en Lua redirigida a Callback y salida estándar
static int custom_lua_print(lua_State* L) {
    int n = lua_gettop(L); // Número de argumentos
    std::string line;
    for (int i = 1; i <= n; i++) {
        size_t len = 0;
        const char* s = luaL_tolstring(L, i, &len);
        if (s) {
            if (i > 1) line += "\t";
            line.append(s, len);
        }
        lua_pop(L, 1); // Quitar resultado de luaL_tolstring
    }

    printf("[Lua] %s\n", line.c_str());
    LuaEngine::getInstance().emitPrint(line);
    return 0;
}

LuaEngine& LuaEngine::getInstance() {
    static LuaEngine instance;
    return instance;
}

LuaEngine::LuaEngine()
    : L(nullptr), initialized(false), usingPsram(false), printCallback(nullptr) {}

LuaEngine::~LuaEngine() {
    deinit();
}

void* LuaEngine::psramAlloc(void* ud, void* ptr, size_t osize, size_t nsize) {
    (void)ud;
    (void)osize;

    if (nsize == 0) {
        if (ptr) {
            free(ptr);
        }
        return nullptr;
    }

    if (ptr == nullptr) {
        void* p = heap_caps_malloc(nsize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!p) {
            p = malloc(nsize);
        }
        return p;
    } else {
        void* p = heap_caps_realloc(ptr, nsize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!p) {
            p = realloc(ptr, nsize);
        }
        return p;
    }
}

bool LuaEngine::init(bool usePsram) {
    if (initialized) {
        return true;
    }

    usingPsram = usePsram;
    lastError.clear();

    if (usingPsram) {
        L = lua_newstate(psramAlloc, nullptr);
    } else {
        L = luaL_newstate();
    }

    if (!L) {
        lastError = "Error al crear estado de Lua (Fallo de memoria)";
        printf("[LuaEngine] Error: Falló la inicialización de memoria.\n");
        return false;
    }

    // Abrir librerías estándar de Lua
    luaL_openlibs(L);

    // Reemplazar print global con nuestro handler
    lua_register(L, "print", custom_lua_print);

    // Registrar bindings nativos de cbdos.*
    LuaBridge::registerAll(L);

    initialized = true;
    printf("[LuaEngine] Motor Lua 5.4 inicializado con éxito (%s).\n", 
           usingPsram ? "PSRAM" : "SRAM");
    return true;
}

void LuaEngine::deinit() {
    if (L) {
        lua_close(L);
        L = nullptr;
    }
    initialized = false;
}

bool LuaEngine::isInitialized() const {
    return initialized;
}

bool LuaEngine::executeString(const std::string& code, std::string* outResult) {
    if (!initialized && !init(true)) {
        if (outResult) *outResult = lastError;
        return false;
    }

    lastError.clear();

    int status = luaL_dostring(L, code.c_str());
    if (status != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        lastError = err ? err : "Error de ejecución desconocido en Lua";
        lua_pop(L, 1);
        if (outResult) *outResult = lastError;
        printf("[LuaEngine] Error: %s\n", lastError.c_str());
        return false;
    }

    if (outResult) {
        if (lua_gettop(L) > 0) {
            const char* res = lua_tostring(L, -1);
            *outResult = res ? res : "OK";
            lua_pop(L, 1);
        } else {
            *outResult = "OK";
        }
    }

    return true;
}

bool LuaEngine::executeFile(const std::string& filePath, std::string* outResult) {
    if (!initialized && !init(true)) {
        if (outResult) *outResult = lastError;
        return false;
    }

    lastError.clear();

    std::string pathTry = filePath;
    FILE* f = fopen(pathTry.c_str(), "rb");
    if (!f && pathTry.rfind("/sdcard/", 0) != 0) {
        pathTry = std::string("/sdcard/") + (filePath[0] == '/' ? filePath.substr(1) : filePath);
        f = fopen(pathTry.c_str(), "rb");
    }

    if (!f) {
        lastError = "cannot open " + filePath + ": No such file or directory";
        if (outResult) *outResult = lastError;
        printf("[LuaEngine] Error al ejecutar '%s': %s\n", filePath.c_str(), lastError.c_str());
        return false;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    std::string content;
    if (sz > 0) {
        content.resize(sz);
        size_t bytesRead = fread((void*)content.data(), 1, sz, f);
        fclose(f);
        if (bytesRead != (size_t)sz) {
            lastError = "Error reading file: " + pathTry;
            if (outResult) *outResult = lastError;
            return false;
        }
    } else {
        fclose(f);
    }

    // Cargar y compilar el buffer en el estado de Lua
    int status = luaL_loadbuffer(L, content.data(), content.size(), pathTry.c_str());
    if (status != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        lastError = err ? err : "Error de sintaxis en archivo Lua";
        lua_pop(L, 1);
        if (outResult) *outResult = lastError;
        printf("[LuaEngine] Error de sintaxis en '%s': %s\n", pathTry.c_str(), lastError.c_str());
        return false;
    }

    // Ejecutar el script cargado
    status = lua_pcall(L, 0, LUA_MULTRET, 0);
    if (status != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        lastError = err ? err : "Error de ejecución en archivo Lua";
        lua_pop(L, 1);
        if (outResult) *outResult = lastError;
        printf("[LuaEngine] Error al ejecutar '%s': %s\n", pathTry.c_str(), lastError.c_str());
        return false;
    }

    if (outResult) {
        *outResult = "OK";
    }
    return true;
}

std::string LuaEngine::getLastError() const {
    return lastError;
}

void LuaEngine::setPrintCallback(LuaPrintCallback cb) {
    printCallback = cb;
}

lua_State* LuaEngine::getRawState() {
    return L;
}

void LuaEngine::emitPrint(const std::string& text) {
    if (printCallback) {
        printCallback(text);
    }
}
