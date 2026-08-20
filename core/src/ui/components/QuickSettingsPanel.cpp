#include "QuickSettingsPanel.hpp"
#include "../themes/DefaultTheme.h"
#include "cbdos/display.hpp"
#include "cbdos/audio.hpp"
#include "cbdos/network.hpp"
#include "cbdos/system.hpp"
#include <cstdio>

namespace cbdos {
namespace ui {

lv_obj_t* QuickSettingsPanel::panelObj = nullptr;

void QuickSettingsPanel::hide() {
    if (panelObj && lv_obj_is_valid(panelObj)) {
        lv_obj_delete_async(panelObj);
        panelObj = nullptr;
    }
}

void QuickSettingsPanel::mask_click_cb(lv_event_t* e) {
    hide();
}

void QuickSettingsPanel::volume_slider_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* slider = (lv_obj_t*)lv_event_get_target(e);
    int32_t val = lv_slider_get_value(slider);
    cbdos::audio::setVolume((uint8_t)val);
    if (code == LV_EVENT_RELEASED) {
        if (!cbdos::audio::getStats().isPlaying) {
            cbdos::audio::playBeep();
        }
    }
}

void QuickSettingsPanel::brightness_slider_cb(lv_event_t* e) {
    lv_obj_t* slider = (lv_obj_t*)lv_event_get_target(e);
    int32_t val = lv_slider_get_value(slider);
    cbdos::display::setBrightness((uint8_t)val);
}

void QuickSettingsPanel::wifi_switch_cb(lv_event_t* e) {
    lv_obj_t* sw = (lv_obj_t*)lv_event_get_target(e);
    if (sw) {
        bool checked = lv_obj_has_state(sw, LV_STATE_CHECKED);
        if (checked) {
            cbdos::network::init();
        } else {
            cbdos::network::disconnectWifi();
        }
    }
}

void QuickSettingsPanel::restart_btn_cb(lv_event_t* e) {
    cbdos::system::restart();
}

static lv_obj_t* createSliderRow(lv_obj_t* parent, const char* labelText,
                                  int32_t minVal, int32_t maxVal, int32_t curVal,
                                  lv_event_cb_t cb) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, 0, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_pad_row(row, 4, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
    DefaultTheme::disableScroll(row);

    lv_obj_t* lbl = lv_label_create(row);
    lv_label_set_text(lbl, labelText);
    lv_obj_set_style_text_color(lbl, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);

    lv_obj_t* slider = lv_slider_create(row);
    lv_obj_set_width(slider, lv_pct(100));
    lv_obj_set_height(slider, 12);
    lv_slider_set_range(slider, minVal, maxVal);
    lv_slider_set_value(slider, curVal, LV_ANIM_OFF);

    lv_obj_set_style_bg_color(slider, lv_color_hex(0x334155), 0);
    lv_obj_set_style_bg_color(slider, DefaultTheme::getPrimaryAccent(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, DefaultTheme::getPrimaryAccent(), LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, 4, LV_PART_KNOB);

    lv_obj_add_event_cb(slider, cb, LV_EVENT_VALUE_CHANGED, NULL);
    return slider;
}

void QuickSettingsPanel::toggle() {
    static uint32_t lastToggleMs = 0;
    uint32_t now = lv_tick_get();
    if (now - lastToggleMs < 300) return;
    lastToggleMs = now;

    if (panelObj && lv_obj_is_valid(panelObj)) {
        hide();
        return;
    }

    panelObj = lv_obj_create(lv_layer_top());
    lv_obj_set_width(panelObj, 300);
    lv_obj_set_height(panelObj, LV_SIZE_CONTENT);
    DefaultTheme::applyRaisedCard(panelObj, 18);
    lv_obj_align(panelObj, LV_ALIGN_TOP_MID, 0, 56);
    lv_obj_set_flex_flow(panelObj, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(panelObj, 14, 0);
    lv_obj_set_style_pad_row(panelObj, 10, 0);

    // Fila 0: Header con título y botón X
    lv_obj_t* headerRow = lv_obj_create(panelObj);
    lv_obj_set_width(headerRow, lv_pct(100));
    lv_obj_set_height(headerRow, 30);
    lv_obj_set_style_bg_opa(headerRow, 0, 0);
    lv_obj_set_style_border_width(headerRow, 0, 0);
    lv_obj_set_style_pad_all(headerRow, 0, 0);
    DefaultTheme::disableScroll(headerRow);

    lv_obj_t* lblTitle = lv_label_create(headerRow);
    lv_label_set_text(lblTitle, LV_SYMBOL_SETTINGS " Ajustes Rápidos");
    lv_obj_set_style_text_color(lblTitle, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(lblTitle, &lv_font_montserrat_14, 0);
    lv_obj_align(lblTitle, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t* btnClose = lv_button_create(headerRow);
    lv_obj_set_size(btnClose, 30, 30);
    lv_obj_align(btnClose, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(btnClose, lv_color_hex(0xEF4444), 0);
    lv_obj_set_style_bg_opa(btnClose, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btnClose, 8, 0);
    lv_obj_set_style_shadow_width(btnClose, 0, 0);
    lv_obj_set_style_border_width(btnClose, 0, 0);
    lv_obj_add_event_cb(btnClose, mask_click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* lblX = lv_label_create(btnClose);
    lv_label_set_text(lblX, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(lblX, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(lblX, &lv_font_montserrat_14, 0);
    lv_obj_center(lblX);

    // Fila 1: WiFi Switch
    lv_obj_t* rowWifi = lv_obj_create(panelObj);
    lv_obj_set_width(rowWifi, lv_pct(100));
    lv_obj_set_height(rowWifi, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(rowWifi, 0, 0);
    lv_obj_set_style_border_width(rowWifi, 0, 0);
    lv_obj_set_style_pad_all(rowWifi, 0, 0);
    lv_obj_set_flex_flow(rowWifi, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rowWifi, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    DefaultTheme::disableScroll(rowWifi);

    lv_obj_t* lblWifi = lv_label_create(rowWifi);
    lv_label_set_text(lblWifi, LV_SYMBOL_WIFI " Conexión WiFi");
    lv_obj_set_style_text_color(lblWifi, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(lblWifi, &lv_font_montserrat_12, 0);

    lv_obj_t* swWifi = lv_switch_create(rowWifi);
    if (cbdos::network::isConnected()) {
        lv_obj_add_state(swWifi, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(swWifi, wifi_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Fila 2: Slider Volumen
    createSliderRow(panelObj, LV_SYMBOL_AUDIO " Volumen",
                    0, 100, cbdos::audio::getVolume(),
                    volume_slider_cb);

    // Fila 3: Slider Brillo
    createSliderRow(panelObj, LV_SYMBOL_EYE_OPEN " Brillo",
                    10, 100, cbdos::display::getBrightness(),
                    brightness_slider_cb);

    // Fila 4: Botón Reiniciar
    lv_obj_t* btnRestart = lv_button_create(panelObj);
    lv_obj_set_width(btnRestart, lv_pct(100));
    lv_obj_set_height(btnRestart, 34);
    DefaultTheme::applyButton(btnRestart, 10);
    lv_obj_set_style_bg_color(btnRestart, lv_color_hex(0x991B1B), 0);
    lv_obj_add_event_cb(btnRestart, restart_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* lblRestart = lv_label_create(btnRestart);
    lv_label_set_text(lblRestart, LV_SYMBOL_POWER " Reiniciar Sistema");
    lv_obj_set_style_text_color(lblRestart, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(lblRestart, &lv_font_montserrat_12, 0);
    lv_obj_center(lblRestart);
}

} // namespace ui
} // namespace cbdos
