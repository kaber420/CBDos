#include "PowerConfigView.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include "PowerManager.hpp"
#include <cstdio>

namespace cbdos {
namespace ui {

PowerConfigView::PowerConfigView()
    : BaseView("Sistema y Energia") {
}

void PowerConfigView::screen_off_btn_cb(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        cbdos::system::PowerManager::getInstance().turnOffScreen();
        UIManager::showToast("Pantalla Apagada (Toque para encender)");
    }
}

void PowerConfigView::light_sleep_btn_cb(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        UIManager::showToast("Entrando a Suspesion (Light Sleep)...");
        cbdos::system::PowerManager::getInstance().enterLightSleep();
    }
}

void PowerConfigView::deep_sleep_btn_cb(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        UIManager::showToast("Entrando a Suspesion Profunda (Deep Sleep)...");
        cbdos::system::PowerManager::getInstance().enterDeepSleep();
    }
}

void PowerConfigView::restart_btn_cb(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        UIManager::showToast("Reiniciando sistema...");
        cbdos::system::PowerManager::getInstance().restart();
    }
}

void PowerConfigView::timeout_dropdown_cb(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t* dd = (lv_obj_t*)lv_event_get_target(e);
        uint16_t sel = lv_dropdown_get_selected(dd);
        uint32_t timeoutSec = 0;
        
        switch (sel) {
            case 0: timeoutSec = 0; break;     // Desactivado
            case 1: timeoutSec = 60; break;    // 1 Minuto
            case 2: timeoutSec = 180; break;   // 3 Minutos
            case 3: timeoutSec = 300; break;   // 5 Minutos
            case 4: timeoutSec = 600; break;   // 10 Minutos
            default: timeoutSec = 60; break;
        }

        cbdos::system::PowerManager::getInstance().setIdleTimeoutSec(timeoutSec);
        if (timeoutSec == 0) {
            UIManager::showToast("Auto-Apagado de Pantalla Desactivado");
        } else {
            char msg[64];
            snprintf(msg, sizeof(msg), "Auto-Apagado Pantalla: %u min", (unsigned int)(timeoutSec / 60));
            UIManager::showToast(msg);
        }
    }
}

bool PowerConfigView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;

    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(m_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_container, 8, 0);
    lv_obj_set_style_pad_row(m_container, 12, 0);
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_container, 0, 0);

    // ─── SECCIÓN 1: Controles de Inactividad ───
    lv_obj_t* sec1Lbl = lv_label_create(m_container);
    lv_label_set_text(sec1Lbl, "Ahorro de Bateria por Inactividad:");
    lv_obj_set_style_text_color(sec1Lbl, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(sec1Lbl, &lv_font_montserrat_14, 0);

    lv_obj_t* cardTout = lv_obj_create(m_container);
    lv_obj_set_width(cardTout, lv_pct(100));
    DefaultTheme::applyRaisedCard(cardTout, 12);
    lv_obj_set_style_pad_all(cardTout, 14, 0);
    lv_obj_set_flex_flow(cardTout, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cardTout, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* lblTout = lv_label_create(cardTout);
    lv_label_set_text(lblTout, "Auto-Apagado de Pantalla");
    lv_obj_set_style_text_color(lblTout, DefaultTheme::getTextColor(), 0);

    lv_obj_t* ddTout = lv_dropdown_create(cardTout);
    lv_dropdown_set_options(ddTout, "Desactivado\n1 Minuto\n3 Minutos\n5 Minutos\n10 Minutos");

    uint32_t currentSec = cbdos::system::PowerManager::getInstance().getIdleTimeoutSec();
    uint16_t currentIdx = 1; // 1 min por defecto
    if (currentSec == 0) currentIdx = 0;
    else if (currentSec == 60) currentIdx = 1;
    else if (currentSec == 180) currentIdx = 2;
    else if (currentSec == 300) currentIdx = 3;
    else if (currentSec == 600) currentIdx = 4;

    lv_dropdown_set_selected(ddTout, currentIdx);
    lv_obj_add_event_cb(ddTout, timeout_dropdown_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // ─── SECCIÓN 2: Acciones Directas de Energía ───
    lv_obj_t* sec2Lbl = lv_label_create(m_container);
    lv_label_set_text(sec2Lbl, "Acciones de Energia:");
    lv_obj_set_style_text_color(sec2Lbl, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(sec2Lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_margin_top(sec2Lbl, 10, 0);

    // Botón 1: Apagar Pantalla (Mantener tareas de fondo)
    lv_obj_t* btnScreenOff = lv_button_create(m_container);
    lv_obj_set_width(btnScreenOff, lv_pct(100));
    lv_obj_set_height(btnScreenOff, 54);
    DefaultTheme::applyButton(btnScreenOff, 14);
    lv_obj_add_event_cb(btnScreenOff, screen_off_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_set_flex_flow(btnScreenOff, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btnScreenOff, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(btnScreenOff, 16, 0);

    lv_obj_t* iconScrOff = lv_label_create(btnScreenOff);
    lv_label_set_text(iconScrOff, LV_SYMBOL_EYE_CLOSE);
    lv_obj_set_style_text_color(iconScrOff, lv_color_hex(0x00e5ff), 0);
    lv_obj_set_style_margin_right(iconScrOff, 12, 0);

    lv_obj_t* lblScrOff = lv_label_create(btnScreenOff);
    lv_label_set_text(lblScrOff, "Apagar Pantalla (Mantener tareas fondo)");
    lv_obj_set_style_text_color(lblScrOff, DefaultTheme::getTextColor(), 0);

    // Botón 2: Suspensión (Light Sleep)
    lv_obj_t* btnLightSleep = lv_button_create(m_container);
    lv_obj_set_width(btnLightSleep, lv_pct(100));
    lv_obj_set_height(btnLightSleep, 54);
    DefaultTheme::applyButton(btnLightSleep, 14);
    lv_obj_add_event_cb(btnLightSleep, light_sleep_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_set_flex_flow(btnLightSleep, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btnLightSleep, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(btnLightSleep, 16, 0);

    lv_obj_t* iconLight = lv_label_create(btnLightSleep);
    lv_label_set_text(iconLight, LV_SYMBOL_PAUSE);
    lv_obj_set_style_text_color(iconLight, lv_color_hex(0xa78bfa), 0);
    lv_obj_set_style_margin_right(iconLight, 12, 0);

    lv_obj_t* lblLight = lv_label_create(btnLightSleep);
    lv_label_set_text(lblLight, "Suspesion (Light Sleep)");
    lv_obj_set_style_text_color(lblLight, DefaultTheme::getTextColor(), 0);

    // Botón 3: Suspensión Profunda (Deep Sleep - LP Core)
    lv_obj_t* btnDeepSleep = lv_button_create(m_container);
    lv_obj_set_width(btnDeepSleep, lv_pct(100));
    lv_obj_set_height(btnDeepSleep, 54);
    DefaultTheme::applyButton(btnDeepSleep, 14);
    lv_obj_add_event_cb(btnDeepSleep, deep_sleep_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_set_flex_flow(btnDeepSleep, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btnDeepSleep, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(btnDeepSleep, 16, 0);

    lv_obj_t* iconDeep = lv_label_create(btnDeepSleep);
    lv_label_set_text(iconDeep, LV_SYMBOL_POWER);
    lv_obj_set_style_text_color(iconDeep, lv_color_hex(0xef4444), 0);
    lv_obj_set_style_margin_right(iconDeep, 12, 0);

    lv_obj_t* lblDeep = lv_label_create(btnDeepSleep);
    lv_label_set_text(lblDeep, "Suspesion Profunda (Deep Sleep - LP Core)");
    lv_obj_set_style_text_color(lblDeep, DefaultTheme::getTextColor(), 0);

    // Botón 4: Reiniciar Sistema
    lv_obj_t* btnRestart = lv_button_create(m_container);
    lv_obj_set_width(btnRestart, lv_pct(100));
    lv_obj_set_height(btnRestart, 54);
    DefaultTheme::applyButton(btnRestart, 14);
    lv_obj_add_event_cb(btnRestart, restart_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_set_flex_flow(btnRestart, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btnRestart, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(btnRestart, 16, 0);

    lv_obj_t* iconRst = lv_label_create(btnRestart);
    lv_label_set_text(iconRst, LV_SYMBOL_REFRESH);
    lv_obj_set_style_text_color(iconRst, lv_color_hex(0xfb8500), 0);
    lv_obj_set_style_margin_right(iconRst, 12, 0);

    lv_obj_t* lblRst = lv_label_create(btnRestart);
    lv_label_set_text(lblRst, "Reiniciar Sistema (Reboot)");
    lv_obj_set_style_text_color(lblRst, DefaultTheme::getTextColor(), 0);

    return true;
}

} // namespace ui
} // namespace cbdos
