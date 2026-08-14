#include "WallpaperConfigView.h"
#include "../UIManager.h"
#include "../Themes/DefaultTheme.h"
#include "../WallpaperManager.h"
#include <cstdio>

HeaderBar* WallpaperConfigView::headerBar = nullptr;

void WallpaperConfigView::default_btn_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        WallpaperManager::getInstance().restoreDefault();
        UIManager::showToast("Fondo predeterminado aplicado");
        UIManager::getInstance().loadLauncher();
    }
}

void WallpaperConfigView::wallpaper_select_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
        const char* path = (const char*)lv_obj_get_user_data(btn);
        if (path) {
            WallpaperManager::getInstance().setWallpaper(path);
            UIManager::showToast("Fondo de pantalla actualizado");
            UIManager::getInstance().loadLauncher();
        }
    }
}

lv_obj_t* WallpaperConfigView::create() {
    lv_obj_t* screen = lv_obj_create(NULL);
    DefaultTheme::applyFlatBg(screen);
    DefaultTheme::disableScroll(screen);

    WallpaperManager::getInstance().applyWallpaper(screen);

    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(screen, 12, 0);
    lv_obj_set_style_pad_row(screen, 10, 0);

    headerBar = HeaderBar::create(screen, "Fondo de Pantalla", true, true);

    lv_obj_t* content = lv_obj_create(screen);
    lv_obj_set_width(content, lv_pct(100));
    lv_obj_set_flex_grow(content, 1);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(content, 4, 0);
    lv_obj_set_style_pad_row(content, 10, 0);
    lv_obj_set_style_bg_opa(content, 0, 0);
    lv_obj_set_style_border_width(content, 0, 0);

    // Botón Fondo Predeterminado
    lv_obj_t* defaultCard = lv_button_create(content);
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

    // Separador / Título de sección
    lv_obj_t* secLbl = lv_label_create(content);
    lv_label_set_text(secLbl, "Fondos en MicroSD (/wallpapers/):");
    lv_obj_set_style_text_color(secLbl, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(secLbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_margin_top(secLbl, 10, 0);

    // Obtener lista de wallpapers de la SD
    std::vector<std::string> wallpapers = WallpaperManager::getInstance().getAvailableWallpapers();

    if (wallpapers.empty()) {
        lv_obj_t* emptyCard = lv_obj_create(content);
        lv_obj_set_width(emptyCard, lv_pct(100));
        DefaultTheme::applyRaisedCard(emptyCard, 12);
        lv_obj_set_style_pad_all(emptyCard, 14, 0);

        lv_obj_t* emptyLbl = lv_label_create(emptyCard);
        lv_label_set_text(emptyLbl, "No se encontraron imagenes en\nS:/wallpapers/*.jpg");
        lv_obj_set_style_text_color(emptyLbl, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_align(emptyLbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(emptyLbl);
    } else {
        for (size_t i = 0; i < wallpapers.size(); i++) {
            const std::string& fullPath = wallpapers[i];
            
            // Extraer solo el nombre de archivo
            size_t slashPos = fullPath.find_last_of('/');
            std::string filename = (slashPos != std::string::npos) ? fullPath.substr(slashPos + 1) : fullPath;

            lv_obj_t* card = lv_button_create(content);
            lv_obj_set_width(card, lv_pct(100));
            lv_obj_set_height(card, 54);
            DefaultTheme::applyButton(card, 14);

            // Guardamos puntero a string estático o duplicado en heap
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

    return screen;
}
