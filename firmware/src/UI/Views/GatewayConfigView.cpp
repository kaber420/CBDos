#include "GatewayConfigView.h"
#include "../UIManager.h"
#include "../Themes/DefaultTheme.h"
#include "../Modals/AddGatewayModal.h"
#include "../../Network/MQTTService.h"
#include <cstdio>

HeaderBar* GatewayConfigView::headerBar = nullptr;
std::vector<GatewayConfig> GatewayConfigView::gatewaysList;
GatewayConfig GatewayConfigView::activeGw;

void GatewayConfigView::select_gw_cb(lv_event_t* e) {
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
    int idx = (int)(intptr_t)lv_obj_get_user_data(btn);
    if (idx >= 0 && idx < (int)gatewaysList.size()) {
        const auto& selected = gatewaysList[idx];
        ConfigManager::getInstance().setActiveGateway(selected.id);
        MQTTService::getInstance().reconnectTo(selected);
        UIManager::showToast("Gateway activo actualizado");
        UIManager::getInstance().loadGatewayConfig();
    }
}

void GatewayConfigView::add_gw_cb(lv_event_t* e) {
    AddGatewayModal::show(lv_screen_active());
}

void GatewayConfigView::delete_gw_cb(lv_event_t* e) {
    if (activeGw.id.length() > 0) {
        if (ConfigManager::getInstance().removeGateway(activeGw.id)) {
            UIManager::showToast("Gateway eliminado");
            UIManager::getInstance().loadGatewayConfig();
        }
    } else {
        UIManager::showToast("Seleccione un gateway");
    }
}

lv_obj_t* GatewayConfigView::create() {
    gatewaysList = ConfigManager::getInstance().listGateways();
    bool hasActive = ConfigManager::getInstance().loadActiveGateway(activeGw);

    lv_obj_t* screen = lv_obj_create(NULL);
    DefaultTheme::applyFlatBg(screen);
    DefaultTheme::disableScroll(screen);

    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(screen, 12, 0);
    lv_obj_set_style_pad_row(screen, 10, 0);

    headerBar = HeaderBar::create(screen, "Gateways", true, true);

    lv_obj_t* content = lv_obj_create(screen);
    lv_obj_set_width(content, lv_pct(100));
    lv_obj_set_flex_grow(content, 1);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(content, 4, 0);
    lv_obj_set_style_pad_row(content, 10, 0);
    lv_obj_set_style_bg_opa(content, 0, 0);
    lv_obj_set_style_border_width(content, 0, 0);

    // Card del Gateway Activo
    lv_obj_t* activeCard = lv_obj_create(content);
    lv_obj_set_width(activeCard, lv_pct(100));
    DefaultTheme::applyRaisedCard(activeCard, 14);
    lv_obj_set_flex_flow(activeCard, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(activeCard, 12, 0);
    lv_obj_set_style_pad_row(activeCard, 4, 0);

    lv_obj_t* lblActiveTitle = lv_label_create(activeCard);
    lv_label_set_text(lblActiveTitle, "Gateway Activo:");
    lv_obj_set_style_text_color(lblActiveTitle, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(lblActiveTitle, &lv_font_montserrat_12, 0);

    lv_obj_t* lblActiveName = lv_label_create(activeCard);
    if (hasActive) {
        char buf[128];
        snprintf(buf, sizeof(buf), "%s (%s:%d)", activeGw.name.c_str(), activeGw.address.c_str(), activeGw.mqttPort);
        lv_label_set_text(lblActiveName, buf);
        lv_obj_set_style_text_color(lblActiveName, DefaultTheme::getPrimaryAccent(), 0);
    } else {
        lv_label_set_text(lblActiveName, "Ninguno seleccionado");
        lv_obj_set_style_text_color(lblActiveName, lv_color_hex(0xEF4444), 0);
    }
    lv_obj_set_style_text_font(lblActiveName, &lv_font_montserrat_16, 0);

    // Lista de Gateways Disponibles
    lv_obj_t* lblListTitle = lv_label_create(content);
    lv_label_set_text(lblListTitle, "Gateways Importados:");
    lv_obj_set_style_text_color(lblListTitle, DefaultTheme::getTextColor(), 0);

    if (gatewaysList.empty()) {
        lv_obj_t* emptyLbl = lv_label_create(content);
        lv_label_set_text(emptyLbl, "No hay gateways. Coloque un archivo .enc en la SD e impórtelo.");
        lv_label_set_long_mode(emptyLbl, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(emptyLbl, lv_pct(100));
        lv_obj_set_style_text_color(emptyLbl, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_font(emptyLbl, &lv_font_montserrat_12, 0);
    } else {
        for (size_t i = 0; i < gatewaysList.size(); i++) {
            const auto& gw = gatewaysList[i];
            lv_obj_t* itemBtn = lv_button_create(content);
            lv_obj_set_width(itemBtn, lv_pct(100));
            lv_obj_set_height(itemBtn, 48);
            DefaultTheme::applyButton(itemBtn, 12);
            lv_obj_set_user_data(itemBtn, (void*)(intptr_t)i);
            lv_obj_add_event_cb(itemBtn, select_gw_cb, LV_EVENT_CLICKED, NULL);

            lv_obj_set_flex_flow(itemBtn, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(itemBtn, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

            lv_obj_t* lblItemName = lv_label_create(itemBtn);
            char itemBuf[128];
            snprintf(itemBuf, sizeof(itemBuf), "%s %s", (hasActive && gw.id == activeGw.id) ? LV_SYMBOL_OK : "  ", gw.name.c_str());
            lv_label_set_text(lblItemName, itemBuf);
            lv_obj_set_style_text_color(lblItemName, DefaultTheme::getTextColor(), 0);

            lv_obj_t* lblAddr = lv_label_create(itemBtn);
            lv_label_set_text(lblAddr, gw.address.c_str());
            lv_obj_set_style_text_color(lblAddr, DefaultTheme::getMutedTextColor(), 0);
            lv_obj_set_style_text_font(lblAddr, &lv_font_montserrat_12, 0);
        }
    }

    // Botones de Acción
    lv_obj_t* btnCont = lv_obj_create(content);
    lv_obj_set_width(btnCont, lv_pct(100));
    lv_obj_set_height(btnCont, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(btnCont, 0, 0);
    lv_obj_set_style_border_width(btnCont, 0, 0);
    lv_obj_set_flex_flow(btnCont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btnCont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(btnCont, 0, 0);

    lv_obj_t* btnAdd = lv_button_create(btnCont);
    lv_obj_set_size(btnAdd, 140, 44);
    DefaultTheme::applyButton(btnAdd, 12);
    lv_obj_set_style_bg_color(btnAdd, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_t* lblAdd = lv_label_create(btnAdd);
    lv_label_set_text(lblAdd, LV_SYMBOL_PLUS " Agregar .enc");
    lv_obj_set_style_text_color(lblAdd, lv_color_hex(0x0F172A), 0);
    lv_obj_center(lblAdd);
    lv_obj_add_event_cb(btnAdd, add_gw_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* btnDel = lv_button_create(btnCont);
    lv_obj_set_size(btnDel, 130, 44);
    DefaultTheme::applyButton(btnDel, 12);
    lv_obj_set_style_bg_color(btnDel, lv_color_hex(0xEF4444), 0);
    lv_obj_t* lblDel = lv_label_create(btnDel);
    lv_label_set_text(lblDel, LV_SYMBOL_TRASH " Eliminar");
    lv_obj_set_style_text_color(lblDel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(lblDel);
    lv_obj_add_event_cb(btnDel, delete_gw_cb, LV_EVENT_CLICKED, NULL);

    return screen;
}
