#include "LoRaConfigView.h"
#include "../UIManager.h"
#include "../Themes/DefaultTheme.h"
#include <cstdio>
#include <cstdlib>

HeaderBar* LoRaConfigView::headerBar = nullptr;
LoRaConfig LoRaConfigView::currentCfg;
lv_obj_t* LoRaConfigView::taFreq = nullptr;
lv_obj_t* LoRaConfigView::taPwr = nullptr;
lv_obj_t* LoRaConfigView::taBw = nullptr;
lv_obj_t* LoRaConfigView::taSf = nullptr;

void LoRaConfigView::save_event_cb(lv_event_t* e) {
    if (taFreq) currentCfg.frequency = atof(lv_textarea_get_text(taFreq));
    if (taPwr) currentCfg.txPower = atoi(lv_textarea_get_text(taPwr));
    if (taBw) currentCfg.bandwidth = atof(lv_textarea_get_text(taBw));
    if (taSf) currentCfg.spreadingFactor = atoi(lv_textarea_get_text(taSf));

    if (ConfigManager::getInstance().saveLoRa(currentCfg)) {
        UIManager::showToast("LoRa guardado correctamente");
        UIManager::getInstance().loadConfigView();
    } else {
        UIManager::showToast("Error al guardar LoRa");
    }
}

lv_obj_t* LoRaConfigView::create() {
    ConfigManager::getInstance().loadLoRa(currentCfg);

    lv_obj_t* screen = lv_obj_create(NULL);
    DefaultTheme::applyFlatBg(screen);
    DefaultTheme::disableScroll(screen);

    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(screen, 12, 0);
    lv_obj_set_style_pad_row(screen, 10, 0);

    headerBar = HeaderBar::create(screen, "LoRa (900MHz)", true, true);

    lv_obj_t* content = lv_obj_create(screen);
    lv_obj_set_width(content, lv_pct(100));
    lv_obj_set_flex_grow(content, 1);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(content, 4, 0);
    lv_obj_set_style_pad_row(content, 10, 0);
    lv_obj_set_style_bg_opa(content, 0, 0);
    lv_obj_set_style_border_width(content, 0, 0);

    // Frecuencia
    lv_obj_t* lblFreq = lv_label_create(content);
    lv_label_set_text(lblFreq, "Frecuencia (MHz):");
    lv_obj_set_style_text_color(lblFreq, DefaultTheme::getTextColor(), 0);

    taFreq = lv_textarea_create(content);
    lv_obj_set_width(taFreq, lv_pct(100));
    lv_textarea_set_one_line(taFreq, true);
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f", currentCfg.frequency);
    lv_textarea_set_text(taFreq, buf);
    DefaultTheme::applyRaisedCard(taFreq, 10);
    UIManager::attachKeyboard(taFreq);

    // Potencia
    lv_obj_t* lblPwr = lv_label_create(content);
    lv_label_set_text(lblPwr, "Potencia TX (dBm):");
    lv_obj_set_style_text_color(lblPwr, DefaultTheme::getTextColor(), 0);

    taPwr = lv_textarea_create(content);
    lv_obj_set_width(taPwr, lv_pct(100));
    lv_textarea_set_one_line(taPwr, true);
    snprintf(buf, sizeof(buf), "%d", currentCfg.txPower);
    lv_textarea_set_text(taPwr, buf);
    DefaultTheme::applyRaisedCard(taPwr, 10);
    UIManager::attachKeyboard(taPwr);

    // Ancho de banda
    lv_obj_t* lblBw = lv_label_create(content);
    lv_label_set_text(lblBw, "Ancho de Banda (kHz):");
    lv_obj_set_style_text_color(lblBw, DefaultTheme::getTextColor(), 0);

    taBw = lv_textarea_create(content);
    lv_obj_set_width(taBw, lv_pct(100));
    lv_textarea_set_one_line(taBw, true);
    snprintf(buf, sizeof(buf), "%.1f", currentCfg.bandwidth);
    lv_textarea_set_text(taBw, buf);
    DefaultTheme::applyRaisedCard(taBw, 10);
    UIManager::attachKeyboard(taBw);

    // Spreading Factor
    lv_obj_t* lblSf = lv_label_create(content);
    lv_label_set_text(lblSf, "Spreading Factor (SF7-SF12):");
    lv_obj_set_style_text_color(lblSf, DefaultTheme::getTextColor(), 0);

    taSf = lv_textarea_create(content);
    lv_obj_set_width(taSf, lv_pct(100));
    lv_textarea_set_one_line(taSf, true);
    snprintf(buf, sizeof(buf), "%d", currentCfg.spreadingFactor);
    lv_textarea_set_text(taSf, buf);
    DefaultTheme::applyRaisedCard(taSf, 10);
    UIManager::attachKeyboard(taSf);

    // Guardar
    lv_obj_t* btnSave = lv_button_create(content);
    lv_obj_set_width(btnSave, lv_pct(100));
    lv_obj_set_height(btnSave, 46);
    DefaultTheme::applyButton(btnSave, 14);
    lv_obj_set_style_bg_color(btnSave, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_add_event_cb(btnSave, save_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* lblBtn = lv_label_create(btnSave);
    lv_label_set_text(lblBtn, "Guardar LoRa");
    lv_obj_set_style_text_color(lblBtn, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_text_font(lblBtn, &lv_font_montserrat_16, 0);
    lv_obj_center(lblBtn);

    return screen;
}
