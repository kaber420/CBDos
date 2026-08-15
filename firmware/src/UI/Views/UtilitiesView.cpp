#include "UtilitiesView.h"
#include "../UIManager.h"
#include "../Themes/DefaultTheme.h"
#include "../WallpaperManager.h"
#include "Utilities/TodoApp.h"
#include "Utilities/CalculatorApp.h"
#include "Utilities/StopwatchApp.h"

HeaderBar* UtilitiesView::headerBar = nullptr;

void UtilitiesView::screen_delete_cb(lv_event_t* e) {
    TodoApp::cleanup();
    CalculatorApp::cleanup();
    StopwatchApp::cleanup();
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

    // Tabview principal con 3 pestañas
    lv_obj_t* tabview = lv_tabview_create(screen);
    lv_tabview_set_tab_bar_position(tabview, LV_DIR_TOP);
    lv_tabview_set_tab_bar_size(tabview, 42);
    lv_obj_set_width(tabview, lv_pct(100));
    lv_obj_set_flex_grow(tabview, 1);
    lv_obj_set_style_bg_opa(tabview, 0, 0);
    lv_obj_set_style_border_width(tabview, 0, 0);

    // Estilizar barra de pestañas
    lv_obj_t* tab_bar = lv_tabview_get_tab_bar(tabview);
    DefaultTheme::applySunkenCard(tab_bar, 10);
    lv_obj_set_style_pad_all(tab_bar, 2, 0);
    lv_obj_set_style_text_color(tab_bar, DefaultTheme::getTextColor(), 0);

    lv_obj_t* tab_todo = lv_tabview_add_tab(tabview, "Notas");
    lv_obj_t* tab_calc = lv_tabview_add_tab(tabview, "Calculadora");
    lv_obj_t* tab_sw   = lv_tabview_add_tab(tabview, "Cronometro");

    // Construcción modular de cada aplicación
    TodoApp::build(tab_todo);
    CalculatorApp::build(tab_calc);
    StopwatchApp::build(tab_sw);

    lv_obj_add_event_cb(screen, screen_delete_cb, LV_EVENT_DELETE, NULL);

    return screen;
}
