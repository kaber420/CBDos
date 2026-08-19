#include "LuaBridge.h"
#include "NativeAudioDriver.h"
#include "SystemStateAPI.h"
#include "LVFS_Driver.h"
#include <Arduino.h>
#include <WiFi.h>
#include <SD.h>
#include <cstring>

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

// ─────────────────────────────────────────────────────────────────────────────
// Audio API
// ─────────────────────────────────────────────────────────────────────────────
static int lua_beep(lua_State* L) {
    lua_Number freq = luaL_checknumber(L, 1);
    lua_Integer ms = luaL_checkinteger(L, 2);
    NativeAudioDriver::getInstance().playTone((float)freq, (int)ms);
    return 0;
}

static int lua_play_mp3(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    NativeAudioDriver::getInstance().playMP3(path);
    return 0;
}

static int lua_stop_audio(lua_State* L) {
    NativeAudioDriver::getInstance().stop();
    return 0;
}

static int lua_set_volume(lua_State* L) {
    lua_Integer vol = luaL_checkinteger(L, 1);
    NativeAudioDriver::setVolume((uint8_t)vol);
    return 0;
}

static int lua_get_volume(lua_State* L) {
    lua_pushinteger(L, NativeAudioDriver::getVolume());
    return 1;
}

// ─────────────────────────────────────────────────────────────────────────────
// System API
// ─────────────────────────────────────────────────────────────────────────────
static int lua_delay(lua_State* L) {
    lua_Integer ms = luaL_checkinteger(L, 1);
    if (ms > 0) {
        vTaskDelay(pdMS_TO_TICKS(ms));
    }
    return 0;
}

static int lua_millis(lua_State* L) {
    lua_pushinteger(L, (lua_Integer)millis());
    return 1;
}

static int lua_free_psram(lua_State* L) {
    lua_pushinteger(L, (lua_Integer)ESP.getFreePsram());
    return 1;
}

static int lua_free_heap(lua_State* L) {
    lua_pushinteger(L, (lua_Integer)ESP.getFreeHeap());
    return 1;
}

static int lua_get_battery(lua_State* L) {
    // Estimación de batería o valor fijo para sistema en desarrollo
    lua_pushinteger(L, 100);
    return 1;
}

static int lua_wifi_status(lua_State* L) {
    lua_pushboolean(L, SystemStateAPI::isWifiConnected());
    return 1;
}

static int lua_get_ip(lua_State* L) {
    if (WiFi.status() == WL_CONNECTED) {
        lua_pushstring(L, WiFi.localIP().toString().c_str());
    } else {
        lua_pushstring(L, "0.0.0.0");
    }
    return 1;
}

// ─────────────────────────────────────────────────────────────────────────────
// GPIO API
// ─────────────────────────────────────────────────────────────────────────────
static int lua_pin_mode(lua_State* L) {
    int pin = (int)luaL_checkinteger(L, 1);
    const char* modeStr = luaL_checkstring(L, 2);
    uint8_t mode = OUTPUT;
    if (strcmp(modeStr, "input") == 0) {
        mode = INPUT;
    } else if (strcmp(modeStr, "pullup") == 0) {
        mode = INPUT_PULLUP;
    } else if (strcmp(modeStr, "pulldown") == 0) {
        mode = INPUT_PULLDOWN;
    } else if (strcmp(modeStr, "output") == 0) {
        mode = OUTPUT;
    }
    pinMode(pin, mode);
    return 0;
}

static int lua_digital_write(lua_State* L) {
    int pin = (int)luaL_checkinteger(L, 1);
    int val = LOW;
    if (lua_isboolean(L, 2)) {
        val = lua_toboolean(L, 2) ? HIGH : LOW;
    } else {
        val = (int)luaL_checkinteger(L, 2) ? HIGH : LOW;
    }
    digitalWrite(pin, val);
    return 0;
}

static int lua_digital_read(lua_State* L) {
    int pin = (int)luaL_checkinteger(L, 1);
    lua_pushinteger(L, digitalRead(pin));
    return 1;
}

static int lua_analog_read(lua_State* L) {
    int pin = (int)luaL_checkinteger(L, 1);
    lua_pushinteger(L, analogRead(pin));
    return 1;
}

// ─────────────────────────────────────────────────────────────────────────────
// Filesystem / SD API
// ─────────────────────────────────────────────────────────────────────────────
static int lua_read_file(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    lv_fs_spi_lock();
    File f = SD.open(path, FILE_READ);
    if (!f || f.isDirectory()) {
        if (f) f.close();
        lv_fs_spi_unlock();
        lua_pushnil(L);
        lua_pushstring(L, "No se pudo abrir el archivo");
        return 2;
    }

    size_t size = f.size();
    char* buf = (char*)malloc(size + 1);
    if (!buf) {
        f.close();
        lv_fs_spi_unlock();
        lua_pushnil(L);
        lua_pushstring(L, "Memoria insuficiente");
        return 2;
    }

    size_t readLen = f.read((uint8_t*)buf, size);
    buf[readLen] = '\0';
    f.close();
    lv_fs_spi_unlock();

    lua_pushlstring(L, buf, readLen);
    free(buf);
    return 1;
}

static int lua_write_file(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    size_t dataLen = 0;
    const char* data = luaL_checklstring(L, 2, &dataLen);

    lv_fs_spi_lock();
    File f = SD.open(path, FILE_WRITE);
    if (!f) {
        lv_fs_spi_unlock();
        lua_pushboolean(L, 0);
        lua_pushstring(L, "No se pudo crear/abrir el archivo para escritura");
        return 2;
    }

    size_t written = f.write((const uint8_t*)data, dataLen);
    f.close();
    lv_fs_spi_unlock();

    lua_pushboolean(L, written == dataLen);
    return 1;
}

static int lua_file_exists(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    lv_fs_spi_lock();
    bool exists = SD.exists(path);
    lv_fs_spi_unlock();
    lua_pushboolean(L, exists);
    return 1;
}

static int lua_list_dir(lua_State* L) {
    const char* path = luaL_optstring(L, 1, "/");
    lv_fs_spi_lock();
    File root = SD.open(path);
    lua_newtable(L);
    if (root && root.isDirectory()) {
        int idx = 1;
        File file = root.openNextFile();
        while (file) {
            lua_pushstring(L, file.name());
            lua_rawseti(L, -2, idx++);
            file = root.openNextFile();
        }
        root.close();
    }
    lv_fs_spi_unlock();
    return 1;
}

// ─────────────────────────────────────────────────────────────────────────────
// Registro de Módulos
// ─────────────────────────────────────────────────────────────────────────────
void LuaBridge::registerAudioAPI(lua_State* L) {
    // Subtabla cbdos.audio
    lua_newtable(L);
    lua_pushcfunction(L, lua_beep);
    lua_setfield(L, -2, "beep");
    lua_pushcfunction(L, lua_play_mp3);
    lua_setfield(L, -2, "play_mp3");
    lua_pushcfunction(L, lua_play_mp3);
    lua_setfield(L, -2, "play_wav");
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
    lua_setfield(L, -2, "play_wav");
    lua_pushcfunction(L, lua_stop_audio);
    lua_setfield(L, -2, "stop_audio");
    lua_pushcfunction(L, lua_set_volume);
    lua_setfield(L, -2, "set_volume");
    lua_pushcfunction(L, lua_get_volume);
    lua_setfield(L, -2, "get_volume");
}

void LuaBridge::registerSystemAPI(lua_State* L) {
    // Subtabla cbdos.sys
    lua_newtable(L);
    lua_pushcfunction(L, lua_delay);
    lua_setfield(L, -2, "delay");
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
    lua_setfield(L, -2, "sys");

    // Accesos directos en cbdos.*
    lua_pushcfunction(L, lua_delay);
    lua_setfield(L, -2, "delay");
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
    // Subtabla cbdos.gpio
    lua_newtable(L);
    lua_pushcfunction(L, lua_pin_mode);
    lua_setfield(L, -2, "pin_mode");
    lua_pushcfunction(L, lua_digital_write);
    lua_setfield(L, -2, "digital_write");
    lua_pushcfunction(L, lua_digital_read);
    lua_setfield(L, -2, "digital_read");
    lua_pushcfunction(L, lua_analog_read);
    lua_setfield(L, -2, "analog_read");
    lua_setfield(L, -2, "gpio");

    // Accesos directos en cbdos.*
    lua_pushcfunction(L, lua_pin_mode);
    lua_setfield(L, -2, "pin_mode");
    lua_pushcfunction(L, lua_digital_write);
    lua_setfield(L, -2, "digital_write");
    lua_pushcfunction(L, lua_digital_read);
    lua_setfield(L, -2, "digital_read");
    lua_pushcfunction(L, lua_analog_read);
    lua_setfield(L, -2, "analog_read");
}

void LuaBridge::registerFsAPI(lua_State* L) {
    // Subtabla cbdos.fs
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

// ─────────────────────────────────────────────────────────────────────────────
// Graphics API
// ─────────────────────────────────────────────────────────────────────────────
#include <JC3248W535.h>
#include <lvgl.h>

extern JC3248W535_Display displayDriver;
extern JC3248W535_Touch touchDriver;

static volatile uint32_t s_uiPausedUntil = 0;
static volatile bool s_uiPausedIndefinite = false;
static volatile bool s_needsScreenRefresh = false;

void LuaBridge::pauseUI(uint32_t seconds) {
    if (seconds == 0) {
        s_uiPausedIndefinite = true;
        s_uiPausedUntil = 0;
    } else {
        s_uiPausedIndefinite = false;
        s_uiPausedUntil = millis() + (seconds * 1000);
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
    if (s_uiPausedIndefinite) {
        return true;
    }
    if (s_uiPausedUntil > 0) {
        if (millis() < s_uiPausedUntil) {
            return true;
        } else {
            // Expiró la pausa temporal
            resumeUI();
            return false;
        }
    }
    return false;
}


static inline uint16_t toRGB565(uint32_t c) {
    uint8_t r = (c >> 16) & 0xFF;
    uint8_t g = (c >> 8) & 0xFF;
    uint8_t b = c & 0xFF;
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

static int lua_gfx_clear(lua_State* L) {
    uint32_t col = (uint32_t)luaL_optinteger(L, 1, 0x000000);
    if (displayDriver.getCanvas()) {
        displayDriver.getCanvas()->fillScreen(toRGB565(col));
        displayDriver.flush();
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
    if (displayDriver.getCanvas()) {
        if (filled) {
            displayDriver.getCanvas()->fillRect(x, y, w, h, toRGB565(col));
        } else {
            displayDriver.getCanvas()->drawRect(x, y, w, h, toRGB565(col));
        }
        displayDriver.flush();
    }
    return 0;
}

static int lua_gfx_draw_circle(lua_State* L) {
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    int r = (int)luaL_checkinteger(L, 3);
    uint32_t col = (uint32_t)luaL_checkinteger(L, 4);
    bool filled = lua_toboolean(L, 5);
    if (displayDriver.getCanvas()) {
        if (filled) {
            displayDriver.getCanvas()->fillCircle(x, y, r, toRGB565(col));
        } else {
            displayDriver.getCanvas()->drawCircle(x, y, r, toRGB565(col));
        }
        displayDriver.flush();
    }
    return 0;
}

static int lua_gfx_draw_line(lua_State* L) {
    int x0 = (int)luaL_checkinteger(L, 1);
    int y0 = (int)luaL_checkinteger(L, 2);
    int x1 = (int)luaL_checkinteger(L, 3);
    int y1 = (int)luaL_checkinteger(L, 4);
    uint32_t col = (uint32_t)luaL_checkinteger(L, 5);
    if (displayDriver.getCanvas()) {
        displayDriver.getCanvas()->drawLine(x0, y0, x1, y1, toRGB565(col));
        displayDriver.flush();
    }
    return 0;
}

static int lua_gfx_draw_text(lua_State* L) {
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    const char* text = luaL_checkstring(L, 3);
    uint32_t col = (uint32_t)luaL_optinteger(L, 4, 0xFFFFFF);
    int size = (int)luaL_optinteger(L, 5, 1);
    if (displayDriver.getCanvas()) {
        displayDriver.getCanvas()->setCursor(x, y);
        displayDriver.getCanvas()->setTextColor(toRGB565(col));
        displayDriver.getCanvas()->setTextSize(size);
        displayDriver.getCanvas()->print(text);
        displayDriver.flush();
    }
    return 0;
}

static int lua_gfx_touch(lua_State* L) {
    TouchPoint tp;
    bool touched = touchDriver.read(tp) && tp.touched;
    lua_newtable(L);
    lua_pushboolean(L, touched);
    lua_setfield(L, -2, "touched");
    lua_pushinteger(L, tp.x);
    lua_setfield(L, -2, "x");
    lua_pushinteger(L, tp.y);
    lua_setfield(L, -2, "y");
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

static int lua_gfx_width(lua_State* L) {
    lua_pushinteger(L, 320);
    return 1;
}

static int lua_gfx_height(lua_State* L) {
    lua_pushinteger(L, 480);
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

    Serial.println("[LuaBridge] Bindings nativos 'cbdos.*' registrados con éxito.");
}
