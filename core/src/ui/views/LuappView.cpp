#include "LuappView.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include "../../lua/LuaBridge.hpp"
#include "cbdos/storage.hpp"
#include "cbdos/system.hpp"

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#include <cstdio>

namespace cbdos {
namespace ui {

LuappView::LuappView(const std::string& scriptPath, 
                     const std::string& appName,
                     const std::string& iconSymbol)
    : BaseView(appName),
      m_scriptPath(scriptPath),
      m_iconSymbol(iconSymbol),
      m_L(nullptr),
      m_initialized(false) {
}

LuappView::~LuappView() {
    if (m_L) {
        lua_close(m_L);
        m_L = nullptr;
    }
}

bool LuappView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;

    // Contenedor base de la aplicación con scroll suave
    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_container, 0, 0);
    lv_obj_set_style_radius(m_container, 0, 0);
    lv_obj_set_style_pad_all(m_container, 12, 0);
    lv_obj_set_scrollbar_mode(m_container, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_flex_flow(m_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(m_container, 10, 0);

    // 1. Inicializar Lua VM aislada
    m_L = luaL_newstate();
    if (!m_L) {
        renderError("Fallo crítico: No se pudo instanciar la máquina virtual Lua.");
        return true;
    }

    luaL_openlibs(m_L);

    // 2. Registrar Bindings de Hardware y UI (cbdos.*)
    LuaBridge::registerAll(m_L);

    // 3. Exponer variables de entorno
    lua_pushlightuserdata(m_L, m_container);
    lua_setglobal(m_L, "root_container");

    lua_pushstring(m_L, m_scriptPath.c_str());
    lua_setglobal(m_L, "APP_PATH");

    // 4. Cargar código fuente
    std::string scriptCode = cbdos::storage::readFile(m_scriptPath.c_str());
    if (scriptCode.empty() && !cbdos::storage::fileExists(m_scriptPath.c_str())) {
        if (m_scriptPath.rfind("/sdcard/", 0) != 0) {
            std::string alt = std::string("/sdcard/") + (m_scriptPath[0] == '/' ? m_scriptPath.substr(1) : m_scriptPath);
            scriptCode = cbdos::storage::readFile(alt.c_str());
        }
    }

    if (scriptCode.empty()) {
        renderError("No se pudo leer el archivo de la aplicación .luapp.");
        return true;
    }

    // 5. Ejecutar script
    if (luaL_dostring(m_L, scriptCode.c_str()) != 0) {
        const char* err = lua_tostring(m_L, -1);
        renderError(err ? err : "Error de sintaxis o ejecución al cargar el script Lua.");
        lua_pop(m_L, 1);
        return true;
    }

    // 6. Invocar on_create(root_container) si está definida
    lua_getglobal(m_L, "on_create");
    if (lua_isfunction(m_L, -1)) {
        lua_pushlightuserdata(m_L, m_container);
        if (lua_pcall(m_L, 1, 0, 0) != 0) {
            const char* err = lua_tostring(m_L, -1);
            renderError(err ? err : "Error en la ejecución de on_create().");
            lua_pop(m_L, 1);
            return true;
        }
    } else {
        lua_pop(m_L, 1);
    }

    m_initialized = true;
    return true;
}

void LuappView::onShow() {
    BaseView::onShow();

    // Actualizar título en HeaderBar
    auto& header = UIManager::getInstance().getHeaderBar();
    header.setTitle(m_name.c_str());

    if (m_L && m_initialized) {
        lua_getglobal(m_L, "on_show");
        if (lua_isfunction(m_L, -1)) {
            if (lua_pcall(m_L, 0, 0, 0) != 0) {
                const char* err = lua_tostring(m_L, -1);
                printf("[LuappView] Error in on_show(): %s\n", err ? err : "Unknown");
                lua_pop(m_L, 1);
            }
        } else {
            lua_pop(m_L, 1);
        }
    }
}

void LuappView::onHide() {
    if (m_L && m_initialized) {
        lua_getglobal(m_L, "on_hide");
        if (lua_isfunction(m_L, -1)) {
            if (lua_pcall(m_L, 0, 0, 0) != 0) {
                const char* err = lua_tostring(m_L, -1);
                printf("[LuappView] Error in on_hide(): %s\n", err ? err : "Unknown");
                lua_pop(m_L, 1);
            }
        } else {
            lua_pop(m_L, 1);
        }
    }

    BaseView::onHide();
}

void LuappView::onDestroy() {
    if (m_L && m_initialized) {
        lua_getglobal(m_L, "on_destroy");
        if (lua_isfunction(m_L, -1)) {
            if (lua_pcall(m_L, 0, 0, 0) != 0) {
                const char* err = lua_tostring(m_L, -1);
                printf("[LuappView] Error in on_destroy(): %s\n", err ? err : "Unknown");
                lua_pop(m_L, 1);
            }
        } else {
            lua_pop(m_L, 1);
        }

        lua_close(m_L);
        m_L = nullptr;
        m_initialized = false;
    }

    BaseView::onDestroy();
}

void LuappView::onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) {
    if (m_container && lv_obj_is_valid(m_container)) {
        lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
    }
}

void LuappView::renderError(const char* message) {
    if (!m_container || !lv_obj_is_valid(m_container)) return;

    lv_obj_clean(m_container);

    lv_obj_t* card = lv_obj_create(m_container);
    lv_obj_set_size(card, LV_PCT(100), LV_SIZE_CONTENT);
    DefaultTheme::applyRaisedCard(card, 16);
    lv_obj_set_style_border_color(card, lv_color_hex(0xEF4444), 0);
    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_style_pad_all(card, 16, 0);

    lv_obj_t* title = lv_label_create(card);
    lv_label_set_text(title, LV_SYMBOL_WARNING " Error al ejecutar Lua App");
    lv_obj_set_style_text_color(title, lv_color_hex(0xEF4444), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);

    lv_obj_t* body = lv_label_create(card);
    lv_label_set_text(body, message ? message : "Error desconocido");
    lv_obj_set_style_text_color(body, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(body, &lv_font_montserrat_12, 0);
    lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(body, LV_PCT(100));
    lv_obj_set_style_margin_top(body, 8, 0);
}

} // namespace ui
} // namespace cbdos
