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
static bool s_isDisplayLocked = false;

void LuaBridge::pauseUI(uint32_t seconds) {
    if (!s_isDisplayLocked) {
        cbdos::display::lock(500);
        s_isDisplayLocked = true;
    }
    if (seconds == 0) {
        s_uiPausedIndefinite = true;
        s_uiPausedUntil = 0;
    } else {
        s_uiPausedIndefinite = false;
        s_uiPausedUntil = cbdos::system::getTimeMs() + (seconds * 1000);
    }
}

void LuaBridge::resumeUI() {
    s_uiPausedIndefinite = false;
    s_uiPausedUntil = 0;
    if (s_isDisplayLocked) {
        cbdos::display::unlock();
        s_isDisplayLocked = false;
    }
    // Forzar redibujado de la pantalla de LVGL
    if (lv_is_initialized()) {
        lv_obj_t* scr = lv_screen_active();
        if (scr) {
            lv_obj_invalidate(scr);
        }
    }
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
// Graphics 2D Canvas Engine (Direct Framebuffer Access & Primitives)
// ─────────────────────────────────────────────────────────────────────────────
static inline uint16_t toRGB565(uint32_t c) {
    uint8_t r = (c >> 16) & 0xFF;
    uint8_t g = (c >> 8) & 0xFF;
    uint8_t b = c & 0xFF;
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// Tabla de fuente bitmap 5x7 ASCII básica (32 a 126)
static const uint8_t font5x7[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, //   (space)
    0x00, 0x00, 0x5F, 0x00, 0x00, // !
    0x00, 0x07, 0x00, 0x07, 0x00, // "
    0x14, 0x7F, 0x14, 0x7F, 0x14, // #
    0x24, 0x2A, 0x7F, 0x2A, 0x12, // $
    0x23, 0x13, 0x08, 0x64, 0x62, // %
    0x36, 0x49, 0x55, 0x22, 0x50, // &
    0x00, 0x05, 0x03, 0x00, 0x00, // '
    0x00, 0x1C, 0x22, 0x41, 0x00, // (
    0x00, 0x41, 0x22, 0x1C, 0x00, // )
    0x08, 0x2A, 0x1C, 0x2A, 0x08, // *
    0x08, 0x08, 0x3E, 0x08, 0x08, // +
    0x00, 0x50, 0x30, 0x00, 0x00, // ,
    0x08, 0x08, 0x08, 0x08, 0x08, // -
    0x00, 0x60, 0x60, 0x00, 0x00, // .
    0x20, 0x10, 0x08, 0x04, 0x02, // /
    0x3E, 0x51, 0x49, 0x45, 0x3E, // 0
    0x00, 0x42, 0x7F, 0x40, 0x00, // 1
    0x42, 0x61, 0x51, 0x49, 0x46, // 2
    0x21, 0x41, 0x45, 0x4B, 0x31, // 3
    0x18, 0x14, 0x12, 0x7F, 0x10, // 4
    0x27, 0x45, 0x45, 0x45, 0x39, // 5
    0x3C, 0x4A, 0x49, 0x49, 0x30, // 6
    0x01, 0x71, 0x09, 0x05, 0x03, // 7
    0x36, 0x49, 0x49, 0x49, 0x36, // 8
    0x06, 0x49, 0x49, 0x29, 0x1E, // 9
    0x00, 0x36, 0x36, 0x00, 0x00, // :
    0x00, 0x56, 0x36, 0x00, 0x00, // ;
    0x00, 0x08, 0x14, 0x22, 0x41, // <
    0x14, 0x14, 0x14, 0x14, 0x14, // =
    0x41, 0x22, 0x14, 0x08, 0x00, // >
    0x02, 0x01, 0x51, 0x09, 0x06, // ?
    0x32, 0x49, 0x79, 0x41, 0x3E, // @
    0x7E, 0x11, 0x11, 0x11, 0x7E, // A
    0x7F, 0x49, 0x49, 0x49, 0x36, // B
    0x3E, 0x41, 0x41, 0x41, 0x22, // C
    0x7F, 0x41, 0x41, 0x22, 0x1C, // D
    0x7F, 0x49, 0x49, 0x49, 0x41, // E
    0x7F, 0x09, 0x09, 0x01, 0x01, // F
    0x3E, 0x41, 0x41, 0x51, 0x32, // G
    0x7F, 0x08, 0x08, 0x08, 0x7F, // H
    0x00, 0x41, 0x7F, 0x41, 0x00, // I
    0x20, 0x40, 0x41, 0x3F, 0x01, // J
    0x7F, 0x08, 0x14, 0x22, 0x41, // K
    0x7F, 0x40, 0x40, 0x40, 0x40, // L
    0x7F, 0x02, 0x04, 0x02, 0x7F, // M
    0x7F, 0x04, 0x08, 0x10, 0x7F, // N
    0x3E, 0x41, 0x41, 0x41, 0x3E, // O
    0x7F, 0x09, 0x09, 0x09, 0x06, // P
    0x3E, 0x41, 0x51, 0x21, 0x5E, // Q
    0x7F, 0x09, 0x19, 0x29, 0x46, // R
    0x46, 0x49, 0x49, 0x49, 0x31, // S
    0x01, 0x01, 0x7F, 0x01, 0x01, // T
    0x3F, 0x40, 0x40, 0x40, 0x3F, // U
    0x1F, 0x20, 0x40, 0x20, 0x1F, // V
    0x7F, 0x20, 0x18, 0x20, 0x7F, // W
    0x63, 0x14, 0x08, 0x14, 0x63, // X
    0x03, 0x04, 0x78, 0x04, 0x03, // Y
    0x61, 0x51, 0x49, 0x45, 0x43, // Z
    0x00, 0x00, 0x7F, 0x41, 0x41, // [
    0x02, 0x04, 0x08, 0x10, 0x20, // backslash
    0x41, 0x41, 0x7F, 0x00, 0x00, // ]
    0x04, 0x02, 0x01, 0x02, 0x04, // ^
    0x40, 0x40, 0x40, 0x40, 0x40, // _
    0x00, 0x01, 0x02, 0x04, 0x00, // `
    0x20, 0x54, 0x54, 0x54, 0x78, // a
    0x7F, 0x48, 0x44, 0x44, 0x38, // b
    0x38, 0x44, 0x44, 0x44, 0x20, // c
    0x38, 0x44, 0x44, 0x48, 0x7F, // d
    0x38, 0x54, 0x54, 0x54, 0x18, // e
    0x08, 0x7E, 0x09, 0x01, 0x02, // f
    0x08, 0x14, 0x54, 0x54, 0x3C, // g
    0x7F, 0x08, 0x04, 0x04, 0x78, // h
    0x00, 0x44, 0x7D, 0x40, 0x00, // i
    0x20, 0x40, 0x44, 0x3D, 0x00, // j
    0x00, 0x7F, 0x10, 0x28, 0x44, // k
    0x00, 0x41, 0x7F, 0x40, 0x00, // l
    0x7C, 0x04, 0x18, 0x04, 0x78, // m
    0x7C, 0x08, 0x04, 0x04, 0x78, // n
    0x38, 0x44, 0x44, 0x44, 0x38, // o
    0x7C, 0x14, 0x14, 0x14, 0x08, // p
    0x08, 0x14, 0x14, 0x18, 0x7C, // q
    0x7C, 0x08, 0x04, 0x04, 0x08, // r
    0x48, 0x54, 0x54, 0x54, 0x20, // s
    0x04, 0x3F, 0x44, 0x40, 0x20, // t
    0x3C, 0x40, 0x40, 0x20, 0x7C, // u
    0x1C, 0x20, 0x40, 0x20, 0x1C, // v
    0x3C, 0x40, 0x30, 0x40, 0x3C, // w
    0x44, 0x28, 0x10, 0x28, 0x44, // x
    0x0C, 0x50, 0x50, 0x50, 0x3C, // y
    0x44, 0x64, 0x54, 0x4C, 0x44, // z
    0x00, 0x08, 0x36, 0x41, 0x00, // {
    0x00, 0x00, 0x7F, 0x00, 0x00, // |
    0x00, 0x41, 0x36, 0x08, 0x00, // }
    0x08, 0x08, 0x2A, 0x1C, 0x08  // ~
};

static inline void setPixel(int x, int y, uint16_t col565, int w, int h, uint16_t* fb0, uint16_t* fb1) {
    if (x >= 0 && x < w && y >= 0 && y < h) {
        int idx = y * w + x;
        if (fb0) fb0[idx] = col565;
        if (fb1) fb1[idx] = col565;
    }
}

static void drawHLine(int x, int y, int len, uint16_t col565, int w, int h, uint16_t* fb0, uint16_t* fb1) {
    if (y < 0 || y >= h) return;
    int xStart = std::max(0, x);
    int xEnd = std::min(w, x + len);
    for (int i = xStart; i < xEnd; i++) {
        int idx = y * w + i;
        if (fb0) fb0[idx] = col565;
        if (fb1) fb1[idx] = col565;
    }
}

static void drawVLine(int x, int y, int len, uint16_t col565, int w, int h, uint16_t* fb0, uint16_t* fb1) {
    if (x < 0 || x >= w) return;
    int yStart = std::max(0, y);
    int yEnd = std::min(h, y + len);
    for (int j = yStart; j < yEnd; j++) {
        int idx = j * w + x;
        if (fb0) fb0[idx] = col565;
        if (fb1) fb1[idx] = col565;
    }
}

static void drawRect(int x, int y, int rw, int rh, uint16_t col565, bool filled, int w, int h, uint16_t* fb0, uint16_t* fb1) {
    if (filled) {
        int yStart = std::max(0, y);
        int yEnd = std::min(h, y + rh);
        for (int j = yStart; j < yEnd; j++) {
            drawHLine(x, j, rw, col565, w, h, fb0, fb1);
        }
    } else {
        drawHLine(x, y, rw, col565, w, h, fb0, fb1);
        drawHLine(x, y + rh - 1, rw, col565, w, h, fb0, fb1);
        drawVLine(x, y, rh, col565, w, h, fb0, fb1);
        drawVLine(x + rw - 1, y, rh, col565, w, h, fb0, fb1);
    }
}

static void drawLine(int x0, int y0, int x1, int y1, uint16_t col565, int w, int h, uint16_t* fb0, uint16_t* fb1) {
    int dx = abs(x1 - x0);
    int sx = (x0 < x1) ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    while (true) {
        setPixel(x0, y0, col565, w, h, fb0, fb1);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void drawCircle(int cx, int cy, int r, uint16_t col565, bool filled, int w, int h, uint16_t* fb0, uint16_t* fb1) {
    int f = 1 - r;
    int ddF_x = 1;
    int ddF_y = -2 * r;
    int x = 0;
    int y = r;

    if (filled) {
        drawHLine(cx - r, cy, 2 * r + 1, col565, w, h, fb0, fb1);
    } else {
        setPixel(cx, cy + r, col565, w, h, fb0, fb1);
        setPixel(cx, cy - r, col565, w, h, fb0, fb1);
        setPixel(cx + r, cy, col565, w, h, fb0, fb1);
        setPixel(cx - r, cy, col565, w, h, fb0, fb1);
    }

    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        if (filled) {
            drawHLine(cx - x, cy + y, 2 * x + 1, col565, w, h, fb0, fb1);
            drawHLine(cx - x, cy - y, 2 * x + 1, col565, w, h, fb0, fb1);
            drawHLine(cx - y, cy + x, 2 * y + 1, col565, w, h, fb0, fb1);
            drawHLine(cx - y, cy - x, 2 * y + 1, col565, w, h, fb0, fb1);
        } else {
            setPixel(cx + x, cy + y, col565, w, h, fb0, fb1);
            setPixel(cx - x, cy + y, col565, w, h, fb0, fb1);
            setPixel(cx + x, cy - y, col565, w, h, fb0, fb1);
            setPixel(cx - x, cy - y, col565, w, h, fb0, fb1);
            setPixel(cx + y, cy + x, col565, w, h, fb0, fb1);
            setPixel(cx - y, cy + x, col565, w, h, fb0, fb1);
            setPixel(cx + y, cy - x, col565, w, h, fb0, fb1);
            setPixel(cx - y, cy - x, col565, w, h, fb0, fb1);
        }
    }
}

static void drawChar(int x, int y, char c, uint16_t col565, int size, int w, int h, uint16_t* fb0, uint16_t* fb1) {
    if (c < 32 || c > 126) c = '?';
    int fontIdx = (c - 32) * 5;

    for (int col = 0; col < 5; col++) {
        uint8_t line = font5x7[fontIdx + col];
        for (int row = 0; row < 7; row++) {
            if (line & 0x01) {
                if (size == 1) {
                    setPixel(x + col, y + row, col565, w, h, fb0, fb1);
                } else {
                    drawRect(x + (col * size), y + (row * size), size, size, col565, true, w, h, fb0, fb1);
                }
            }
            line >>= 1;
        }
    }
}

static void drawText(int x, int y, const char* str, uint16_t col565, int size, int w, int h, uint16_t* fb0, uint16_t* fb1) {
    if (!str) return;
    int cursorX = x;
    int cursorY = y;
    int charW = 6 * size;
    int charH = 8 * size;

    while (*str) {
        if (*str == '\n') {
            cursorX = x;
            cursorY += charH;
        } else if (*str == '\r') {
            // ignorar
        } else {
            drawChar(cursorX, cursorY, *str, col565, size, w, h, fb0, fb1);
            cursorX += charW;
        }
        str++;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Lua GFX Bindings
// ─────────────────────────────────────────────────────────────────────────────
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

static int lua_gfx_clear(lua_State* L) {
    uint32_t col = (uint32_t)luaL_optinteger(L, 1, 0x000000);
    uint16_t col565 = toRGB565(col);
    auto caps = cbdos::display::getCapabilities();
    uint16_t* fb0 = (uint16_t*)cbdos::display::getFramebuffer(0);
    uint16_t* fb1 = (uint16_t*)cbdos::display::getFramebuffer(1);

    size_t totalPixels = caps.width * caps.height;
    if (fb0) {
        for (size_t i = 0; i < totalPixels; i++) fb0[i] = col565;
    }
    if (fb1) {
        for (size_t i = 0; i < totalPixels; i++) fb1[i] = col565;
    }
    return 0;
}

static int lua_gfx_draw_rect(lua_State* L) {
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    int rw = (int)luaL_checkinteger(L, 3);
    int rh = (int)luaL_checkinteger(L, 4);
    uint32_t col = (uint32_t)luaL_checkinteger(L, 5);
    bool filled = lua_toboolean(L, 6);

    auto caps = cbdos::display::getCapabilities();
    uint16_t* fb0 = (uint16_t*)cbdos::display::getFramebuffer(0);
    uint16_t* fb1 = (uint16_t*)cbdos::display::getFramebuffer(1);
    drawRect(x, y, rw, rh, toRGB565(col), filled, caps.width, caps.height, fb0, fb1);
    return 0;
}

static int lua_gfx_draw_circle(lua_State* L) {
    int cx = (int)luaL_checkinteger(L, 1);
    int cy = (int)luaL_checkinteger(L, 2);
    int r = (int)luaL_checkinteger(L, 3);
    uint32_t col = (uint32_t)luaL_checkinteger(L, 4);
    bool filled = lua_toboolean(L, 5);

    auto caps = cbdos::display::getCapabilities();
    uint16_t* fb0 = (uint16_t*)cbdos::display::getFramebuffer(0);
    uint16_t* fb1 = (uint16_t*)cbdos::display::getFramebuffer(1);
    drawCircle(cx, cy, r, toRGB565(col), filled, caps.width, caps.height, fb0, fb1);
    return 0;
}

static int lua_gfx_draw_line(lua_State* L) {
    int x0 = (int)luaL_checkinteger(L, 1);
    int y0 = (int)luaL_checkinteger(L, 2);
    int x1 = (int)luaL_checkinteger(L, 3);
    int y1 = (int)luaL_checkinteger(L, 4);
    uint32_t col = (uint32_t)luaL_checkinteger(L, 5);

    auto caps = cbdos::display::getCapabilities();
    uint16_t* fb0 = (uint16_t*)cbdos::display::getFramebuffer(0);
    uint16_t* fb1 = (uint16_t*)cbdos::display::getFramebuffer(1);
    drawLine(x0, y0, x1, y1, toRGB565(col), caps.width, caps.height, fb0, fb1);
    return 0;
}

static int lua_gfx_draw_text(lua_State* L) {
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    const char* text = luaL_checkstring(L, 3);
    uint32_t col = (uint32_t)luaL_optinteger(L, 4, 0xFFFFFF);
    int size = (int)luaL_optinteger(L, 5, 1);
    if (size < 1) size = 1;

    auto caps = cbdos::display::getCapabilities();
    uint16_t* fb0 = (uint16_t*)cbdos::display::getFramebuffer(0);
    uint16_t* fb1 = (uint16_t*)cbdos::display::getFramebuffer(1);
    drawText(x, y, text, toRGB565(col), size, caps.width, caps.height, fb0, fb1);
    return 0;
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
