#include "DashboardView.hpp"
#include "ConfigView.hpp"
#include "MusicPlayerView.hpp"
#include "AudioRecorderView.hpp"
#include "GalleryListView.hpp"
#include "FlasherView.hpp"
#include "RadioView.hpp"
#include "LuaRunnerView.hpp"
#include "TextEditorView.hpp"
#include "CartridgeView.hpp"
#include "FileManagerView.hpp"
#include "SerialTerminalView.hpp"
#include "UtilitiesView.hpp"
#include "TlvBrowserView.hpp"
#include "LuappView.hpp"
#include "../../lua/LuappManager.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include "cbdos/display.hpp"
#include "cbdos/system.hpp"
#include <cstring>

namespace cbdos {
namespace ui {

DashboardView::DashboardView()
    : BaseView("Dashboard") {
}

bool DashboardView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;

    // Resetear lista de aplicaciones
    m_apps = {
        {"browser", "Navegador", LV_SYMBOL_EYE_OPEN, 0x00B4D8, false, ""},
        {"gallery", "Galeria", LV_SYMBOL_IMAGE, 0xEC4899, false, ""},
        {"files", "Archivos", LV_SYMBOL_DIRECTORY, 0xF77F00, false, ""},
        {"utilities", "Utilidades", LV_SYMBOL_LIST, 0x00F5D4, false, ""},
        {"cartridge", "Cartuchos", LV_SYMBOL_PLAY, 0x3F68D9, false, ""},
        {"lua", "Lua Runner", LV_SYMBOL_FILE, 0x06B6D4, false, ""},
        {"editor", "Editor", LV_SYMBOL_EDIT, 0x3B82F6, false, ""},
        {"radio", "Radio Online", LV_SYMBOL_WIFI, 0x10B981, false, ""},
        {"flasher", "Flasheador", LV_SYMBOL_DOWNLOAD, 0xF59E0B, false, ""},
        {"terminal", "Terminal UART", LV_SYMBOL_KEYBOARD, 0x10B981, false, ""},
        {"recorder", "Grabadora", LV_SYMBOL_AUDIO, 0xEF4444, false, ""},
        {"music", "Musica", LV_SYMBOL_AUDIO, 0x00E5FF, false, ""},
        {"config", "Configuracion", LV_SYMBOL_SETTINGS, 0x9D4EDD, false, ""}
    };

    // Escanear dinámicamente aplicaciones .luapp en la MicroSD
    auto& luappMgr = cbdos::lua::LuappManager::getInstance();
    luappMgr.scanApps("/sdcard/apps");
    for (const auto& app : luappMgr.getDiscoveredApps()) {
        m_apps.push_back({
            "luapp_" + app.name,
            app.name,
            app.iconSymbol,
            app.accentColor,
            true,
            app.filePath
        });
    }

    // Contenedor principal con scroll vertical suave (Fondo transparente para ver el Wallpaper)
    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_container, 0, 0);
    lv_obj_set_style_radius(m_container, 0, 0);
    lv_obj_set_style_pad_all(m_container, 12, 0);
    lv_obj_set_scrollbar_mode(m_container, LV_SCROLLBAR_MODE_AUTO);

    setupLayout();
    createCards();
    return true;
}

void DashboardView::onDestroy() {
    m_cardObjs.clear();
    BaseView::onDestroy();
}

void DashboardView::setupLayout() {
    auto caps = cbdos::display::getCapabilities();

    // Configurar Flex Wrap para distribución automática de tarjetas en Grid
    lv_obj_set_flex_flow(m_container, LV_FLEX_FLOW_ROW_WRAP);
    
    if (caps.width >= 480) {
        // Modo Alta Resolución (ESP32-P4: 480x800)
        lv_obj_set_flex_align(m_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_row(m_container, 16, 0);
        lv_obj_set_style_pad_column(m_container, 12, 0);
    } else {
        // Modo Compacto (ESP32-S3: 320x480)
        lv_obj_set_flex_align(m_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_row(m_container, 12, 0);
        lv_obj_set_style_pad_column(m_container, 12, 0);
    }
}

void DashboardView::createCards() {
    auto caps = cbdos::display::getCapabilities();

    // Dimensiones del ítem de app en el OS
    int32_t itemWidth = (caps.width >= 480) ? 104 : 96;
    int32_t iconSize = (caps.width >= 480) ? 68 : 60;
    int32_t iconRadius = (caps.width >= 480) ? 20 : 18;

    m_cardObjs.clear();

    for (size_t i = 0; i < m_apps.size(); ++i) {
        const auto& app = m_apps[i];

        // 1. Contenedor vertical transparente de la app (Ícono + Título)
        lv_obj_t* appItem = lv_obj_create(m_container);
        lv_obj_set_size(appItem, itemWidth, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(appItem, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(appItem, 0, 0);
        lv_obj_set_style_pad_all(appItem, 0, 0);
        DefaultTheme::disableScroll(appItem);

        lv_obj_set_flex_flow(appItem, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(appItem, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        // 2. Botón Squircle del Icono (iOS / Android style)
        lv_obj_t* btnIcon = lv_button_create(appItem);
        lv_obj_set_size(btnIcon, iconSize, iconSize);
        lv_obj_set_style_radius(btnIcon, iconRadius, 0);
        lv_obj_set_style_bg_color(btnIcon, lv_color_hex(0x1E2230), 0);
        lv_obj_set_style_bg_grad_color(btnIcon, lv_color_hex(0x13161F), 0);
        lv_obj_set_style_bg_grad_dir(btnIcon, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_border_color(btnIcon, lv_color_hex(app.accentColor), 0);
        lv_obj_set_style_border_width(btnIcon, 1, 0);
        lv_obj_set_style_border_opa(btnIcon, LV_OPA_60, 0);
        lv_obj_set_style_shadow_width(btnIcon, 12, 0);
        lv_obj_set_style_shadow_color(btnIcon, lv_color_black(), 0);
        lv_obj_set_style_shadow_opa(btnIcon, LV_OPA_40, 0);
        lv_obj_set_style_pad_all(btnIcon, 0, 0);

        if (app.id == "recorder") {
            // Icono vectorial estilizado de Micrófono (Cápsula + Soporte + Base)
            lv_obj_t* micBox = lv_obj_create(btnIcon);
            lv_obj_set_size(micBox, 28, 36);
            lv_obj_set_style_bg_opa(micBox, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(micBox, 0, 0);
            lv_obj_set_style_pad_all(micBox, 0, 0);
            lv_obj_center(micBox);
            lv_obj_remove_flag(micBox, LV_OBJ_FLAG_CLICKABLE);
            DefaultTheme::disableScroll(micBox);

            // Cápsula superior del micrófono
            lv_obj_t* capsule = lv_obj_create(micBox);
            lv_obj_set_size(capsule, 12, 20);
            lv_obj_set_style_radius(capsule, 6, 0);
            lv_obj_set_style_bg_color(capsule, lv_color_hex(0xEF4444), 0);
            lv_obj_set_style_border_color(capsule, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_border_width(capsule, 1, 0);
            lv_obj_align(capsule, LV_ALIGN_TOP_MID, 0, 0);
            lv_obj_remove_flag(capsule, LV_OBJ_FLAG_CLICKABLE);

            // Soporte envolvente inferior
            lv_obj_t* arcHolder = lv_obj_create(micBox);
            lv_obj_set_size(arcHolder, 22, 14);
            lv_obj_set_style_radius(arcHolder, 11, 0);
            lv_obj_set_style_bg_opa(arcHolder, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_color(arcHolder, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_border_width(arcHolder, 2, 0);
            lv_obj_set_style_border_side(arcHolder, (lv_border_side_t)(LV_BORDER_SIDE_BOTTOM | LV_BORDER_SIDE_LEFT | LV_BORDER_SIDE_RIGHT), 0);
            lv_obj_align(arcHolder, LV_ALIGN_TOP_MID, 0, 9);
            lv_obj_remove_flag(arcHolder, LV_OBJ_FLAG_CLICKABLE);

            // Brazo vertical
            lv_obj_t* stem = lv_obj_create(micBox);
            lv_obj_set_size(stem, 2, 6);
            lv_obj_set_style_bg_color(stem, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_border_width(stem, 0, 0);
            lv_obj_align(stem, LV_ALIGN_BOTTOM_MID, 0, -4);
            lv_obj_remove_flag(stem, LV_OBJ_FLAG_CLICKABLE);

            // Base horizontal
            lv_obj_t* base = lv_obj_create(micBox);
            lv_obj_set_size(base, 14, 2);
            lv_obj_set_style_bg_color(base, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_border_width(base, 0, 0);
            lv_obj_align(base, LV_ALIGN_BOTTOM_MID, 0, -2);
            lv_obj_remove_flag(base, LV_OBJ_FLAG_CLICKABLE);
        } else {
            // Icono estándar por símbolo LVGL
            lv_obj_t* lblIcon = lv_label_create(btnIcon);
            lv_label_set_text(lblIcon, app.icon.c_str());
            lv_obj_set_style_text_color(lblIcon, lv_color_hex(app.accentColor), 0);
            lv_obj_set_style_text_font(lblIcon, &lv_font_montserrat_24, 0);
            lv_obj_center(lblIcon);
        }

        // 3. Título de la App situado debajo del ícono (como en un OS)
        lv_obj_t* lblTitle = lv_label_create(appItem);
        lv_label_set_text(lblTitle, app.title.c_str());
        lv_obj_set_style_text_color(lblTitle, DefaultTheme::getTextColor(), 0);
        lv_obj_set_style_text_font(lblTitle, &lv_font_montserrat_12, 0);
        lv_obj_set_style_margin_top(lblTitle, 6, 0);
        lv_obj_set_style_text_align(lblTitle, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(lblTitle, itemWidth);

        // Evento de Click pasando el índice de la App
        lv_obj_set_user_data(btnIcon, (void*)(uintptr_t)i);
        lv_obj_add_event_cb(btnIcon, cardClickedEventCb, LV_EVENT_CLICKED, this);

        m_cardObjs.push_back(btnIcon);
    }
}

void DashboardView::cardClickedEventCb(lv_event_t* e) {
    auto* view = static_cast<DashboardView*>(lv_event_get_user_data(e));
    lv_obj_t* target = (lv_obj_t*)lv_event_get_target(e);
    if (!view || !target) return;

    size_t idx = (size_t)(uintptr_t)lv_obj_get_user_data(target);
    if (idx >= view->m_apps.size()) return;
    const auto& app = view->m_apps[idx];

    if (app.isLuapp) {
        UIManager::getInstance().pushView(std::make_shared<LuappView>(app.luappPath, app.title, app.icon));
        return;
    }

    if (app.id == "browser") {
        UIManager::getInstance().pushView(std::make_shared<TlvBrowserView>());
    } else if (app.id == "gallery") {
        UIManager::getInstance().pushView(std::make_shared<GalleryListView>());
    } else if (app.id == "files") {
        UIManager::getInstance().pushView(std::make_shared<FileManagerView>());
    } else if (app.id == "utilities") {
        UIManager::getInstance().pushView(std::make_shared<UtilitiesView>());
    } else if (app.id == "cartridge") {
        UIManager::getInstance().pushView(std::make_shared<CartridgeView>());
    } else if (app.id == "lua") {
        UIManager::getInstance().pushView(std::make_shared<LuaRunnerView>());
    } else if (app.id == "editor") {
        UIManager::getInstance().pushView(std::make_shared<TextEditorView>());
    } else if (app.id == "radio") {
        UIManager::getInstance().pushView(std::make_shared<RadioView>());
    } else if (app.id == "config") {
        UIManager::getInstance().pushView(std::make_shared<ConfigView>());
    } else if (app.id == "recorder") {
        UIManager::getInstance().pushView(std::make_shared<AudioRecorderView>());
    } else if (app.id == "music") {
        UIManager::getInstance().pushView(std::make_shared<MusicPlayerView>());
    } else if (app.id == "flasher") {
        UIManager::getInstance().pushView(std::make_shared<FlasherView>());
    } else if (app.id == "terminal") {
        UIManager::getInstance().pushView(std::make_shared<SerialTerminalView>());
    }
}

void DashboardView::onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) {
    if (!m_container || !lv_obj_is_valid(m_container)) return;
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
}

} // namespace ui
} // namespace cbdos
