#include "DiagnosticsModal.h"
#include "../Themes/DefaultTheme.h"
#include <stdio.h>

lv_obj_t* DiagnosticsModal::modalMask = nullptr;

void DiagnosticsModal::close_btn_cb(lv_event_t* e) {
    hide();
}

void DiagnosticsModal::hide() {
    if (modalMask && lv_obj_is_valid(modalMask)) {
        lv_obj_delete_async(modalMask);
        modalMask = nullptr;
    }
}

void DiagnosticsModal::show(lv_obj_t* parent, const SystemDiagnostics& diag) {
    hide(); // Limpiar previo si existe

    modalMask = lv_obj_create(parent ? parent : lv_screen_active());
    lv_obj_add_flag(modalMask, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_size(modalMask, 320, 480);
    lv_obj_set_style_bg_color(modalMask, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(modalMask, LV_OPA_80, 0);
    lv_obj_set_style_border_width(modalMask, 0, 0);
    lv_obj_set_style_pad_all(modalMask, 10, 0);

    lv_obj_t* card = lv_obj_create(modalMask);
    lv_obj_set_width(card, 300);
    lv_obj_set_height(card, 440);
    DefaultTheme::applyRaisedCard(card, 16);
    lv_obj_center(card);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(card, 12, 0);

    // Título
    lv_obj_t* title = lv_label_create(card);
    lv_label_set_text(title, "Diagnóstico del Sistema");
    lv_obj_set_style_text_color(title, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_margin_bottom(card, 8, 0);

    // Helper para añadir filas de info
    auto addInfoRow = [](lv_obj_t* parent, const char* label, const char* val, bool isOk = true) {
        lv_obj_t* row = lv_obj_create(parent);
        lv_obj_set_width(row, 270);
        lv_obj_set_height(row, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(row, 0, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(row, 2, 0);

        lv_obj_t* lblKey = lv_label_create(row);
        lv_label_set_text(lblKey, label);
        lv_obj_set_style_text_color(lblKey, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_font(lblKey, &lv_font_montserrat_12, 0);

        lv_obj_t* lblVal = lv_label_create(row);
        lv_label_set_text(lblVal, val);
        lv_obj_set_style_text_font(lblVal, &lv_font_montserrat_12, 0);
        if (isOk) {
            lv_obj_set_style_text_color(lblVal, DefaultTheme::getTextColor(), 0);
        } else {
            lv_obj_set_style_text_color(lblVal, lv_color_hex(0xFF6B6B), 0);
        }
    };

    // Sección SD
    addInfoRow(card, "[SD] Estado:", diag.sdMounted ? "Montada OK" : "No detectada", diag.sdMounted);
    addInfoRow(card, "[SD] Archivo Enc:", diag.sdHasConfigFile ? "tablehub.enc OK" : "Sin archivo", diag.sdHasConfigFile);

    // Sección Red
    addInfoRow(card, "[WiFi] Red:", diag.wifiConnected ? diag.wifiSsid : "Desconectado", diag.wifiConnected);
    addInfoRow(card, "[WiFi] IP:", diag.ipAddress[0] ? diag.ipAddress : "--", diag.wifiConnected);
    addInfoRow(card, "[WiFi] MAC:", diag.macAddress[0] ? diag.macAddress : "--", true);
    
    char rssiBuf[16];
    snprintf(rssiBuf, sizeof(rssiBuf), "%d dBm", diag.rssi);
    addInfoRow(card, "[WiFi] Señal:", diag.wifiConnected ? rssiBuf : "--", diag.wifiConnected);

    // Sección MQTT / Hub
    addInfoRow(card, "[Hub] IP Servidor:", diag.hubIp[0] ? diag.hubIp : "Buscando...", diag.hubIp[0] != '\0');
    addInfoRow(card, "[MQTT] Estado:", diag.mqttConnected ? "Conectado" : "Desconectado", diag.mqttConnected);

    // Sección Hardware
    char heapBuf[32];
    snprintf(heapBuf, sizeof(heapBuf), "%u KB", diag.freeHeap / 1024);
    addInfoRow(card, "[RAM] Heap Libre:", heapBuf, diag.freeHeap > 30000);

    char psramBuf[32];
    snprintf(psramBuf, sizeof(psramBuf), "%u KB", diag.freePsram / 1024);
    addInfoRow(card, "[PSRAM] Libre:", psramBuf, true);

    char upBuf[32];
    uint32_t mins = diag.uptimeSeconds / 60;
    uint32_t secs = diag.uptimeSeconds % 60;
    snprintf(upBuf, sizeof(upBuf), "%um %us", mins, secs);
    addInfoRow(card, "[Sistema] Uptime:", upBuf, true);

    // Botón Cerrar
    lv_obj_t* btnClose = lv_button_create(card);
    lv_obj_set_size(btnClose, 240, 36);
    DefaultTheme::applyButton(btnClose, 10);
    lv_obj_set_style_bg_color(btnClose, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_set_style_margin_top(btnClose, 12, 0);

    lv_obj_t* lblC = lv_label_create(btnClose);
    lv_label_set_text(lblC, "Cerrar Diagnóstico");
    lv_obj_set_style_text_color(lblC, lv_color_hex(0x000000), 0);
    lv_obj_center(lblC);

    lv_obj_add_event_cb(btnClose, close_btn_cb, LV_EVENT_CLICKED, nullptr);
}
