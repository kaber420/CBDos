#include "DashboardView.h"
#include "../UIManager.h"
#include "../Themes/DefaultTheme.h"
#include <cstdio>

HeaderBar* DashboardView::headerBar = nullptr;
DashboardView::CommandCallback DashboardView::commandCb = nullptr;
lv_obj_t* DashboardView::ordersBtnLabel = nullptr;
lv_obj_t* DashboardView::ordersBtnIcon = nullptr;

void DashboardView::btn_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * btn = (lv_obj_t *)lv_event_get_target(e);
    
    if (code == LV_EVENT_CLICKED) {
        int id = (int)(intptr_t)lv_obj_get_user_data(btn);
        if(id == 1) {
            UIManager::getInstance().loadMeshChat();
        } else if(id == 2) {
            UIManager::getInstance().loadMediaGallery(); // Galería de fotos (MenuView)
        } else if(id == 3) {
            UIManager::getInstance().loadMusicPlayer(); // Reproductor de Música (MusicView)
        } else if(id == 4) {
            UIManager::getInstance().loadConfigView();
        }
    }
}

void DashboardView::refreshState() {
}

lv_obj_t* DashboardView::create() {
    lv_obj_t* screen = lv_obj_create(NULL);
    DefaultTheme::applyFlatBg(screen);
    DefaultTheme::disableScroll(screen);

    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(screen, 12, 0);
    lv_obj_set_style_pad_row(screen, 10, 0);

    // --- Header Bar ---
    headerBar = HeaderBar::create(screen, "ESP32OS", false, true);
    HeaderBar::setActiveHeader(headerBar);

    // --- Dashboard Grid ---
    lv_obj_t * grid = lv_obj_create(screen);
    lv_obj_set_width(grid, lv_pct(100));
    lv_obj_set_flex_grow(grid, 1);
    DefaultTheme::disableScroll(grid);
    
    lv_obj_set_style_bg_opa(grid, 0, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, 2, 0);
    
    static int32_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static int32_t row_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
    lv_obj_set_style_pad_column(grid, 12, 0);
    lv_obj_set_style_pad_row(grid, 12, 0);

    const char* titles[] = {"Mesh Chat", "Galeria", "Musica", "Configuracion"};
    const char* icons[] = {LV_SYMBOL_VOLUME_MAX, LV_SYMBOL_IMAGE, LV_SYMBOL_AUDIO, LV_SYMBOL_SETTINGS};
    lv_color_t iconColors[] = {
        DefaultTheme::getPrimaryAccent(),
        lv_color_hex(0xFF2E93),
        lv_color_hex(0xFFB800),
        DefaultTheme::getSecondaryAccent()
    };
    int ids[] = {1, 2, 3, 4};

    for(int i=0; i<4; i++) {
        uint8_t col = i % 2;
        uint8_t row = i / 2;
        
        lv_obj_t * btn = lv_button_create(grid);
        lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_STRETCH, col, 1, LV_GRID_ALIGN_STRETCH, row, 1);
        DefaultTheme::applyButton(btn, 16);
        DefaultTheme::disableScroll(btn);

        lv_obj_set_user_data(btn, (void*)(intptr_t)ids[i]);
        lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(btn, 8, 0);

        // Contenedor para el Icono
        lv_obj_t * iconContainer = lv_obj_create(btn);
        lv_obj_set_size(iconContainer, 50, 50);
        DefaultTheme::applySunkenCard(iconContainer, 25);
        DefaultTheme::disableScroll(iconContainer);
        lv_obj_set_style_pad_all(iconContainer, 0, 0);
        lv_obj_remove_flag(iconContainer, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t * icon = lv_label_create(iconContainer);
        lv_label_set_text(icon, icons[i]);
        lv_obj_set_style_text_color(icon, iconColors[i], 0);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_24, 0);
        lv_obj_center(icon);

        // Etiqueta de Texto
        lv_obj_t * label = lv_label_create(btn);
        lv_label_set_text(label, titles[i]);
        lv_obj_set_style_text_color(label, DefaultTheme::getTextColor(), 0);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_margin_top(label, 6, 0);
    }

    return screen;
}
