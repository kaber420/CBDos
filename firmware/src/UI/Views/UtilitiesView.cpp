#include "UtilitiesView.h"
#include "../UIManager.h"
#include "../Themes/DefaultTheme.h"
#include "../WallpaperManager.h"
#include "Utilities/TodoApp.h"
#include "Utilities/CalculatorApp.h"
#include "Utilities/StopwatchApp.h"
#include "Utilities/PomodoroApp.h"

HeaderBar* UtilitiesView::headerBar = nullptr;

void UtilitiesView::screen_delete_cb(lv_event_t* e) {
    TodoApp::cleanup();
    CalculatorApp::cleanup();
    StopwatchApp::cleanup();
    PomodoroApp::cleanup();
}

lv_obj_t* UtilitiesView::create() {
    lv_obj_t* screen = lv_obj_create(NULL);
    DefaultTheme::applyFlatBg(screen);
    DefaultTheme::disableScroll(screen);

    WallpaperManager::getInstance().applyWallpaper(screen);

    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(screen, 8, 0);
    lv_obj_set_style_pad_row(screen, 6, 0);

    headerBar = HeaderBar::create(screen, "Utilidades", true, true);

    // Tabview principal con 4 pestañas
    lv_obj_t* tabview = lv_tabview_create(screen);
    lv_tabview_set_tab_bar_position(tabview, LV_DIR_TOP);
    lv_tabview_set_tab_bar_size(tabview, 38);
    lv_obj_set_width(tabview, lv_pct(100));
    lv_obj_set_flex_grow(tabview, 1);
    lv_obj_set_style_bg_opa(tabview, 0, 0);
    lv_obj_set_style_border_width(tabview, 0, 0);

    // Estilizar barra de pestañas
    lv_obj_t* tab_bar = lv_tabview_get_tab_bar(tabview);
    DefaultTheme::applySunkenCard(tab_bar, 10);
    lv_obj_set_style_pad_all(tab_bar, 2, 0);
    lv_obj_set_style_pad_column(tab_bar, 4, 0);

    lv_obj_t* tab_todo = lv_tabview_add_tab(tabview, "Notas");
    lv_obj_t* tab_calc = lv_tabview_add_tab(tabview, "Calc");
    lv_obj_t* tab_sw   = lv_tabview_add_tab(tabview, "Crono");
    lv_obj_t* tab_pomo = lv_tabview_add_tab(tabview, "Pomodoro");

    // Desactivar scroll por gestos táctiles en el contenedor de pestañas
    lv_obj_t* content = lv_tabview_get_content(tabview);
    DefaultTheme::disableScroll(content);
    lv_obj_set_scroll_snap_x(content, LV_SCROLL_SNAP_NONE);
    lv_obj_set_scroll_snap_y(content, LV_SCROLL_SNAP_NONE);
    lv_obj_set_style_pad_all(content, 0, 0);

    // Desactivar scroll en los contenedores individuales de pestaña
    DefaultTheme::disableScroll(tab_todo);
    DefaultTheme::disableScroll(tab_calc);
    DefaultTheme::disableScroll(tab_sw);
    DefaultTheme::disableScroll(tab_pomo);

    // Estilizar botones individuales de las pestañas
    uint32_t btn_cnt = lv_obj_get_child_count(tab_bar);
    for (uint32_t i = 0; i < btn_cnt; i++) {
        lv_obj_t* btn = lv_obj_get_child(tab_bar, i);
        if (btn) {
            lv_obj_set_style_radius(btn, 8, 0);
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x1B1E29), 0);
            lv_obj_set_style_bg_opa(btn, LV_OPA_60, 0);
            lv_obj_set_style_text_color(btn, DefaultTheme::getMutedTextColor(), 0);
            lv_obj_set_style_border_width(btn, 0, 0);

            // Ajustar tipografía de la etiqueta
            lv_obj_t* lbl = lv_obj_get_child(btn, 0);
            if (lbl) {
                lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
            }

            // Estado seleccionado (activo)
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x242838), LV_STATE_CHECKED);
            lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_STATE_CHECKED);
            lv_obj_set_style_text_color(btn, DefaultTheme::getPrimaryAccent(), LV_STATE_CHECKED);
            lv_obj_set_style_border_color(btn, DefaultTheme::getPrimaryAccent(), LV_STATE_CHECKED);
            lv_obj_set_style_border_width(btn, 1, LV_STATE_CHECKED);
            lv_obj_set_style_border_opa(btn, LV_OPA_COVER, LV_STATE_CHECKED);
        }
    }

    // Construcción modular de cada aplicación
    TodoApp::build(tab_todo);
    CalculatorApp::build(tab_calc);
    StopwatchApp::build(tab_sw);
    PomodoroApp::build(tab_pomo);

    lv_obj_add_event_cb(screen, screen_delete_cb, LV_EVENT_DELETE, NULL);

    return screen;
}
