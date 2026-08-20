#include "SplashScreenView.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include "cbdos/display.hpp"
#include <cstdio>

namespace cbdos {
namespace ui {

SplashScreenView::SplashScreenView()
    : BaseView("Splash"), m_timer(nullptr) {
}

void SplashScreenView::onDestroy() {
    if (m_timer) {
        lv_timer_delete(m_timer);
        m_timer = nullptr;
    }
    BaseView::onDestroy();
}

void SplashScreenView::splash_timer_cb(lv_timer_t* timer) {
    auto* self = static_cast<SplashScreenView*>(lv_timer_get_user_data(timer));
    if (self) {
        self->m_timer = nullptr;
        lv_timer_delete(timer);
    }
    UIManager::getInstance().openDashboard();
}

bool SplashScreenView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;

    auto caps = cbdos::display::getCapabilities();

    // Contenedor de pantalla completa en negro puro sólido (#000000)
    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(m_container, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(m_container, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(m_container, 0, 0);
    lv_obj_set_style_pad_all(m_container, 20, 0);
    lv_obj_remove_flag(m_container, LV_OBJ_FLAG_SCROLLABLE);

    // Layout centrado verticalmente
    lv_obj_set_flex_flow(m_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(m_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(m_container, 10, 0);

    // 1. Icono de Arranque estilizado
    lv_obj_t* iconBox = lv_obj_create(m_container);
    lv_obj_set_size(iconBox, 68, 68);
    lv_obj_set_style_bg_color(iconBox, lv_color_hex(0x111827), 0);
    lv_obj_set_style_bg_opa(iconBox, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(iconBox, lv_color_hex(0x00F5D4), 0);
    lv_obj_set_style_border_width(iconBox, 2, 0);
    lv_obj_set_style_radius(iconBox, 34, 0);
    lv_obj_set_style_pad_all(iconBox, 0, 0);
    lv_obj_remove_flag(iconBox, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lblIcon = lv_label_create(iconBox);
    lv_label_set_text(lblIcon, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_color(lblIcon, lv_color_hex(0x00F5D4), 0);
    lv_obj_set_style_text_font(lblIcon, &lv_font_montserrat_24, 0);
    lv_obj_center(lblIcon);

    // 2. Título Principal
    lv_obj_t* lblTitle = lv_label_create(m_container);
    lv_label_set_text(lblTitle, "CyBerDeck OS");
    lv_obj_set_style_text_color(lblTitle, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(lblTitle, &lv_font_montserrat_24, 0);
    lv_obj_set_style_margin_top(lblTitle, 8, 0);

    // 3. Subtítulo de Versión
    lv_obj_t* lblSub = lv_label_create(m_container);
    lv_label_set_text(lblSub, "v0.2.0 - Universal Core");
    lv_obj_set_style_text_color(lblSub, lv_color_hex(0x9D4EDD), 0);
    lv_obj_set_style_text_font(lblSub, &lv_font_montserrat_14, 0);

    // 4. Target de Hardware Detectado
    lv_obj_t* lblTarget = lv_label_create(m_container);
    if (caps.width >= 480) {
        lv_label_set_text(lblTarget, "ESP32-P4 (480x800 MIPI-DSI @ 60 FPS)");
    } else {
        lv_label_set_text(lblTarget, "ESP32-S3 (320x480 QSPI @ 30 FPS)");
    }
    lv_obj_set_style_text_color(lblTarget, lv_color_hex(0x64748B), 0);
    lv_obj_set_style_text_font(lblTarget, &lv_font_montserrat_12, 0);
    lv_obj_set_style_margin_top(lblTarget, 16, 0);

    // 5. Barra de Carga Minimalista
    lv_obj_t* bar = lv_bar_create(m_container);
    lv_obj_set_size(bar, (caps.width >= 480) ? 240 : 180, 4);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, 100, LV_ANIM_ON);
    lv_obj_set_style_anim_duration(bar, 1400, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x1F2937), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x00F5D4), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 2, LV_PART_INDICATOR);
    lv_obj_set_style_margin_top(bar, 12, 0);

    // Temporizador de 1.5 segundos para saltar al Dashboard
    m_timer = lv_timer_create(splash_timer_cb, 1500, this);
    return true;
}

} // namespace ui
} // namespace cbdos
