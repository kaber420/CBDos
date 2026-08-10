#include "QuickSettingsPanel.h"
#include "../UIManager.h"
#include "../Themes/DefaultTheme.h"
#include "../../Network/ConfigManager.h"
#include <cstdio>

lv_obj_t* QuickSettingsPanel::panelObj = nullptr;
lv_obj_t* QuickSettingsPanel::overlayMask = nullptr;

void QuickSettingsPanel::hide() {
    if (panelObj && lv_obj_is_valid(panelObj)) {
        lv_obj_delete_async(panelObj);
        panelObj = nullptr;
    }
    if (overlayMask && lv_obj_is_valid(overlayMask)) {
        lv_obj_delete_async(overlayMask);
        overlayMask = nullptr;
    }
}

void QuickSettingsPanel::mask_click_cb(lv_event_t* e) {
    hide();
}

void QuickSettingsPanel::full_config_cb(lv_event_t* e) {
    hide();
    UIManager::getInstance().loadConfigView();
}

void QuickSettingsPanel::toggle(lv_obj_t* parentScreen) {
    if (panelObj && lv_obj_is_valid(panelObj)) {
        hide();
        return;
    }

    if (!parentScreen) parentScreen = lv_screen_active();
    if (!parentScreen) return;

    // Máscara transparente de toque externo
    overlayMask = lv_obj_create(parentScreen);
    lv_obj_add_flag(overlayMask, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_size(overlayMask, 320, 480);
    lv_obj_set_style_bg_opa(overlayMask, LV_OPA_0, 0);
    lv_obj_set_style_border_width(overlayMask, 0, 0);
    lv_obj_add_event_cb(overlayMask, mask_click_cb, LV_EVENT_CLICKED, NULL);

    // Panel desplegable superior
    panelObj = lv_obj_create(overlayMask);
    lv_obj_set_width(panelObj, 300);
    lv_obj_set_height(panelObj, LV_SIZE_CONTENT);
    DefaultTheme::applyRaisedCard(panelObj, 18);
    lv_obj_align(panelObj, LV_ALIGN_TOP_MID, 0, 48); // Justo debajo del HeaderBar
    lv_obj_set_flex_flow(panelObj, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(panelObj, 14, 0);
    lv_obj_set_style_pad_row(panelObj, 12, 0);

    // Fila 1: Toggles WiFi, LoRa, FLRC
    lv_obj_t* rowToggles = lv_obj_create(panelObj);
    lv_obj_set_width(rowToggles, lv_pct(100));
    lv_obj_set_height(rowToggles, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(rowToggles, 0, 0);
    lv_obj_set_style_border_width(rowToggles, 0, 0);
    lv_obj_set_flex_flow(rowToggles, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rowToggles, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(rowToggles, 0, 0);
    DefaultTheme::disableScroll(rowToggles);

    // WiFi Toggle
    lv_obj_t* swWifi = lv_switch_create(rowToggles);
    lv_obj_add_state(swWifi, LV_STATE_CHECKED);

    // LoRa Toggle
    lv_obj_t* swLora = lv_switch_create(rowToggles);

    // FLRC Toggle
    lv_obj_t* swFlrc = lv_switch_create(rowToggles);

    // Gateway Activo
    GatewayConfig activeGw;
    bool hasGw = ConfigManager::getInstance().loadActiveGateway(activeGw);
    lv_obj_t* lblGw = lv_label_create(panelObj);
    char gwBuf[128];
    snprintf(gwBuf, sizeof(gwBuf), "%s Gateway: %s", LV_SYMBOL_SETTINGS, hasGw ? activeGw.name.c_str() : "Ninguno");
    lv_label_set_text(lblGw, gwBuf);
    lv_obj_set_style_text_color(lblGw, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_set_style_text_font(lblGw, &lv_font_montserrat_14, 0);

    // Botón Configuración Completa
    lv_obj_t* btnFull = lv_button_create(panelObj);
    lv_obj_set_width(btnFull, lv_pct(100));
    lv_obj_set_height(btnFull, 40);
    DefaultTheme::applyButton(btnFull, 12);
    lv_obj_add_event_cb(btnFull, full_config_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* lblFull = lv_label_create(btnFull);
    lv_label_set_text(lblFull, "Configuración Completa " LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(lblFull, DefaultTheme::getTextColor(), 0);
    lv_obj_center(lblFull);
}
