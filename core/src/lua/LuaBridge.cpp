#include "LuaBridge.hpp"
#include "cbdos/audio.hpp"
#include "cbdos/system.hpp"
#include "cbdos/storage.hpp"
#include "cbdos/network.hpp"
#include "cbdos/display.hpp"
#include "cbdos/input.hpp"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <driver/gpio.h>
#include <lvgl.h>

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

// ─────────────────────────────────────────────────────────────────────────────
// UI Control & State
// ─────────────────────────────────────────────────────────────────────────────
static volatile uint32_t s_uiPausedUntil = 0;
static volatile bool s_uiPausedIndefinite = false;
static volatile bool s_needsScreenRefresh = false;

void LuaBridge::pauseUI(uint32_t seconds) {
    if (seconds == 0) {
        s_uiPausedIndefinite = true;
        s_uiPausedUntil = 0;
    } else {
        s_uiPausedIndefinite = false;
        s_uiPausedUntil = cbdos::system::getTimeMs() + (seconds * 1000);
    }
}

void LuaBridge::resumeUI() {
    bool wasPaused = s_uiPausedIndefinite || (s_uiPausedUntil > 0);
    s_uiPausedIndefinite = false;
    s_uiPausedUntil = 0;
    if (wasPaused) {
        s_needsScreenRefresh = true;
    }
}

bool LuaBridge::checkAndClearNeedsRefresh() {
    if (s_needsScreenRefresh) {
        s_needsScreenRefresh = false;
        return true;
    }
    return false;
}

bool LuaBridge::isUIPaused() {
    if (s_uiPausedIndefinite) return true;
    if (s_uiPausedUntil > 0) {
        if (cbdos::system::getTimeMs() < s_uiPausedUntil) {
            return true;
        } else {
            resumeUI();
            return false;
        }
    }
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Audio API
// ─────────────────────────────────────────────────────────────────────────────
static int lua_beep(lua_State* L) {
    lua_Number freq = luaL_checknumber(L, 1);
    lua_Integer ms = luaL_checkinteger(L, 2);
    cbdos::audio::playTone((uint32_t)freq, (uint32_t)ms);
    return 0;
}

static int lua_play_mp3(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    bool ok = cbdos::audio::playFile(path);
    lua_pushboolean(L, ok);
    return 1;
}

static int lua_stop_audio(lua_State* L) {
    cbdos::audio::stop();
    return 0;
}

static int lua_set_volume(lua_State* L) {
    lua_Integer vol = luaL_checkinteger(L, 1);
    cbdos::audio::setVolume((uint8_t)vol);
    return 0;
}

static int lua_get_volume(lua_State* L) {
    lua_pushinteger(L, cbdos::audio::getVolume());
    return 1;
}

// ─────────────────────────────────────────────────────────────────────────────
// System API
// ─────────────────────────────────────────────────────────────────────────────
static int lua_delay(lua_State* L) {
    lua_Integer ms = luaL_checkinteger(L, 1);
    if (ms > 0) {
        cbdos::system::sleepMs((uint32_t)ms);
    }
    return 0;
}

static int lua_millis(lua_State* L) {
    lua_pushinteger(L, (lua_Integer)cbdos::system::getTimeMs());
    return 1;
}

static int lua_free_psram(lua_State* L) {
    lua_pushinteger(L, (lua_Integer)cbdos::system::getFreePsram());
    return 1;
}

static int lua_free_heap(lua_State* L) {
    lua_pushinteger(L, (lua_Integer)cbdos::system::getFreeHeap());
    return 1;
}

static int lua_get_battery(lua_State* L) {
    lua_pushinteger(L, 100);
    return 1;
}

static int lua_wifi_status(lua_State* L) {
    lua_pushboolean(L, cbdos::network::isConnected());
    return 1;
}

static int lua_get_ip(lua_State* L) {
    std::string ip = cbdos::network::getIpAddress();
    if (ip.empty()) {
        ip = "0.0.0.0";
    }
    lua_pushstring(L, ip.c_str());
    return 1;
}

// ─────────────────────────────────────────────────────────────────────────────
// GPIO API
// ─────────────────────────────────────────────────────────────────────────────
static int lua_pin_mode(lua_State* L) {
    int pin = (int)luaL_checkinteger(L, 1);
    const char* modeStr = luaL_checkstring(L, 2);

    gpio_reset_pin((gpio_num_t)pin);
    if (strcmp(modeStr, "input") == 0) {
        gpio_set_direction((gpio_num_t)pin, GPIO_MODE_INPUT);
    } else if (strcmp(modeStr, "pullup") == 0) {
        gpio_set_direction((gpio_num_t)pin, GPIO_MODE_INPUT);
        gpio_set_pull_mode((gpio_num_t)pin, GPIO_PULLUP_ONLY);
    } else if (strcmp(modeStr, "pulldown") == 0) {
        gpio_set_direction((gpio_num_t)pin, GPIO_MODE_INPUT);
        gpio_set_pull_mode((gpio_num_t)pin, GPIO_PULLDOWN_ONLY);
    } else if (strcmp(modeStr, "output") == 0) {
        gpio_set_direction((gpio_num_t)pin, GPIO_MODE_OUTPUT);
    }
    return 0;
}

static int lua_digital_write(lua_State* L) {
    int pin = (int)luaL_checkinteger(L, 1);
    int val = 0;
    if (lua_isboolean(L, 2)) {
        val = lua_toboolean(L, 2) ? 1 : 0;
    } else {
        val = (int)luaL_checkinteger(L, 2) ? 1 : 0;
    }
    gpio_set_level((gpio_num_t)pin, val);
    return 0;
}

static int lua_digital_read(lua_State* L) {
    int pin = (int)luaL_checkinteger(L, 1);
    lua_pushinteger(L, gpio_get_level((gpio_num_t)pin));
    return 1;
}

// ─────────────────────────────────────────────────────────────────────────────
// Filesystem / SD API
// ─────────────────────────────────────────────────────────────────────────────
static int lua_read_file(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    std::string pathTry = path;
    FILE* f = fopen(pathTry.c_str(), "rb");
    if (!f && pathTry.rfind("/sdcard/", 0) != 0) {
        pathTry = std::string("/sdcard/") + (path[0] == '/' ? path + 1 : path);
        f = fopen(pathTry.c_str(), "rb");
    }

    if (!f) {
        lua_pushnil(L);
        lua_pushstring(L, "No se pudo abrir el archivo");
        return 2;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* buf = (char*)malloc(sz + 1);
    if (!buf) {
        fclose(f);
        lua_pushnil(L);
        lua_pushstring(L, "Memoria insuficiente");
        return 2;
    }

    size_t readLen = fread(buf, 1, sz, f);
    buf[readLen] = '\0';
    fclose(f);

    lua_pushlstring(L, buf, readLen);
    free(buf);
    return 1;
}

static int lua_write_file(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    size_t dataLen = 0;
    const char* data = luaL_checklstring(L, 2, &dataLen);

    std::string pathTry = path;
    FILE* f = fopen(pathTry.c_str(), "wb");
    if (!f && pathTry.rfind("/sdcard/", 0) != 0) {
        pathTry = std::string("/sdcard/") + (path[0] == '/' ? path + 1 : path);
        f = fopen(pathTry.c_str(), "wb");
    }

    if (!f) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "No se pudo crear/abrir el archivo para escritura");
        return 2;
    }

    size_t written = fwrite(data, 1, dataLen, f);
    fclose(f);

    lua_pushboolean(L, written == dataLen);
    return 1;
}

static int lua_file_exists(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    bool exists = cbdos::storage::fileExists(path);
    if (!exists && std::string(path).rfind("/sdcard/", 0) != 0) {
        std::string p = std::string("/sdcard/") + (path[0] == '/' ? path + 1 : path);
        exists = cbdos::storage::fileExists(p.c_str());
    }
    lua_pushboolean(L, exists);
    return 1;
}

static int lua_list_dir(lua_State* L) {
    const char* path = luaL_optstring(L, 1, "/sdcard");
    auto entries = cbdos::storage::listDir(path);
    lua_newtable(L);
    for (size_t i = 0; i < entries.size(); i++) {
        lua_newtable(L);
        lua_pushstring(L, entries[i].name.c_str());
        lua_setfield(L, -2, "name");
        lua_pushinteger(L, entries[i].size);
        lua_setfield(L, -2, "size");
        lua_pushboolean(L, entries[i].isDirectory);
        lua_setfield(L, -2, "isDirectory");
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

// ─────────────────────────────────────────────────────────────────────────────
// Graphics API (Exact espOS32 Legacy Canvas & Hardware Integration)
// ─────────────────────────────────────────────────────────────────────────────
#ifdef ARDUINO
#include <JC3248W535.h>
extern JC3248W535_Display& get_s3_display_driver();
extern JC3248W535_Touch& get_s3_touch_driver();

static inline uint16_t toRGB565(uint32_t c) {
    uint8_t r = (c >> 16) & 0xFF;
    uint8_t g = (c >> 8) & 0xFF;
    uint8_t b = c & 0xFF;
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

static int lua_gfx_clear(lua_State* L) {
    uint32_t col = (uint32_t)luaL_optinteger(L, 1, 0x000000);
    auto& drv = get_s3_display_driver();
    if (drv.getCanvas()) {
        drv.getCanvas()->fillScreen(toRGB565(col));
        drv.flush();
    }
    return 0;
}

static int lua_gfx_draw_rect(lua_State* L) {
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    int w = (int)luaL_checkinteger(L, 3);
    int h = (int)luaL_checkinteger(L, 4);
    uint32_t col = (uint32_t)luaL_checkinteger(L, 5);
    bool filled = lua_toboolean(L, 6);
    auto& drv = get_s3_display_driver();
    if (drv.getCanvas()) {
        if (filled) {
            drv.getCanvas()->fillRect(x, y, w, h, toRGB565(col));
        } else {
            drv.getCanvas()->drawRect(x, y, w, h, toRGB565(col));
        }
        drv.flush();
    }
    return 0;
}

static int lua_gfx_draw_circle(lua_State* L) {
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    int r = (int)luaL_checkinteger(L, 3);
    uint32_t col = (uint32_t)luaL_checkinteger(L, 4);
    bool filled = lua_toboolean(L, 5);
    auto& drv = get_s3_display_driver();
    if (drv.getCanvas()) {
        if (filled) {
            drv.getCanvas()->fillCircle(x, y, r, toRGB565(col));
        } else {
            drv.getCanvas()->drawCircle(x, y, r, toRGB565(col));
        }
        drv.flush();
    }
    return 0;
}

static int lua_gfx_draw_line(lua_State* L) {
    int x0 = (int)luaL_checkinteger(L, 1);
    int y0 = (int)luaL_checkinteger(L, 2);
    int x1 = (int)luaL_checkinteger(L, 3);
    int y1 = (int)luaL_checkinteger(L, 4);
    uint32_t col = (uint32_t)luaL_checkinteger(L, 5);
    auto& drv = get_s3_display_driver();
    if (drv.getCanvas()) {
        drv.getCanvas()->drawLine(x0, y0, x1, y1, toRGB565(col));
        drv.flush();
    }
    return 0;
}

static int lua_gfx_draw_text(lua_State* L) {
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    const char* text = luaL_checkstring(L, 3);
    uint32_t col = (uint32_t)luaL_optinteger(L, 4, 0xFFFFFF);
    int size = (int)luaL_optinteger(L, 5, 1);
    auto& drv = get_s3_display_driver();
    if (drv.getCanvas()) {
        drv.getCanvas()->setCursor(x, y);
        drv.getCanvas()->setTextColor(toRGB565(col));
        drv.getCanvas()->setTextSize(size);
        drv.getCanvas()->print(text);
        drv.flush();
    }
    return 0;
}

static int lua_gfx_touch(lua_State* L) {
    ::TouchPoint tp;
    auto& touchDrv = get_s3_touch_driver();
    bool touched = touchDrv.read(tp) && tp.touched;
    lua_newtable(L);
    lua_pushboolean(L, touched);
    lua_setfield(L, -2, "touched");
    lua_pushinteger(L, tp.x);
    lua_setfield(L, -2, "x");
    lua_pushinteger(L, tp.y);
    lua_setfield(L, -2, "y");
    return 1;
}

static int lua_gfx_flush(lua_State* L) {
    (void)L;
    auto& drv = get_s3_display_driver();
    drv.flush();
    return 0;
}

#else
// Fallback para ESP32-P4
static inline uint16_t toRGB565(uint32_t c) {
    uint8_t r = (c >> 16) & 0xFF;
    uint8_t g = (c >> 8) & 0xFF;
    uint8_t b = c & 0xFF;
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

static int lua_gfx_clear(lua_State* L) {
    uint32_t col = (uint32_t)luaL_optinteger(L, 1, 0x000000);
    uint16_t col565 = toRGB565(col);
    auto caps = cbdos::display::getCapabilities();
    uint16_t* fb0 = (uint16_t*)cbdos::display::getFramebuffer(0);
    size_t totalPixels = caps.width * caps.height;
    if (fb0) {
        for (size_t i = 0; i < totalPixels; i++) fb0[i] = col565;
    }
    cbdos::display::flush();
    return 0;
}

static int lua_gfx_draw_rect(lua_State* L) {
    (void)L;
    return 0;
}

static int lua_gfx_draw_circle(lua_State* L) {
    (void)L;
    return 0;
}

static int lua_gfx_draw_line(lua_State* L) {
    (void)L;
    return 0;
}

static int lua_gfx_draw_text(lua_State* L) {
    (void)L;
    return 0;
}

static int lua_gfx_touch(lua_State* L) {
    cbdos::input::TouchPoint tp;
    bool touched = cbdos::input::getTouch(tp);
    lua_newtable(L);
    lua_pushboolean(L, touched && tp.isPressed);
    lua_setfield(L, -2, "touched");
    lua_pushinteger(L, tp.x);
    lua_setfield(L, -2, "x");
    lua_pushinteger(L, tp.y);
    lua_setfield(L, -2, "y");
    return 1;
}

static int lua_gfx_flush(lua_State* L) {
    (void)L;
    cbdos::display::flush();
    return 0;
}
#endif

static int lua_gfx_width(lua_State* L) {
    auto caps = cbdos::display::getCapabilities();
    lua_pushinteger(L, caps.width);
    return 1;
}

static int lua_gfx_height(lua_State* L) {
    auto caps = cbdos::display::getCapabilities();
    lua_pushinteger(L, caps.height);
    return 1;
}

static int lua_gfx_rgb(lua_State* L) {
    int r = (int)luaL_checkinteger(L, 1);
    int g = (int)luaL_checkinteger(L, 2);
    int b = (int)luaL_checkinteger(L, 3);
    uint32_t rgb = ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF);
    lua_pushinteger(L, rgb);
    return 1;
}

static int lua_gfx_pause_ui(lua_State* L) {
    uint32_t seconds = (uint32_t)luaL_optinteger(L, 1, 0);
    LuaBridge::pauseUI(seconds);
    return 0;
}

static int lua_gfx_resume_ui(lua_State* L) {
    LuaBridge::resumeUI();
    return 0;
}

static int lua_gfx_is_ui_paused(lua_State* L) {
    lua_pushboolean(L, LuaBridge::isUIPaused());
    return 1;
}

void LuaBridge::registerGfxAPI(lua_State* L) {
    // Subtabla cbdos.gfx
    lua_newtable(L);
    lua_pushcfunction(L, lua_gfx_clear);
    lua_setfield(L, -2, "clear");
    lua_pushcfunction(L, lua_gfx_draw_rect);
    lua_setfield(L, -2, "draw_rect");
    lua_pushcfunction(L, lua_gfx_draw_circle);
    lua_setfield(L, -2, "draw_circle");
    lua_pushcfunction(L, lua_gfx_draw_line);
    lua_setfield(L, -2, "draw_line");
    lua_pushcfunction(L, lua_gfx_draw_text);
    lua_setfield(L, -2, "draw_text");
    lua_pushcfunction(L, lua_gfx_flush);
    lua_setfield(L, -2, "flush");
    lua_pushcfunction(L, lua_gfx_touch);
    lua_setfield(L, -2, "touch");
    lua_pushcfunction(L, lua_gfx_rgb);
    lua_setfield(L, -2, "rgb");
    lua_pushcfunction(L, lua_gfx_width);
    lua_setfield(L, -2, "width");
    lua_pushcfunction(L, lua_gfx_height);
    lua_setfield(L, -2, "height");
    lua_pushcfunction(L, lua_gfx_pause_ui);
    lua_setfield(L, -2, "pause_ui");
    lua_pushcfunction(L, lua_gfx_resume_ui);
    lua_setfield(L, -2, "resume_ui");
    lua_pushcfunction(L, lua_gfx_is_ui_paused);
    lua_setfield(L, -2, "is_ui_paused");
    lua_setfield(L, -2, "gfx");

    // Accesos directos en cbdos.*
    lua_pushcfunction(L, lua_gfx_pause_ui);
    lua_setfield(L, -2, "pause_ui");
    lua_pushcfunction(L, lua_gfx_resume_ui);
    lua_setfield(L, -2, "resume_ui");
}

void LuaBridge::registerAudioAPI(lua_State* L) {
    lua_newtable(L);
    lua_pushcfunction(L, lua_beep);
    lua_setfield(L, -2, "beep");
    lua_pushcfunction(L, lua_play_mp3);
    lua_setfield(L, -2, "play_mp3");
    lua_pushcfunction(L, lua_play_mp3);
    lua_setfield(L, -2, "play_file");
    lua_pushcfunction(L, lua_stop_audio);
    lua_setfield(L, -2, "stop");
    lua_pushcfunction(L, lua_set_volume);
    lua_setfield(L, -2, "set_volume");
    lua_pushcfunction(L, lua_get_volume);
    lua_setfield(L, -2, "get_volume");
    lua_setfield(L, -2, "audio");

    // Accesos directos en cbdos.*
    lua_pushcfunction(L, lua_beep);
    lua_setfield(L, -2, "beep");
    lua_pushcfunction(L, lua_play_mp3);
    lua_setfield(L, -2, "play_mp3");
    lua_pushcfunction(L, lua_play_mp3);
    lua_setfield(L, -2, "play_file");
    lua_pushcfunction(L, lua_stop_audio);
    lua_setfield(L, -2, "stop_audio");
    lua_pushcfunction(L, lua_set_volume);
    lua_setfield(L, -2, "set_volume");
    lua_pushcfunction(L, lua_get_volume);
    lua_setfield(L, -2, "get_volume");
}

void LuaBridge::registerSystemAPI(lua_State* L) {
    lua_newtable(L);
    lua_pushcfunction(L, lua_delay);
    lua_setfield(L, -2, "delay");
    lua_pushcfunction(L, lua_delay);
    lua_setfield(L, -2, "sleep");
    lua_pushcfunction(L, lua_millis);
    lua_setfield(L, -2, "millis");
    lua_pushcfunction(L, lua_millis);
    lua_setfield(L, -2, "get_time");
    lua_pushcfunction(L, lua_free_psram);
    lua_setfield(L, -2, "free_psram");
    lua_pushcfunction(L, lua_free_heap);
    lua_setfield(L, -2, "free_heap");
    lua_pushcfunction(L, lua_get_battery);
    lua_setfield(L, -2, "get_battery");
    lua_pushcfunction(L, lua_wifi_status);
    lua_setfield(L, -2, "wifi_status");
    lua_pushcfunction(L, lua_get_ip);
    lua_setfield(L, -2, "get_ip");
    lua_setfield(L, -2, "system");

    // Accesos directos en cbdos.*
    lua_pushcfunction(L, lua_delay);
    lua_setfield(L, -2, "delay");
    lua_pushcfunction(L, lua_delay);
    lua_setfield(L, -2, "sleep");
    lua_pushcfunction(L, lua_millis);
    lua_setfield(L, -2, "millis");
    lua_pushcfunction(L, lua_free_psram);
    lua_setfield(L, -2, "free_psram");
    lua_pushcfunction(L, lua_free_heap);
    lua_setfield(L, -2, "free_heap");
    lua_pushcfunction(L, lua_get_battery);
    lua_setfield(L, -2, "get_battery");
    lua_pushcfunction(L, lua_wifi_status);
    lua_setfield(L, -2, "wifi_status");
    lua_pushcfunction(L, lua_get_ip);
    lua_setfield(L, -2, "get_ip");
}

void LuaBridge::registerGpioAPI(lua_State* L) {
    lua_newtable(L);
    lua_pushcfunction(L, lua_pin_mode);
    lua_setfield(L, -2, "pin_mode");
    lua_pushcfunction(L, lua_digital_write);
    lua_setfield(L, -2, "digital_write");
    lua_pushcfunction(L, lua_digital_read);
    lua_setfield(L, -2, "digital_read");
    lua_setfield(L, -2, "gpio");

    // Accesos directos en cbdos.*
    lua_pushcfunction(L, lua_pin_mode);
    lua_setfield(L, -2, "pin_mode");
    lua_pushcfunction(L, lua_digital_write);
    lua_setfield(L, -2, "digital_write");
    lua_pushcfunction(L, lua_digital_read);
    lua_setfield(L, -2, "digital_read");
}

void LuaBridge::registerFsAPI(lua_State* L) {
    lua_newtable(L);
    lua_pushcfunction(L, lua_read_file);
    lua_setfield(L, -2, "read_file");
    lua_pushcfunction(L, lua_write_file);
    lua_setfield(L, -2, "write_file");
    lua_pushcfunction(L, lua_file_exists);
    lua_setfield(L, -2, "file_exists");
    lua_pushcfunction(L, lua_list_dir);
    lua_setfield(L, -2, "list_dir");
    lua_setfield(L, -2, "fs");

    // Accesos directos en cbdos.*
    lua_pushcfunction(L, lua_read_file);
    lua_setfield(L, -2, "read_file");
    lua_pushcfunction(L, lua_write_file);
    lua_setfield(L, -2, "write_file");
    lua_pushcfunction(L, lua_file_exists);
    lua_setfield(L, -2, "file_exists");
    lua_pushcfunction(L, lua_list_dir);
    lua_setfield(L, -2, "list_dir");
}

void LuaBridge::registerAll(lua_State* L) {
    if (!L) return;

    // Crear la tabla global cbdos
    lua_newtable(L);

    registerAudioAPI(L);
    registerSystemAPI(L);
    registerGpioAPI(L);
    registerFsAPI(L);
    registerGfxAPI(L);

    // Guardar tabla como global "cbdos"
    lua_setglobal(L, "cbdos");

    printf("[LuaBridge] Bindings nativos 'cbdos.*' y 'cbdos.gfx.*' registrados con éxito.\n");
}
