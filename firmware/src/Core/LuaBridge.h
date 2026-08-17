#pragma once

struct lua_State;

class LuaBridge {
public:
    // Registra todas las APIs nativas del hardware en el estado de Lua
    static void registerAll(lua_State* L);

private:
    static void registerAudioAPI(lua_State* L);
    static void registerSystemAPI(lua_State* L);
    static void registerGpioAPI(lua_State* L);
    static void registerFsAPI(lua_State* L);
    static void registerGfxAPI(lua_State* L);
};
