#include "QuickSettingsPanel.h"
#include "../UIManager.h"
#include "../Themes/DefaultTheme.h"
#include "../../Network/ConfigManager.h"
#include "../../Core/NativeAudioDriver.h"
#include <JC3248W535.h>
#include <cstdio>

// Display global definido en main.cpp
extern JC3248W535_Display displayDriver;

lv_obj_t* QuickSettingsPanel::panelObj = nullptr;
lv_obj_t* QuickSettingsPanel::overlayMask = nullptr;

void QuickSettingsPanel::hide() {
    if (panelObj && lv_obj_is_valid(panelObj)) {
        lv_obj_delete_async(panelObj);
        panelObj = nullptr;
    }
    // overlayMask ya no se usa, pero mantener limpio por si acaso
    overlayMask = nullptr;
}

void QuickSettingsPanel::mask_click_cb(lv_event_t* e) {
    hide();
}

void QuickSettingsPanel::full_config_cb(lv_event_t* e) {
    hide();
    UIManager::getInstance().loadConfigView();
}

void QuickSettingsPanel::volume_slider_cb(lv_event_t* e) {
    lv_obj_t* slider = (lv_obj_t*)lv_event_get_target(e);
    int32_t val = lv_slider_get_value(slider);
    NativeAudioDriver::setVolume((uint8_t)val);
}

void QuickSettingsPanel::brightness_slider_cb(lv_event_t* e) {
    lv_obj_t* slider = (lv_obj_t*)lv_event_get_target(e);
    int32_t val = lv_slider_get_value(slider);
    displayDriver.setBacklight((uint8_t)val);
}

// Helper: Crea una fila con un label de ícono + texto y un slider
static lv_obj_t* createSliderRow(lv_obj_t* parent, const char* labelText,
                                  int32_t minVal, int32_t maxVal, int32_t curVal,
                                  lv_event_cb_t cb) {
    // Contenedor de fila
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, 0, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_pad_row(row, 4, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
    DefaultTheme::disableScroll(row);

    // Label
    lv_obj_t* lbl = lv_label_create(row);
    lv_label_set_text(lbl, labelText);
    lv_obj_set_style_text_color(lbl, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);

    // Slider
    lv_obj_t* slider = lv_slider_create(row);
    lv_obj_set_width(slider, lv_pct(100));
    lv_obj_set_height(slider, 10);
    lv_slider_set_range(slider, minVal, maxVal);
    lv_slider_set_value(slider, curVal, LV_ANIM_OFF);

    // Estilo del slider — knob y barra activa con el accent del tema
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x334155), 0);  // Fondo inactivo
    lv_obj_set_style_bg_color(slider, DefaultTheme::getPrimaryAccent(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, DefaultTheme::getPrimaryAccent(), LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, 4, LV_PART_KNOB);

    lv_obj_add_event_cb(slider, cb, LV_EVENT_VALUE_CHANGED, NULL);

    return slider;
}

void QuickSettingsPanel::toggle(lv_obj_t* parentScreen) {
    // Debounce: ignorar toques repetidos dentro de 400ms
    static uint32_t lastToggleMs = 0;
    uint32_t now = lv_tick_get();
    if (now - lastToggleMs < 400) return;
    lastToggleMs = now;

    if (panelObj && lv_obj_is_valid(panelObj)) {
        hide();
        return;
    }

    // Panel directamente en lv_layer_top() — sin overlay mask.
    // Solo se cierra con el botón X explícito.
    panelObj = lv_obj_create(lv_layer_top());
    lv_obj_set_width(panelObj, 300);
    lv_obj_set_height(panelObj, LV_SIZE_CONTENT);
    DefaultTheme::applyRaisedCard(panelObj, 18);
    lv_obj_align(panelObj, LV_ALIGN_TOP_MID, 0, 48); // Justo debajo del HeaderBar
    lv_obj_set_flex_flow(panelObj, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(panelObj, 14, 0);
    lv_obj_set_style_pad_row(panelObj, 10, 0);

    // ── Fila 0: Header con título y botón X de cerrar ─────────────────
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

    // ── Fila 1: Toggles WiFi, LoRa, FLRC ─────────────────────────────
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
    lv_switch_create(rowToggles);

    // FLRC Toggle
    lv_switch_create(rowToggles);

    // ── Fila 2: Slider de Volumen ─────────────────────────────────────
    createSliderRow(panelObj, LV_SYMBOL_AUDIO " Volumen",
                    0, 100, NativeAudioDriver::getVolume(),
                    volume_slider_cb);

    // ── Fila 3: Slider de Brillo ──────────────────────────────────────
    createSliderRow(panelObj, LV_SYMBOL_IMAGE " Brillo",
                    80, 255, 255,
                    brightness_slider_cb);

    // ── Gateway Activo ────────────────────────────────────────────────
    GatewayConfig activeGw;
    bool hasGw = ConfigManager::getInstance().loadActiveGateway(activeGw);
    lv_obj_t* lblGw = lv_label_create(panelObj);
    char gwBuf[128];
    snprintf(gwBuf, sizeof(gwBuf), "%s Gateway: %s", LV_SYMBOL_SETTINGS, hasGw ? activeGw.name.c_str() : "Ninguno");
    lv_label_set_text(lblGw, gwBuf);
    lv_obj_set_style_text_color(lblGw, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_set_style_text_font(lblGw, &lv_font_montserrat_14, 0);

    // ── Botón Configuración Completa ──────────────────────────────────
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
