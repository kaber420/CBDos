#include "HeaderBar.hpp"
#include "ThemeEngine.hpp"
#include "../themes/DefaultTheme.h"
#include "cbdos/system.hpp"
#include "cbdos/network.hpp"
#include <cstdio>
#include <cstring>
#include <time.h>

namespace cbdos {
namespace ui {

HeaderBar::HeaderBar()
    : m_container(nullptr),
      m_labelTitle(nullptr),
      m_btnBack(nullptr),
      m_labelClock(nullptr),
      m_labelWifi(nullptr),
      m_onClickCb(nullptr),
      m_onBackCb(nullptr),
      m_lastUpdateMs(0) {
}

HeaderBar::~HeaderBar() {
    if (m_container && lv_obj_is_valid(m_container)) {
        lv_obj_delete(m_container);
        m_container = nullptr;
    }
}

bool HeaderBar::init(lv_obj_t* parent) {
    if (!parent) return false;

    const auto& palette = ThemeEngine::getInstance().getPalette();

    // 1. Contenedor Isla Flotante (44px de altura, redondeada y translúcida)
    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(94), 44);
    lv_obj_align(m_container, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_remove_flag(m_container, LV_OBJ_FLAG_SCROLLABLE);

    // Estilo Glassmorphism translúcido original
    lv_obj_set_style_bg_color(m_container, lv_color_hex(0x1B1E29), 0);
    lv_obj_set_style_bg_opa(m_container, LV_OPA_70, 0);
    lv_obj_set_style_border_color(m_container, lv_color_hex(0x3B4252), 0);
    lv_obj_set_style_border_width(m_container, 1, 0);
    lv_obj_set_style_radius(m_container, 14, 0);
    lv_obj_set_style_pad_hor(m_container, 14, 0);
    lv_obj_set_style_pad_ver(m_container, 0, 0);

    // Layout Flex horizontal
    lv_obj_set_flex_flow(m_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(m_container, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // 2. Izquierda: Contenedor Izquierdo (Título o Botón Volver)
    lv_obj_t* leftBox = lv_obj_create(m_container);
    lv_obj_remove_flag(leftBox, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(leftBox, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(leftBox, 0, 0);
    lv_obj_set_style_pad_all(leftBox, 0, 0);
    lv_obj_set_size(leftBox, LV_SIZE_CONTENT, LV_PCT(100));
    lv_obj_set_flex_flow(leftBox, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(leftBox, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    m_btnBack = lv_button_create(leftBox);
    lv_obj_set_size(m_btnBack, 84, 30);
    DefaultTheme::applyButton(m_btnBack, 10);
    lv_obj_t* lblBack = lv_label_create(m_btnBack);
    lv_label_set_text(lblBack, LV_SYMBOL_LEFT " Volver");
    lv_obj_set_style_text_color(lblBack, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_set_style_text_font(lblBack, &lv_font_montserrat_14, 0);
    lv_obj_center(lblBack);
    lv_obj_add_event_cb(m_btnBack, backBtnEventHandler, LV_EVENT_CLICKED, this);
    lv_obj_add_flag(m_btnBack, LV_OBJ_FLAG_HIDDEN);

    m_labelTitle = lv_label_create(leftBox);
    lv_label_set_text(m_labelTitle, "CBDos");
    lv_obj_set_style_text_color(m_labelTitle, lv_color_hex(palette.textPrimary), 0);
    lv_obj_set_style_text_font(m_labelTitle, &lv_font_montserrat_14, 0);

    // 3. Centro: Contenedor táctil central (Solo tocar aquí abre los Accesos Rápidos)
    lv_obj_t* centerBox = lv_obj_create(m_container);
    lv_obj_remove_flag(centerBox, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(centerBox, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(centerBox, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(centerBox, 0, 0);
    lv_obj_set_style_pad_hor(centerBox, 24, 0);
    lv_obj_set_style_pad_ver(centerBox, 0, 0);
    lv_obj_set_size(centerBox, LV_SIZE_CONTENT, LV_PCT(100));
    lv_obj_set_flex_flow(centerBox, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(centerBox, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    m_labelClock = lv_label_create(centerBox);
    lv_label_set_text(m_labelClock, "--:--");
    lv_obj_set_style_text_color(m_labelClock, lv_color_hex(palette.textPrimary), 0);
    lv_obj_set_style_text_font(m_labelClock, &lv_font_montserrat_16, 0);

    // Evento de click EXCLUSIVO en la zona central
    lv_obj_add_event_cb(centerBox, eventHandler, LV_EVENT_CLICKED, this);

    // 4. Derecha: WiFi Status
    m_labelWifi = lv_label_create(m_container);
    lv_label_set_text(m_labelWifi, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(m_labelWifi, lv_color_hex(0x10B981), 0);
    lv_obj_set_style_text_font(m_labelWifi, &lv_font_montserrat_16, 0);

    update();
    return true;
}

void HeaderBar::setTitle(const char* title) {
    if (m_labelTitle && lv_obj_is_valid(m_labelTitle) && title) {
        lv_label_set_text(m_labelTitle, title);
    }
}

void HeaderBar::showBackButton(bool show, ClickCallback onBack) {
    m_onBackCb = onBack;
    if (m_btnBack && lv_obj_is_valid(m_btnBack)) {
        if (show) {
            lv_obj_remove_flag(m_btnBack, LV_OBJ_FLAG_HIDDEN);
            if (m_labelTitle) lv_obj_add_flag(m_labelTitle, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(m_btnBack, LV_OBJ_FLAG_HIDDEN);
            if (m_labelTitle) lv_obj_remove_flag(m_labelTitle, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void HeaderBar::eventHandler(lv_event_t* e) {
    auto* self = static_cast<HeaderBar*>(lv_event_get_user_data(e));
    if (self && self->m_onClickCb) {
        self->m_onClickCb();
    }
}

void HeaderBar::backBtnEventHandler(lv_event_t* e) {
    auto* self = static_cast<HeaderBar*>(lv_event_get_user_data(e));
    if (self && self->m_onBackCb) {
        self->m_onBackCb();
    }
}

void HeaderBar::setOnClickCallback(ClickCallback cb) {
    m_onClickCb = cb;
}

void HeaderBar::update() {
    uint32_t now = cbdos::system::getTimeMs();
    if (now - m_lastUpdateMs < 5000 && m_lastUpdateMs != 0) {
        return; // Actualización cada 5 segundos
    }
    m_lastUpdateMs = now;

    if (!m_container || !lv_obj_is_valid(m_container)) return;

    // 1. Reloj NTP Real (HH:MM)
    time_t rawtime;
    time(&rawtime);
    struct tm* timeinfo = localtime(&rawtime);

    char clockBuf[16];
    if (timeinfo && timeinfo->tm_year > (2020 - 1900)) {
        snprintf(clockBuf, sizeof(clockBuf), "%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min);
    } else {
        snprintf(clockBuf, sizeof(clockBuf), "--:--");
    }

    if (m_labelClock && lv_obj_is_valid(m_labelClock)) {
        const char* currentText = lv_label_get_text(m_labelClock);
        if (!currentText || strcmp(currentText, clockBuf) != 0) {
            lv_label_set_text(m_labelClock, clockBuf);
        }
    }

    // 2. Actualizar WiFi
    if (m_labelWifi && lv_obj_is_valid(m_labelWifi)) {
        bool connected = cbdos::network::isConnected();
        lv_label_set_text(m_labelWifi, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_color(m_labelWifi, lv_color_hex(connected ? 0x10B981 : 0x64748B), 0);
    }
}

void HeaderBar::onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) {
    if (!m_container || !lv_obj_is_valid(m_container)) return;

    if (m_labelTitle && lv_obj_is_valid(m_labelTitle)) {
        lv_obj_set_style_text_color(m_labelTitle, lv_color_hex(palette.textPrimary), 0);
    }
    if (m_labelClock && lv_obj_is_valid(m_labelClock)) {
        lv_obj_set_style_text_color(m_labelClock, lv_color_hex(palette.textPrimary), 0);
    }
}

} // namespace ui
} // namespace cbdos
