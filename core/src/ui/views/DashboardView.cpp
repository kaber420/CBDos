#include "DashboardView.hpp"
#include "ConfigView.hpp"
#include "MusicPlayerView.hpp"
#include "FlasherView.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include "cbdos/display.hpp"
#include "cbdos/system.hpp"
#include <cstring>

namespace cbdos {
namespace ui {

static const char* TAG = "DashboardView";

DashboardView::DashboardView()
    : BaseView("Dashboard") {
    m_apps = {
        {"flasher", "Flasheador", LV_SYMBOL_DOWNLOAD, 0xF59E0B},
        {"music", "Musica", LV_SYMBOL_AUDIO, 0x00E5FF},
        {"config", "Configuracion", LV_SYMBOL_SETTINGS, 0x9D4EDD}
    };
}


bool DashboardView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;

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

    for (const auto& app : m_apps) {
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
        lv_label_set_text(lblIcon, app.icon);
        lv_obj_set_style_text_color(lblIcon, lv_color_hex(app.accentColor), 0);
        lv_obj_set_style_text_font(lblIcon, &lv_font_montserrat_24, 0);
        lv_obj_center(lblIcon);

        // 2. Título de la App
        lv_obj_t* lblTitle = lv_label_create(card);
        lv_label_set_text(lblTitle, app.title);
        lv_obj_set_style_text_color(lblTitle, DefaultTheme::getTextColor(), 0);
        lv_obj_set_style_text_font(lblTitle, &lv_font_montserrat_14, 0);
        lv_obj_set_style_margin_top(lblTitle, 4, 0);
        lv_obj_set_style_text_align(lblTitle, LV_TEXT_ALIGN_CENTER, 0);

        // Evento de Click en la tarjeta
        lv_obj_add_event_cb(card, cardClickedEventCb, LV_EVENT_CLICKED, (void*)app.id);

        m_cardObjs.push_back(card);
    }
}

void DashboardView::cardClickedEventCb(lv_event_t* e) {
    const char* appId = static_cast<const char*>(lv_event_get_user_data(e));
    if (appId) {
        if (strcmp(appId, "config") == 0) {
            UIManager::getInstance().pushView(std::make_shared<ConfigView>());
        } else if (strcmp(appId, "music") == 0) {
            UIManager::getInstance().pushView(std::make_shared<MusicPlayerView>());
        } else if (strcmp(appId, "flasher") == 0) {
            UIManager::getInstance().pushView(std::make_shared<FlasherView>());
        }
    }
}


void DashboardView::onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) {
    if (!m_container || !lv_obj_is_valid(m_container)) return;
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
}

} // namespace ui
} // namespace cbdos
