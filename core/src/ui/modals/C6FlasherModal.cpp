#include "C6FlasherModal.hpp"
#include "../themes/DefaultTheme.h"
#include "../UIManager.hpp"
#include "cbdos/system.hpp"
#include "cbdos/flasher.hpp"
#include <cstdio>

namespace cbdos {
namespace ui {

lv_obj_t* C6FlasherModal::s_modalMask = nullptr;
lv_obj_t* C6FlasherModal::s_barProgress = nullptr;
lv_obj_t* C6FlasherModal::s_lblStatus = nullptr;
lv_obj_t* C6FlasherModal::s_btnStart = nullptr;
lv_obj_t* C6FlasherModal::s_lblBtn = nullptr;

bool C6FlasherModal::isVisible() {
    return s_modalMask != nullptr;
}

void C6FlasherModal::close_btn_cb(lv_event_t* e) {
    hide();
}

void C6FlasherModal::start_flash_cb(lv_event_t* e) {
    if (cbdos::flasher::isBusy()) {
        return;
    }

    if (s_btnStart) {
        lv_obj_add_state(s_btnStart, LV_STATE_DISABLED);
    }
    if (s_lblBtn) {
        lv_label_set_text(s_lblBtn, "Flasheando...");
    }
    if (s_barProgress) {
        lv_bar_set_value(s_barProgress, 0, LV_ANIM_OFF);
    }
    if (s_lblStatus) {
        lv_label_set_text(s_lblStatus, "Iniciando proceso autónomo...");
        lv_obj_set_style_text_color(s_lblStatus, lv_color_hex(0x60A5FA), 0);
    }

    cbdos::flasher::startFlash([](cbdos::flasher::FlasherStatus status, int percent, const char* message) {
        if (s_barProgress) {
            lv_bar_set_value(s_barProgress, percent, LV_ANIM_ON);
        }
        if (s_lblStatus && message) {
            lv_label_set_text(s_lblStatus, message);
            if (status == cbdos::flasher::FlasherStatus::Failed) {
                lv_obj_set_style_text_color(s_lblStatus, lv_color_hex(0xEF4444), 0);
            } else if (status == cbdos::flasher::FlasherStatus::Success) {
                lv_obj_set_style_text_color(s_lblStatus, lv_color_hex(0x10B981), 0);
            } else {
                lv_obj_set_style_text_color(s_lblStatus, lv_color_hex(0x60A5FA), 0);
            }
        }
        if (status == cbdos::flasher::FlasherStatus::Success || status == cbdos::flasher::FlasherStatus::Failed) {
            if (s_btnStart) {
                lv_obj_remove_state(s_btnStart, LV_STATE_DISABLED);
            }
            if (s_lblBtn) {
                lv_label_set_text(s_lblBtn, (status == cbdos::flasher::FlasherStatus::Success) ? "Flashear Nuevamente" : "Reintentar Flasheo");
            }
        }
    });
}


void C6FlasherModal::show(lv_obj_t* parent) {
    if (s_modalMask) return;

    if (!parent) {
        parent = lv_screen_active();
    }
    if (!parent) return;

    // 1. Fondo oscurecido con efecto modal
    s_modalMask = lv_obj_create(parent);
    lv_obj_set_size(s_modalMask, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(s_modalMask, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_modalMask, LV_OPA_70, 0);
    lv_obj_set_style_border_width(s_modalMask, 0, 0);
    lv_obj_set_style_pad_all(s_modalMask, 16, 0);
    lv_obj_center(s_modalMask);
    lv_obj_set_flex_flow(s_modalMask, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_modalMask, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // 2. Tarjeta principal del modal
    lv_obj_t* card = lv_obj_create(s_modalMask);
    lv_obj_set_width(card, lv_pct(95));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    DefaultTheme::applyRaisedCard(card, 16);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(card, 16, 0);
    lv_obj_set_style_pad_row(card, 12, 0);

    // Cabecera: Título y Botón Cerrar
    lv_obj_t* header = lv_obj_create(card);
    lv_obj_set_width(header, lv_pct(100));
    lv_obj_set_height(header, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(header, 0, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(header, 0, 0);
    DefaultTheme::disableScroll(header);

    lv_obj_t* lblTitle = lv_label_create(header);
    lv_label_set_text(lblTitle, "Flasheador Autónomo C6");
    lv_obj_set_style_text_font(lblTitle, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lblTitle, DefaultTheme::getPrimaryAccent(), 0);

    lv_obj_t* btnClose = lv_button_create(header);
    lv_obj_set_size(btnClose, 32, 32);
    DefaultTheme::applyButton(btnClose, 8);
    lv_obj_set_style_bg_color(btnClose, lv_color_hex(0x334155), 0);
    lv_obj_add_event_cb(btnClose, close_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* lblClose = lv_label_create(btnClose);
    lv_label_set_text(lblClose, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(lblClose, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(lblClose);

    // Cuadro de Conexiones Requeridas
    lv_obj_t* infoBox = lv_obj_create(card);
    lv_obj_set_width(infoBox, lv_pct(100));
    lv_obj_set_height(infoBox, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(infoBox, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_border_color(infoBox, lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(infoBox, 1, 0);
    lv_obj_set_style_radius(infoBox, 8, 0);
    lv_obj_set_style_pad_all(infoBox, 10, 0);
    lv_obj_set_flex_flow(infoBox, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(infoBox, 4, 0);
    DefaultTheme::disableScroll(infoBox);

    lv_obj_t* lblInfoTitle = lv_label_create(infoBox);
    lv_label_set_text(lblInfoTitle, "Conexiones en Conector JP1 (2x13):");
    lv_obj_set_style_text_font(lblInfoTitle, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblInfoTitle, DefaultTheme::getSecondaryAccent(), 0);

    lv_obj_t* lblPinMap = lv_label_create(infoBox);
    lv_label_set_text(lblPinMap, 
        "• 3V3 (Pin 1) ──> ESP_3V3 (Pin 18) [Poder]\n"
        "• GPIO34 (Pin 17) ──> C6_IO9 (Pin 24) [Auto-Boot]\n"
        "• GPIO32 (Pin 19) ──[JUMPER]──> C6_U0RXD (Pin 20)\n"
        "• GPIO28 (Pin 21) ──[JUMPER]──> C6_U0TXD (Pin 22)");
    lv_obj_set_style_text_font(lblPinMap, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblPinMap, DefaultTheme::getTextColor(), 0);

    // Barra de Progreso
    s_barProgress = lv_bar_create(card);
    lv_obj_set_width(s_barProgress, lv_pct(100));
    lv_obj_set_height(s_barProgress, 14);
    lv_bar_set_range(s_barProgress, 0, 100);
    lv_bar_set_value(s_barProgress, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_barProgress, lv_color_hex(0x1E293B), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_barProgress, DefaultTheme::getPrimaryAccent(), LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_barProgress, 6, LV_PART_MAIN);
    lv_obj_set_style_radius(s_barProgress, 6, LV_PART_INDICATOR);

    // Texto de Estado
    s_lblStatus = lv_label_create(card);
    lv_label_set_text(s_lblStatus, "Listo para iniciar. Firmware SDIO Slave embebido listo.");
    lv_obj_set_style_text_font(s_lblStatus, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s_lblStatus, DefaultTheme::getMutedTextColor(), 0);

    // Botón Iniciar Flasheo
    s_btnStart = lv_button_create(card);
    lv_obj_set_width(s_btnStart, lv_pct(100));
    lv_obj_set_height(s_btnStart, 46);
    DefaultTheme::applyButton(s_btnStart, 12);
    lv_obj_set_style_bg_color(s_btnStart, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_add_event_cb(s_btnStart, start_flash_cb, LV_EVENT_CLICKED, NULL);

    s_lblBtn = lv_label_create(s_btnStart);
    lv_label_set_text(s_lblBtn, "Iniciar Flasheo Autónomo (SDIO)");
    lv_obj_set_style_text_font(s_lblBtn, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_lblBtn, lv_color_hex(0x0F172A), 0);
    lv_obj_center(s_lblBtn);
}

void C6FlasherModal::hide() {
    if (s_modalMask) {
        lv_obj_delete(s_modalMask);
        s_modalMask = nullptr;
        s_barProgress = nullptr;
        s_lblStatus = nullptr;
        s_btnStart = nullptr;
        s_lblBtn = nullptr;
    }
}

} // namespace ui
} // namespace cbdos
