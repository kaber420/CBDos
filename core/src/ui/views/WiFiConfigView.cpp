#include "WiFiConfigView.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include "cbdos/network.hpp"
#include <cstdio>

namespace cbdos {
namespace ui {

WiFiConfig WiFiConfigView::currentCfg;
lv_obj_t* WiFiConfigView::taSsid = nullptr;
lv_obj_t* WiFiConfigView::taPass = nullptr;
lv_obj_t* WiFiConfigView::btnTogglePass = nullptr;
lv_obj_t* WiFiConfigView::lblTogglePass = nullptr;
bool WiFiConfigView::passVisible = false;
lv_obj_t* WiFiConfigView::taIp = nullptr;
lv_obj_t* WiFiConfigView::taGw = nullptr;
lv_obj_t* WiFiConfigView::swStatic = nullptr;
lv_obj_t* WiFiConfigView::staticContainer = nullptr;

WiFiConfigView::WiFiConfigView()
    : BaseView("WiFi") {
}

void WiFiConfigView::toggle_pass_event_cb(lv_event_t* e) {
    if (!taPass || !lblTogglePass) return;
    passVisible = !passVisible;
    lv_textarea_set_password_mode(taPass, !passVisible);
    lv_label_set_text(lblTogglePass, passVisible ? LV_SYMBOL_EYE_CLOSE : LV_SYMBOL_EYE_OPEN);
}

void WiFiConfigView::switch_event_cb(lv_event_t* e) {
    if (!swStatic || !staticContainer) return;
    bool isChecked = lv_obj_has_state(swStatic, LV_STATE_CHECKED);
    if (isChecked) {
        lv_obj_remove_flag(staticContainer, LV_OBJ_FLAG_HIDDEN);
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
        if (currentCfg.ssid.length() > 0) {
            if (currentCfg.useStaticIp) {
                cbdos::network::connectWifiStatic(
                    currentCfg.ssid.c_str(),
                    currentCfg.password.c_str(),
                    currentCfg.staticIp.c_str(),
                    currentCfg.gateway.c_str(),
                    currentCfg.subnet.length() > 0 ? currentCfg.subnet.c_str() : "255.255.255.0",
                    currentCfg.dns1.c_str()
                );
            } else {
                cbdos::network::connectWifi(currentCfg.ssid.c_str(), currentCfg.password.c_str());
            }
        }
        UIManager::showToast("WiFi guardado correctamente");
        UIManager::getInstance().popView();
    } else {
        UIManager::showToast("Error al guardar WiFi");
    }
}

bool WiFiConfigView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;

    ConfigManager::getInstance().loadWiFi(currentCfg);

    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(m_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_container, 8, 0);
    lv_obj_set_style_pad_row(m_container, 12, 0);
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_container, 0, 0);

    // SSID
    lv_obj_t* lblSsid = lv_label_create(m_container);
    lv_label_set_text(lblSsid, "SSID (Nombre de Red):");
    lv_obj_set_style_text_color(lblSsid, DefaultTheme::getTextColor(), 0);

    taSsid = lv_textarea_create(m_container);
    lv_obj_set_width(taSsid, lv_pct(100));
    lv_textarea_set_one_line(taSsid, true);
    lv_textarea_set_text(taSsid, currentCfg.ssid.c_str());
    DefaultTheme::applyRaisedCard(taSsid, 10);
    UIManager::attachKeyboard(taSsid);

    // Password
    lv_obj_t* lblPass = lv_label_create(m_container);
    lv_label_set_text(lblPass, "Password:");
    lv_obj_set_style_text_color(lblPass, DefaultTheme::getTextColor(), 0);

    lv_obj_t* passRow = lv_obj_create(m_container);
    lv_obj_set_width(passRow, lv_pct(100));
    lv_obj_set_height(passRow, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(passRow, 0, 0);
    lv_obj_set_style_border_width(passRow, 0, 0);
    lv_obj_set_style_pad_all(passRow, 0, 0);
    lv_obj_set_style_pad_column(passRow, 8, 0);
    lv_obj_set_flex_flow(passRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(passRow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    DefaultTheme::disableScroll(passRow);

    taPass = lv_textarea_create(passRow);
    lv_obj_set_flex_grow(taPass, 1);
    lv_textarea_set_one_line(taPass, true);
    passVisible = false;
    lv_textarea_set_password_mode(taPass, true);
    lv_textarea_set_text(taPass, currentCfg.password.c_str());
    DefaultTheme::applyRaisedCard(taPass, 10);
    UIManager::attachKeyboard(taPass);

    btnTogglePass = lv_button_create(passRow);
    lv_obj_set_size(btnTogglePass, 42, 42);
    DefaultTheme::applyButton(btnTogglePass, 10);
    lv_obj_add_event_cb(btnTogglePass, toggle_pass_event_cb, LV_EVENT_CLICKED, NULL);

    lblTogglePass = lv_label_create(btnTogglePass);
    lv_label_set_text(lblTogglePass, LV_SYMBOL_EYE_OPEN);
    lv_obj_center(lblTogglePass);

    // IP Estatica Toggle
    lv_obj_t* rowSwitch = lv_obj_create(m_container);
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
    staticContainer = lv_obj_create(m_container);
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
    lv_obj_t* btnSave = lv_button_create(m_container);
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

    return true;
}

} // namespace ui
} // namespace cbdos
