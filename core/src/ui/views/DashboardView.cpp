#include "DashboardView.hpp"
#include "ConfigView.hpp"
#include "MusicPlayerView.hpp"
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

    int32_t cardWidth = (caps.width >= 480) ? 140 : 138;
    int32_t cardHeight = (caps.width >= 480) ? 115 : 95;

    m_cardObjs.clear();

    for (size_t i = 0; i < m_apps.size(); ++i) {
        const auto& app = m_apps[i];

        // Botón con estilo original DefaultTheme
        lv_obj_t* card = lv_button_create(m_container);
        lv_obj_set_size(card, cardWidth, cardHeight);
        DefaultTheme::applyButton(card, 16);
        lv_obj_set_style_pad_all(card, 8, 0);

        // Layout vertical interno
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        // Contenedor hundido para el icono
        lv_obj_t* iconContainer = lv_obj_create(card);
        lv_obj_set_size(iconContainer, 46, 46);
        DefaultTheme::applySunkenCard(iconContainer, 23);
        DefaultTheme::disableScroll(iconContainer);
        lv_obj_set_style_pad_all(iconContainer, 0, 0);
        lv_obj_remove_flag(iconContainer, LV_OBJ_FLAG_CLICKABLE);

        // 1. Icono de la App
        lv_obj_t* lblIcon = lv_label_create(iconContainer);
        lv_label_set_text(lblIcon, app.icon.c_str());
        lv_obj_set_style_text_color(lblIcon, lv_color_hex(app.accentColor), 0);
        lv_obj_set_style_text_font(lblIcon, &lv_font_montserrat_24, 0);
        lv_obj_center(lblIcon);

        // 2. Título de la App
        lv_obj_t* lblTitle = lv_label_create(card);
        lv_label_set_text(lblTitle, app.title.c_str());
        lv_obj_set_style_text_color(lblTitle, DefaultTheme::getTextColor(), 0);
        lv_obj_set_style_text_font(lblTitle, &lv_font_montserrat_14, 0);
        lv_obj_set_style_margin_top(lblTitle, 4, 0);
        lv_obj_set_style_text_align(lblTitle, LV_TEXT_ALIGN_CENTER, 0);

        // Evento de Click en la tarjeta pasando el puntero al AppItem
        lv_obj_add_event_cb(card, cardClickedEventCb, LV_EVENT_CLICKED, (void*)&app);

        m_cardObjs.push_back(card);
    }
}

void DashboardView::cardClickedEventCb(lv_event_t* e) {
    const AppItem* app = static_cast<const AppItem*>(lv_event_get_user_data(e));
    if (!app) return;

    if (app->isLuapp) {
        UIManager::getInstance().pushView(std::make_shared<LuappView>(app->luappPath, app->title, app->icon));
        return;
    }

    if (app->id == "browser") {
        UIManager::getInstance().pushView(std::make_shared<TlvBrowserView>());
    } else if (app->id == "gallery") {
        UIManager::getInstance().pushView(std::make_shared<GalleryListView>());
    } else if (app->id == "files") {
        UIManager::getInstance().pushView(std::make_shared<FileManagerView>());
    } else if (app->id == "utilities") {
        UIManager::getInstance().pushView(std::make_shared<UtilitiesView>());
    } else if (app->id == "cartridge") {
        UIManager::getInstance().pushView(std::make_shared<CartridgeView>());
    } else if (app->id == "lua") {
        UIManager::getInstance().pushView(std::make_shared<LuaRunnerView>());
    } else if (app->id == "editor") {
        UIManager::getInstance().pushView(std::make_shared<TextEditorView>());
    } else if (app->id == "radio") {
        UIManager::getInstance().pushView(std::make_shared<RadioView>());
    } else if (app->id == "config") {
        UIManager::getInstance().pushView(std::make_shared<ConfigView>());
    } else if (app->id == "music") {
        UIManager::getInstance().pushView(std::make_shared<MusicPlayerView>());
    } else if (app->id == "flasher") {
        UIManager::getInstance().pushView(std::make_shared<FlasherView>());
    } else if (app->id == "terminal") {
        UIManager::getInstance().pushView(std::make_shared<SerialTerminalView>());
    }
}

void DashboardView::onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) {
    if (!m_container || !lv_obj_is_valid(m_container)) return;
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
}

} // namespace ui
} // namespace cbdos
