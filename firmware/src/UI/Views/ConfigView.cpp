#include "ConfigView.h"
#include "../UIManager.h"
#include "../Themes/DefaultTheme.h"
#include "../Modals/DiagnosticsModal.h"
#include "../../Core/SystemDiagnostics.h"
#include "../../Network/ConfigManager.h"
#include <cstdio>

HeaderBar* ConfigView::headerBar = nullptr;

void ConfigView::btn_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * btn = (lv_obj_t *)lv_event_get_target(e);
    
    if (code == LV_EVENT_CLICKED) {
        int id = (int)(intptr_t)lv_obj_get_user_data(btn);
        if (id == 1) {
            UIManager::getInstance().loadWiFiConfig();
        } else if (id == 2) {
            UIManager::getInstance().loadLoRaConfig();
        } else if (id == 3) {
            UIManager::getInstance().loadFLRCConfig();
        } else if (id == 4) {
            UIManager::getInstance().loadGatewayConfig();
        } else if (id == 5) {
            DiagnosticsModal::show(lv_screen_active(), getSystemDiagnostics());
        } else if (id == 6) {
            ConfigManager::getInstance().clearAllNvs();
            UIManager::showToast("NVS borrado completamente");
        }
    }
}

lv_obj_t* ConfigView::create() {
    lv_obj_t* screen = lv_obj_create(NULL);
    DefaultTheme::applyFlatBg(screen);
    DefaultTheme::disableScroll(screen);

    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(screen, 12, 0);
    lv_obj_set_style_pad_row(screen, 10, 0);

    // Header Bar con botón Volver
    headerBar = HeaderBar::create(screen, "Configuracion", true, true);

    // Contenedor principal scrollable para las opciones
    lv_obj_t * content = lv_obj_create(screen);
    lv_obj_set_width(content, lv_pct(100));
    lv_obj_set_flex_grow(content, 1);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(content, 4, 0);
    lv_obj_set_style_pad_row(content, 10, 0);
    lv_obj_set_style_bg_opa(content, 0, 0);
    lv_obj_set_style_border_width(content, 0, 0);

    struct OptionItem {
        const char* title;
        const char* subtitle;
        int id;
    };

    OptionItem options[] = {
        {"WiFi", "Red local y parametros IP", 1},
        {"LoRa (900MHz)", "Parametros de radio largo alcance", 2},
        {"FLRC (2.4GHz)", "Parametros de alta velocidad", 3},
        {"Gateways", "Servidores y ruteo TLV", 4},
        {"Sistema", "Diagnostico de hardware y memoria", 5},
        {"Resetear NVS", "Borrar todas las configuraciones de NVS", 6}
    };

    for (int i = 0; i < 6; i++) {
        lv_obj_t* card = lv_button_create(content);
        lv_obj_set_width(card, lv_pct(100));
        lv_obj_set_height(card, 60);
        DefaultTheme::applyButton(card, 14);
        lv_obj_set_user_data(card, (void*)(intptr_t)options[i].id);
        lv_obj_add_event_cb(card, btn_event_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(card, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_left(card, 16, 0);
        lv_obj_set_style_pad_right(card, 16, 0);
        lv_obj_set_style_pad_top(card, 8, 0);
        lv_obj_set_style_pad_bottom(card, 8, 0);

        // Texto (Título + Subtítulo)
        lv_obj_t* textCont = lv_obj_create(card);
        lv_obj_set_flex_grow(textCont, 1);
        lv_obj_set_style_bg_opa(textCont, 0, 0);
        lv_obj_set_style_border_width(textCont, 0, 0);
        lv_obj_set_flex_flow(textCont, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(textCont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_all(textCont, 0, 0);
        lv_obj_set_style_pad_row(textCont, 2, 0);
        lv_obj_remove_flag(textCont, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t* titleLbl = lv_label_create(textCont);
        lv_label_set_text(titleLbl, options[i].title);
        lv_obj_set_style_text_color(titleLbl, DefaultTheme::getTextColor(), 0);
        lv_obj_set_style_text_font(titleLbl, &lv_font_montserrat_16, 0);

        lv_obj_t* subLbl = lv_label_create(textCont);
        lv_label_set_text(subLbl, options[i].subtitle);
        lv_obj_set_style_text_color(subLbl, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_font(subLbl, &lv_font_montserrat_12, 0);

        // Flecha a la derecha alineada verticalmente al centro
        lv_obj_t* arrow = lv_label_create(card);
        lv_label_set_text(arrow, LV_SYMBOL_RIGHT);
        lv_obj_set_style_text_color(arrow, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_font(arrow, &lv_font_montserrat_14, 0);
    }

    return screen;
}
