#include "WallpaperConfigView.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include "../WallpaperManager.h"
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>

namespace cbdos {
namespace ui {

WallpaperConfigView::WallpaperConfigView()
    : BaseView("Fondo de Pantalla") {
}

void WallpaperConfigView::default_btn_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        WallpaperManager::getInstance().restoreDefault();
        UIManager::showToast("Fondo predeterminado aplicado");
        UIManager::getInstance().popView();
    }
}

void WallpaperConfigView::animated_btn_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
        const char* mode = (const char*)lv_obj_get_user_data(btn);
        if (!mode) mode = "animated_constellation";
        WallpaperManager::getInstance().setWallpaper(mode);
        UIManager::showToast("Fondo Animado Vectorial activado");
        UIManager::getInstance().popView();
    }
}

void WallpaperConfigView::wallpaper_select_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
        const char* path = (const char*)lv_obj_get_user_data(btn);
        if (path) {
            bool ok = WallpaperManager::getInstance().setWallpaper(path);
            if (ok) {
                UIManager::showToast("Fondo copiado a Flash y aplicado");
            } else {
                UIManager::showToast("Error: Formato no compatible");
            }
            UIManager::getInstance().popView();
        }
    }
}

bool WallpaperConfigView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;

    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(m_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_container, 8, 0);
    lv_obj_set_style_pad_row(m_container, 10, 0);
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_container, 0, 0);

    // 1. Botón Fondo Animado: Constelación Neón
    lv_obj_t* animConstCard = lv_button_create(m_container);
    lv_obj_set_width(animConstCard, lv_pct(100));
    lv_obj_set_height(animConstCard, 54);
    DefaultTheme::applyButton(animConstCard, 14);
    lv_obj_set_user_data(animConstCard, (void*)"animated_constellation");
    lv_obj_add_event_cb(animConstCard, animated_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_set_flex_flow(animConstCard, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(animConstCard, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(animConstCard, 16, 0);

    lv_obj_t* iconConst = lv_label_create(animConstCard);
    lv_label_set_text(iconConst, LV_SYMBOL_REFRESH);
    lv_obj_set_style_text_color(iconConst, lv_color_hex(0x00e5ff), 0);
    lv_obj_set_style_margin_right(iconConst, 12, 0);

    lv_obj_t* lblConst = lv_label_create(animConstCard);
    lv_label_set_text(lblConst, "Animado: Constelación Neón");
    lv_obj_set_style_text_color(lblConst, DefaultTheme::getTextColor(), 0);

    // 2. Botón Fondo Animado: Ondas Neón
    lv_obj_t* animWavesCard = lv_button_create(m_container);
    lv_obj_set_width(animWavesCard, lv_pct(100));
    lv_obj_set_height(animWavesCard, 54);
    DefaultTheme::applyButton(animWavesCard, 14);
    lv_obj_set_user_data(animWavesCard, (void*)"animated_waves");
    lv_obj_add_event_cb(animWavesCard, animated_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_set_flex_flow(animWavesCard, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(animWavesCard, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(animWavesCard, 16, 0);

    lv_obj_t* iconWaves = lv_label_create(animWavesCard);
    lv_label_set_text(iconWaves, LV_SYMBOL_CHARGE);
    lv_obj_set_style_text_color(iconWaves, lv_color_hex(0x9d4edd), 0);
    lv_obj_set_style_margin_right(iconWaves, 12, 0);

    lv_obj_t* lblWaves = lv_label_create(animWavesCard);
    lv_label_set_text(lblWaves, "Animado: Ondas Neón");
    lv_obj_set_style_text_color(lblWaves, DefaultTheme::getTextColor(), 0);

    // 2. Botón Fondo Predeterminado de Fábrica
    lv_obj_t* defaultCard = lv_button_create(m_container);
    lv_obj_set_width(defaultCard, lv_pct(100));
    lv_obj_set_height(defaultCard, 54);
    DefaultTheme::applyButton(defaultCard, 14);
    lv_obj_add_event_cb(defaultCard, default_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_set_flex_flow(defaultCard, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(defaultCard, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(defaultCard, 16, 0);

    lv_obj_t* iconDef = lv_label_create(defaultCard);
    lv_label_set_text(iconDef, LV_SYMBOL_IMAGE);
    lv_obj_set_style_text_color(iconDef, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_set_style_margin_right(iconDef, 12, 0);

    lv_obj_t* lblDef = lv_label_create(defaultCard);
    lv_label_set_text(lblDef, "Fondo Predeterminado (Flash)");
    lv_obj_set_style_text_color(lblDef, DefaultTheme::getTextColor(), 0);

    // 2. Separador de sección
    lv_obj_t* secLbl = lv_label_create(m_container);
    lv_label_set_text(secLbl, "Fondos en MicroSD (/wallpapers/):");
    lv_obj_set_style_text_color(secLbl, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(secLbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_margin_top(secLbl, 10, 0);

    // 3. Obtener lista de wallpapers desde la MicroSD
    std::vector<std::string> wallpapers = WallpaperManager::getInstance().getAvailableWallpapers();

    if (wallpapers.empty()) {
        lv_obj_t* emptyCard = lv_obj_create(m_container);
        lv_obj_set_width(emptyCard, lv_pct(100));
        DefaultTheme::applyRaisedCard(emptyCard, 12);
        lv_obj_set_style_pad_all(emptyCard, 14, 0);

        lv_obj_t* emptyLbl = lv_label_create(emptyCard);
        lv_label_set_text(emptyLbl, "No se encontraron imagenes en\n/wallpapers/ (*.bin, *.bmp, *.jpg)");
        lv_obj_set_style_text_color(emptyLbl, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_align(emptyLbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(emptyLbl);
    } else {
        for (size_t i = 0; i < wallpapers.size(); i++) {
            const std::string& fullPath = wallpapers[i];
            
            size_t slashPos = fullPath.find_last_of('/');
            std::string filename = (slashPos != std::string::npos) ? fullPath.substr(slashPos + 1) : fullPath;

            lv_obj_t* card = lv_button_create(m_container);
            lv_obj_set_width(card, lv_pct(100));
            lv_obj_set_height(card, 54);
            DefaultTheme::applyButton(card, 14);

            char* pathCopy = strdup(fullPath.c_str());
            lv_obj_set_user_data(card, (void*)pathCopy);
            lv_obj_add_event_cb(card, wallpaper_select_cb, LV_EVENT_CLICKED, NULL);

            lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_hor(card, 16, 0);

            lv_obj_t* icon = lv_label_create(card);
            lv_label_set_text(icon, LV_SYMBOL_FILE);
            lv_obj_set_style_text_color(icon, DefaultTheme::getSecondaryAccent(), 0);
            lv_obj_set_style_margin_right(icon, 12, 0);

            lv_obj_t* lbl = lv_label_create(card);
            lv_label_set_text(lbl, filename.c_str());
            lv_obj_set_style_text_color(lbl, DefaultTheme::getTextColor(), 0);
        }
    }

    return true;
}

} // namespace ui
} // namespace cbdos
