#pragma once
#include <cstdint>

struct lua_State;


class LuaBridge {
public:
    // Registra todas las APIs nativas del hardware en el estado de Lua
    static void registerAll(lua_State* L);

    // Control de pausa y reanudación del renderizado / touch de LVGL
    static void pauseUI(uint32_t seconds = 0);
    static void resumeUI();
    static bool isUIPaused();
    static bool checkAndClearNeedsRefresh();


private:
    static void registerAudioAPI(lua_State* L);
    static void registerSystemAPI(lua_State* L);
    static void registerGpioAPI(lua_State* L);
    static void registerFsAPI(lua_State* L);
    static void registerGfxAPI(lua_State* L);
};

