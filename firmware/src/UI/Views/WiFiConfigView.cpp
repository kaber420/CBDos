#include "WiFiConfigView.h"
#include "../UIManager.h"
#include "../Themes/DefaultTheme.h"
#include <cstdio>
#ifdef ARDUINO
#include <WiFi.h>
#endif

HeaderBar* WiFiConfigView::headerBar = nullptr;
WiFiConfig WiFiConfigView::currentCfg;
lv_obj_t* WiFiConfigView::taSsid = nullptr;
lv_obj_t* WiFiConfigView::taPass = nullptr;
lv_obj_t* WiFiConfigView::taIp = nullptr;
lv_obj_t* WiFiConfigView::taGw = nullptr;
lv_obj_t* WiFiConfigView::swStatic = nullptr;
lv_obj_t* WiFiConfigView::staticContainer = nullptr;

void WiFiConfigView::switch_event_cb(lv_event_t* e) {
    if (!swStatic || !staticContainer) return;
    bool isChecked = lv_obj_has_state(swStatic, LV_STATE_CHECKED);
    if (isChecked) {
        lv_obj_clear_flag(staticContainer, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(staticContainer, LV_OBJ_FLAG_HIDDEN);
    }
}

void WiFiConfigView::save_event_cb(lv_event_t* e) {
    if (taSsid) currentCfg.ssid = lv_textarea_get_text(taSsid);
    if (taPass) currentCfg.password = lv_textarea_get_text(taPass);
    if (swStatic) currentCfg.useStaticIp = lv_obj_has_state(swStatic, LV_STATE_CHECKED);
    if (taIp) currentCfg.staticIp = lv_textarea_get_text(taIp);
    if (taGw) currentCfg.gateway = lv_textarea_get_text(taGw);

    if (ConfigManager::getInstance().saveWiFi(currentCfg)) {
        UIManager::showToast("WiFi guardado correctamente");
        UIManager::getInstance().loadConfigView();
    } else {
        UIManager::showToast("Error al guardar WiFi");
    }
}

lv_obj_t* WiFiConfigView::create() {
    ConfigManager::getInstance().loadWiFi(currentCfg);

    lv_obj_t* screen = lv_obj_create(NULL);
    DefaultTheme::applyFlatBg(screen);
    DefaultTheme::disableScroll(screen);

    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(screen, 12, 0);
    lv_obj_set_style_pad_row(screen, 10, 0);

    headerBar = HeaderBar::create(screen, "Configuracion WiFi", true, true);

    lv_obj_t* content = lv_obj_create(screen);
    lv_obj_set_width(content, lv_pct(100));
    lv_obj_set_flex_grow(content, 1);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(content, 4, 0);
    lv_obj_set_style_pad_row(content, 12, 0);
    lv_obj_set_style_bg_opa(content, 0, 0);
    lv_obj_set_style_border_width(content, 0, 0);

    // SSID
    lv_obj_t* lblSsid = lv_label_create(content);
    lv_label_set_text(lblSsid, "SSID (Nombre de Red):");
    lv_obj_set_style_text_color(lblSsid, DefaultTheme::getTextColor(), 0);

    taSsid = lv_textarea_create(content);
    lv_obj_set_width(taSsid, lv_pct(100));
    lv_textarea_set_one_line(taSsid, true);
    lv_textarea_set_text(taSsid, currentCfg.ssid.c_str());
    DefaultTheme::applyRaisedCard(taSsid, 10);
    UIManager::attachKeyboard(taSsid);

    // Password
    lv_obj_t* lblPass = lv_label_create(content);
    lv_label_set_text(lblPass, "Password:");
    lv_obj_set_style_text_color(lblPass, DefaultTheme::getTextColor(), 0);

    taPass = lv_textarea_create(content);
    lv_obj_set_width(taPass, lv_pct(100));
    lv_textarea_set_one_line(taPass, true);
    lv_textarea_set_password_mode(taPass, true);
    lv_textarea_set_text(taPass, currentCfg.password.c_str());
    DefaultTheme::applyRaisedCard(taPass, 10);
    UIManager::attachKeyboard(taPass);

    // IP Estatica Toggle
    lv_obj_t* rowSwitch = lv_obj_create(content);
    lv_obj_set_width(rowSwitch, lv_pct(100));
    lv_obj_set_height(rowSwitch, 44);
    lv_obj_set_style_bg_opa(rowSwitch, 0, 0);
    lv_obj_set_style_border_width(rowSwitch, 0, 0);
    lv_obj_set_flex_flow(rowSwitch, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rowSwitch, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    DefaultTheme::disableScroll(rowSwitch);

    lv_obj_t* lblSw = lv_label_create(rowSwitch);
    lv_label_set_text(lblSw, "Usar IP Estatica");
    lv_obj_set_style_text_color(lblSw, DefaultTheme::getTextColor(), 0);

    swStatic = lv_switch_create(rowSwitch);
    if (currentCfg.useStaticIp) {
        lv_obj_add_state(swStatic, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(swStatic, switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Contenedor para IP Estatica
    staticContainer = lv_obj_create(content);
    lv_obj_set_width(staticContainer, lv_pct(100));
    lv_obj_set_flex_flow(staticContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(staticContainer, 0, 0);
    lv_obj_set_style_pad_row(staticContainer, 8, 0);
    lv_obj_set_style_bg_opa(staticContainer, 0, 0);
    lv_obj_set_style_border_width(staticContainer, 0, 0);

    if (!currentCfg.useStaticIp) {
        lv_obj_add_flag(staticContainer, LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_t* lblIp = lv_label_create(staticContainer);
    lv_label_set_text(lblIp, "Direccion IP:");
    lv_obj_set_style_text_color(lblIp, DefaultTheme::getTextColor(), 0);

    taIp = lv_textarea_create(staticContainer);
    lv_obj_set_width(taIp, lv_pct(100));
    lv_textarea_set_one_line(taIp, true);
    lv_textarea_set_text(taIp, currentCfg.staticIp.c_str());
    DefaultTheme::applyRaisedCard(taIp, 10);
    UIManager::attachKeyboard(taIp);

    lv_obj_t* lblGw = lv_label_create(staticContainer);
    lv_label_set_text(lblGw, "Gateway:");
    lv_obj_set_style_text_color(lblGw, DefaultTheme::getTextColor(), 0);

    taGw = lv_textarea_create(staticContainer);
    lv_obj_set_width(taGw, lv_pct(100));
    lv_textarea_set_one_line(taGw, true);
    lv_textarea_set_text(taGw, currentCfg.gateway.c_str());
    DefaultTheme::applyRaisedCard(taGw, 10);
    UIManager::attachKeyboard(taGw);

    // Botón Guardar
    lv_obj_t* btnSave = lv_button_create(content);
    lv_obj_set_width(btnSave, lv_pct(100));
    lv_obj_set_height(btnSave, 46);
    DefaultTheme::applyButton(btnSave, 14);
    lv_obj_set_style_bg_color(btnSave, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_add_event_cb(btnSave, save_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* lblBtn = lv_label_create(btnSave);
    lv_label_set_text(lblBtn, "Guardar Configuracion");
    lv_obj_set_style_text_color(lblBtn, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_text_font(lblBtn, &lv_font_montserrat_16, 0);
    lv_obj_center(lblBtn);

    return screen;
}
