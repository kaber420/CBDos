#include "FlasherView.hpp"
#include "../themes/DefaultTheme.h"
#include "../UIManager.hpp"
#include "../components/HeaderBar.hpp"
#include "cbdos/flasher.hpp"
#include "cbdos/display.hpp"
#include "cbdos/system.hpp"
#include <cstdio>
#include <string>

namespace cbdos {
namespace ui {

static FlasherView* s_activeFlasherView = nullptr;

FlasherView::FlasherView()
    : BaseView("Flasheador Universal") {
}

bool FlasherView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;
    s_activeFlasherView = this;

    m_presets = cbdos::flasher::getPresets();
    m_selectedPresetIndex = 0;
    m_currentConfig = cbdos::flasher::getDefaultConfig();

    // Contenedor principal scrollable
    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_container, 0, 0);
    lv_obj_set_style_pad_all(m_container, 12, 0);
    lv_obj_set_flex_flow(m_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(m_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(m_container, 12, 0);
    lv_obj_set_scrollbar_mode(m_container, LV_SCROLLBAR_MODE_AUTO);

    // ─────────────────────────────────────────────────────────────
    // 1. Tarjeta de Selección de Preset
    // ─────────────────────────────────────────────────────────────
    lv_obj_t* cardPreset = lv_obj_create(m_container);
    lv_obj_set_width(cardPreset, lv_pct(100));
    lv_obj_set_height(cardPreset, LV_SIZE_CONTENT);
    DefaultTheme::applyRaisedCard(cardPreset, 14);
    lv_obj_set_flex_flow(cardPreset, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(cardPreset, 12, 0);
    lv_obj_set_style_pad_row(cardPreset, 8, 0);

    lv_obj_t* lblPresetTitle = lv_label_create(cardPreset);
    lv_label_set_text(lblPresetTitle, LV_SYMBOL_LIST " Plantilla / Preset de Hardware:");
    lv_obj_set_style_text_font(lblPresetTitle, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblPresetTitle, DefaultTheme::getPrimaryAccent(), 0);

    // Construir lista de opciones para el Dropdown
    std::string dropdownOptions = "";
    for (size_t i = 0; i < m_presets.size(); ++i) {
        if (i > 0) dropdownOptions += "\n";
        dropdownOptions += m_presets[i].name;
    }

    m_ddPresets = lv_dropdown_create(cardPreset);
    lv_dropdown_set_options(m_ddPresets, dropdownOptions.c_str());
    lv_obj_set_width(m_ddPresets, lv_pct(100));
    lv_obj_set_style_text_font(m_ddPresets, &lv_font_montserrat_14, 0);
    lv_dropdown_set_selected(m_ddPresets, m_selectedPresetIndex);
    lv_obj_add_event_cb(m_ddPresets, presetChangedCb, LV_EVENT_VALUE_CHANGED, this);

    m_lblPresetDesc = lv_label_create(cardPreset);
    lv_obj_set_width(m_lblPresetDesc, lv_pct(100));
    lv_obj_set_style_text_font(m_lblPresetDesc, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblPresetDesc, DefaultTheme::getMutedTextColor(), 0);

    // ─────────────────────────────────────────────────────────────
    // 2. Tarjeta de Conexiones / Diagrama de Hardware
    // ─────────────────────────────────────────────────────────────
    lv_obj_t* cardWiring = lv_obj_create(m_container);
    lv_obj_set_width(cardWiring, lv_pct(100));
    lv_obj_set_height(cardWiring, LV_SIZE_CONTENT);
    DefaultTheme::applyRaisedCard(cardWiring, 14);
    lv_obj_set_flex_flow(cardWiring, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(cardWiring, 12, 0);
    lv_obj_set_style_pad_row(cardWiring, 6, 0);

    lv_obj_t* lblWiringHeader = lv_label_create(cardWiring);
    lv_label_set_text(lblWiringHeader, LV_SYMBOL_SETTINGS " Conexión de Pines Requerida:");
    lv_obj_set_style_text_font(lblWiringHeader, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblWiringHeader, DefaultTheme::getPrimaryAccent(), 0);

    m_lblWiring = lv_label_create(cardWiring);
    lv_obj_set_width(m_lblWiring, lv_pct(100));
    lv_obj_set_style_text_font(m_lblWiring, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblWiring, DefaultTheme::getTextColor(), 0);

    // ─────────────────────────────────────────────────────────────
    // 3. Tarjeta de Mapeo de Pines & Parámetros
    // ─────────────────────────────────────────────────────────────
    m_cardPinConfig = lv_obj_create(m_container);
    lv_obj_set_width(m_cardPinConfig, lv_pct(100));
    lv_obj_set_height(m_cardPinConfig, LV_SIZE_CONTENT);
    DefaultTheme::applyRaisedCard(m_cardPinConfig, 14);
    lv_obj_set_flex_flow(m_cardPinConfig, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_cardPinConfig, 12, 0);
    lv_obj_set_style_pad_row(m_cardPinConfig, 8, 0);

    lv_obj_t* lblPinTitle = lv_label_create(m_cardPinConfig);
    lv_label_set_text(lblPinTitle, LV_SYMBOL_EDIT " Configuración de Pines y UART:");
    lv_obj_set_style_text_font(lblPinTitle, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblPinTitle, DefaultTheme::getPrimaryAccent(), 0);

    // Fila 1: TX y RX
    lv_obj_t* row1 = lv_obj_create(m_cardPinConfig);
    lv_obj_set_size(row1, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row1, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row1, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(row1, 0, 0);
    lv_obj_set_style_border_width(row1, 0, 0);
    lv_obj_set_style_pad_all(row1, 0, 0);

    // Pin TX (Host TX -> Target RX)
    lv_obj_t* contTx = lv_obj_create(row1);
    lv_obj_set_size(contTx, lv_pct(48), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(contTx, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(contTx, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(contTx, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_radius(contTx, 8, 0);
    lv_obj_set_style_pad_all(contTx, 6, 0);

    lv_obj_t* btnTxMinus = lv_button_create(contTx);
    lv_obj_set_size(btnTxMinus, 28, 28);
    lv_obj_t* lblMinus1 = lv_label_create(btnTxMinus);
    lv_label_set_text(lblMinus1, "-");
    lv_obj_center(lblMinus1);
    lv_obj_add_event_cb(btnTxMinus, pinAdjustCb, LV_EVENT_CLICKED, (void*)(intptr_t)-1);

    m_lblTxPin = lv_label_create(contTx);
    lv_obj_set_style_text_font(m_lblTxPin, &lv_font_montserrat_12, 0);

    lv_obj_t* btnTxPlus = lv_button_create(contTx);
    lv_obj_set_size(btnTxPlus, 28, 28);
    lv_obj_t* lblPlus1 = lv_label_create(btnTxPlus);
    lv_label_set_text(lblPlus1, "+");
    lv_obj_center(lblPlus1);
    lv_obj_add_event_cb(btnTxPlus, pinAdjustCb, LV_EVENT_CLICKED, (void*)(intptr_t)1);

    // Pin RX (Host RX -> Target TX)
    lv_obj_t* contRx = lv_obj_create(row1);
    lv_obj_set_size(contRx, lv_pct(48), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(contRx, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(contRx, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(contRx, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_radius(contRx, 8, 0);
    lv_obj_set_style_pad_all(contRx, 6, 0);

    lv_obj_t* btnRxMinus = lv_button_create(contRx);
    lv_obj_set_size(btnRxMinus, 28, 28);
    lv_obj_t* lblMinus2 = lv_label_create(btnRxMinus);
    lv_label_set_text(lblMinus2, "-");
    lv_obj_center(lblMinus2);
    lv_obj_add_event_cb(btnRxMinus, pinAdjustCb, LV_EVENT_CLICKED, (void*)(intptr_t)-2);

    m_lblRxPin = lv_label_create(contRx);
    lv_obj_set_style_text_font(m_lblRxPin, &lv_font_montserrat_12, 0);

    lv_obj_t* btnRxPlus = lv_button_create(contRx);
    lv_obj_set_size(btnRxPlus, 28, 28);
    lv_obj_t* lblPlus2 = lv_label_create(btnRxPlus);
    lv_label_set_text(lblPlus2, "+");
    lv_obj_center(lblPlus2);
    lv_obj_add_event_cb(btnRxPlus, pinAdjustCb, LV_EVENT_CLICKED, (void*)(intptr_t)2);

    // Fila 2: BOOT y RST
    lv_obj_t* row2 = lv_obj_create(m_cardPinConfig);
    lv_obj_set_size(row2, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row2, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row2, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(row2, 0, 0);
    lv_obj_set_style_border_width(row2, 0, 0);
    lv_obj_set_style_pad_all(row2, 0, 0);

    // Pin BOOT (IO0 / IO9)
    lv_obj_t* contBoot = lv_obj_create(row2);
    lv_obj_set_size(contBoot, lv_pct(48), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(contBoot, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(contBoot, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(contBoot, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_radius(contBoot, 8, 0);
    lv_obj_set_style_pad_all(contBoot, 6, 0);

    lv_obj_t* btnBootMinus = lv_button_create(contBoot);
    lv_obj_set_size(btnBootMinus, 28, 28);
    lv_obj_t* lblMinus3 = lv_label_create(btnBootMinus);
    lv_label_set_text(lblMinus3, "-");
    lv_obj_center(lblMinus3);
    lv_obj_add_event_cb(btnBootMinus, pinAdjustCb, LV_EVENT_CLICKED, (void*)(intptr_t)-3);

    m_lblBootPin = lv_label_create(contBoot);
    lv_obj_set_style_text_font(m_lblBootPin, &lv_font_montserrat_12, 0);

    lv_obj_t* btnBootPlus = lv_button_create(contBoot);
    lv_obj_set_size(btnBootPlus, 28, 28);
    lv_obj_t* lblPlus3 = lv_label_create(btnBootPlus);
    lv_label_set_text(lblPlus3, "+");
    lv_obj_center(lblPlus3);
    lv_obj_add_event_cb(btnBootPlus, pinAdjustCb, LV_EVENT_CLICKED, (void*)(intptr_t)3);

    // Pin RST
    lv_obj_t* contRst = lv_obj_create(row2);
    lv_obj_set_size(contRst, lv_pct(48), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(contRst, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(contRst, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(contRst, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_radius(contRst, 8, 0);
    lv_obj_set_style_pad_all(contRst, 6, 0);

    lv_obj_t* btnRstMinus = lv_button_create(contRst);
    lv_obj_set_size(btnRstMinus, 28, 28);
    lv_obj_t* lblMinus4 = lv_label_create(btnRstMinus);
    lv_label_set_text(lblMinus4, "-");
    lv_obj_center(lblMinus4);
    lv_obj_add_event_cb(btnRstMinus, pinAdjustCb, LV_EVENT_CLICKED, (void*)(intptr_t)-4);

    m_lblRstPin = lv_label_create(contRst);
    lv_obj_set_style_text_font(m_lblRstPin, &lv_font_montserrat_12, 0);

    lv_obj_t* btnRstPlus = lv_button_create(contRst);
    lv_obj_set_size(btnRstPlus, 28, 28);
    lv_obj_t* lblPlus4 = lv_label_create(btnRstPlus);
    lv_label_set_text(lblPlus4, "+");
    lv_obj_center(lblPlus4);
    lv_obj_add_event_cb(btnRstPlus, pinAdjustCb, LV_EVENT_CLICKED, (void*)(intptr_t)4);

    // Fila 3: Baudrate y Firmware
    lv_obj_t* row3 = lv_obj_create(m_cardPinConfig);
    lv_obj_set_size(row3, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row3, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row3, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(row3, 0, 0);
    lv_obj_set_style_border_width(row3, 0, 0);
    lv_obj_set_style_pad_all(row3, 0, 0);

    m_ddBaud = lv_dropdown_create(row3);
    lv_dropdown_set_options(m_ddBaud, "115200\n230400\n460800\n921600");
    lv_obj_set_width(m_ddBaud, lv_pct(40));
    lv_obj_add_event_cb(m_ddBaud, baudChangedCb, LV_EVENT_VALUE_CHANGED, this);

    m_lblFirmwareSource = lv_label_create(row3);
    lv_obj_set_width(m_lblFirmwareSource, lv_pct(56));
    lv_obj_set_style_text_font(m_lblFirmwareSource, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblFirmwareSource, lv_color_hex(0x38BDF8), 0);

    // ─────────────────────────────────────────────────────────────
    // 4. Tarjeta de Control y Progreso
    // ─────────────────────────────────────────────────────────────
    lv_obj_t* cardControl = lv_obj_create(m_container);
    lv_obj_set_width(cardControl, lv_pct(100));
    lv_obj_set_height(cardControl, LV_SIZE_CONTENT);
    DefaultTheme::applyRaisedCard(cardControl, 14);
    lv_obj_set_flex_flow(cardControl, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(cardControl, 14, 0);
    lv_obj_set_style_pad_row(cardControl, 10, 0);

    m_lblStatus = lv_label_create(cardControl);
    lv_label_set_text(m_lblStatus, "Listo para iniciar flasheo.");
    lv_obj_set_style_text_font(m_lblStatus, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(m_lblStatus, DefaultTheme::getTextColor(), 0);

    // Barra de Progreso
    m_barProgress = lv_bar_create(cardControl);
    lv_obj_set_width(m_barProgress, lv_pct(100));
    lv_obj_set_height(m_barProgress, 18);
    lv_bar_set_range(m_barProgress, 0, 100);
    lv_bar_set_value(m_barProgress, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(m_barProgress, lv_color_hex(0x1E293B), LV_PART_MAIN);
    lv_obj_set_style_bg_color(m_barProgress, DefaultTheme::getPrimaryAccent(), LV_PART_INDICATOR);
    lv_obj_set_style_radius(m_barProgress, 8, LV_PART_MAIN);
    lv_obj_set_style_radius(m_barProgress, 8, LV_PART_INDICATOR);

    // Log de texto
    m_lblLog = lv_label_create(cardControl);
    lv_label_set_text(m_lblLog, "Presiona el botón para iniciar la transferencia.");
    lv_obj_set_style_text_font(m_lblLog, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblLog, DefaultTheme::getMutedTextColor(), 0);

    // Botón Iniciar Flasheo
    m_btnStart = lv_button_create(cardControl);
    lv_obj_set_width(m_btnStart, lv_pct(100));
    lv_obj_set_height(m_btnStart, 48);
    DefaultTheme::applyButton(m_btnStart, 12);
    lv_obj_set_style_bg_color(m_btnStart, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_add_event_cb(m_btnStart, startFlashCb, LV_EVENT_CLICKED, this);

    m_lblBtn = lv_label_create(m_btnStart);
    lv_label_set_text(m_lblBtn, LV_SYMBOL_DOWNLOAD " Iniciar Flasheo");
    lv_obj_set_style_text_font(m_lblBtn, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(m_lblBtn, lv_color_hex(0x0F172A), 0);
    lv_obj_center(m_lblBtn);

    updateUIFromConfig();
    return true;
}

void FlasherView::onDestroy() {
    s_activeFlasherView = nullptr;
    BaseView::onDestroy();
}

void FlasherView::onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) {
    if (!m_container || !lv_obj_is_valid(m_container)) return;
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
}

void FlasherView::updateUIFromConfig() {
    if (m_selectedPresetIndex >= 0 && m_selectedPresetIndex < (int)m_presets.size()) {
        const auto& preset = m_presets[m_selectedPresetIndex];
        if (m_lblPresetDesc) lv_label_set_text(m_lblPresetDesc, preset.description.c_str());
        if (m_lblWiring) lv_label_set_text(m_lblWiring, preset.wiringInfo.c_str());
    }

    char buf[32];
    if (m_lblTxPin) {
        snprintf(buf, sizeof(buf), "TX: GPIO %d", m_currentConfig.txPin);
        lv_label_set_text(m_lblTxPin, buf);
    }
    if (m_lblRxPin) {
        snprintf(buf, sizeof(buf), "RX: GPIO %d", m_currentConfig.rxPin);
        lv_label_set_text(m_lblRxPin, buf);
    }
    if (m_lblBootPin) {
        if (m_currentConfig.bootPin >= 0) snprintf(buf, sizeof(buf), "BOOT: IO %d", m_currentConfig.bootPin);
        else snprintf(buf, sizeof(buf), "BOOT: Manual");
        lv_label_set_text(m_lblBootPin, buf);
    }
    if (m_lblRstPin) {
        if (m_currentConfig.rstPin >= 0) snprintf(buf, sizeof(buf), "RST: IO %d", m_currentConfig.rstPin);
        else snprintf(buf, sizeof(buf), "RST: Manual");
        lv_label_set_text(m_lblRstPin, buf);
    }

    if (m_lblFirmwareSource) {
        if (m_currentConfig.binPath.empty()) {
            lv_label_set_text(m_lblFirmwareSource, "FW: Embebido (SDIO)");
        } else {
            lv_label_set_text(m_lblFirmwareSource, ("FW: " + m_currentConfig.binPath).c_str());
        }
    }
}

void FlasherView::presetChangedCb(lv_event_t* e) {
    auto* view = static_cast<FlasherView*>(lv_event_get_user_data(e));
    if (!view || !view->m_ddPresets) return;

    view->m_selectedPresetIndex = lv_dropdown_get_selected(view->m_ddPresets);
    if (view->m_selectedPresetIndex >= 0 && view->m_selectedPresetIndex < (int)view->m_presets.size()) {
        view->m_currentConfig = view->m_presets[view->m_selectedPresetIndex].config;
        view->updateUIFromConfig();
    }
}

void FlasherView::baudChangedCb(lv_event_t* e) {
    auto* view = static_cast<FlasherView*>(lv_event_get_user_data(e));
    if (!view || !view->m_ddBaud) return;

    uint32_t sel = lv_dropdown_get_selected(view->m_ddBaud);
    switch (sel) {
        case 0: view->m_currentConfig.baudRate = 115200; break;
        case 1: view->m_currentConfig.baudRate = 230400; break;
        case 2: view->m_currentConfig.baudRate = 460800; break;
        case 3: view->m_currentConfig.baudRate = 921600; break;
        default: view->m_currentConfig.baudRate = 115200; break;
    }
}

void FlasherView::pinAdjustCb(lv_event_t* e) {
    auto* view = static_cast<FlasherView*>(lv_event_get_user_data(e));
    if (!view) return;

    // Recuperamos el tag pasado en event callback
    int action = (int)(intptr_t)lv_event_get_user_data(e);

    switch (action) {
        case 1:  view->m_currentConfig.txPin = (view->m_currentConfig.txPin + 1) % 60; break;
        case -1: view->m_currentConfig.txPin = (view->m_currentConfig.txPin > 0) ? view->m_currentConfig.txPin - 1 : 59; break;
        case 2:  view->m_currentConfig.rxPin = (view->m_currentConfig.rxPin + 1) % 60; break;
        case -2: view->m_currentConfig.rxPin = (view->m_currentConfig.rxPin > 0) ? view->m_currentConfig.rxPin - 1 : 59; break;
        case 3:  view->m_currentConfig.bootPin = (view->m_currentConfig.bootPin + 1) % 60; break;
        case -3: view->m_currentConfig.bootPin = (view->m_currentConfig.bootPin > -1) ? view->m_currentConfig.bootPin - 1 : 59; break;
        case 4:  view->m_currentConfig.rstPin = (view->m_currentConfig.rstPin + 1) % 60; break;
        case -4: view->m_currentConfig.rstPin = (view->m_currentConfig.rstPin > -1) ? view->m_currentConfig.rstPin - 1 : 59; break;
    }
    view->updateUIFromConfig();
}

void FlasherView::startFlashCb(lv_event_t* e) {
    auto* view = static_cast<FlasherView*>(lv_event_get_user_data(e));
    if (!view || cbdos::flasher::isBusy()) return;

    if (view->m_btnStart) {
        lv_obj_add_state(view->m_btnStart, LV_STATE_DISABLED);
    }
    if (view->m_lblBtn) {
        lv_label_set_text(view->m_lblBtn, "Flasheando...");
    }
    if (view->m_barProgress) {
        lv_bar_set_value(view->m_barProgress, 0, LV_ANIM_OFF);
    }
    if (view->m_lblStatus) {
        lv_label_set_text(view->m_lblStatus, "Iniciando proceso...");
        lv_obj_set_style_text_color(view->m_lblStatus, lv_color_hex(0x60A5FA), 0);
    }

    cbdos::flasher::startFlash(view->m_currentConfig, [view](cbdos::flasher::FlasherStatus status, int percent, const char* message) {
        if (!s_activeFlasherView || s_activeFlasherView != view) return;

        if (view->m_barProgress && lv_obj_is_valid(view->m_barProgress)) {
            lv_bar_set_value(view->m_barProgress, percent, LV_ANIM_ON);
        }
        if (view->m_lblLog && lv_obj_is_valid(view->m_lblLog) && message) {
            lv_label_set_text(view->m_lblLog, message);
        }
        if (view->m_lblStatus && lv_obj_is_valid(view->m_lblStatus)) {
            if (status == cbdos::flasher::FlasherStatus::Failed) {
                lv_label_set_text(view->m_lblStatus, "Fallo durante el flasheo");
                lv_obj_set_style_text_color(view->m_lblStatus, lv_color_hex(0xEF4444), 0);
            } else if (status == cbdos::flasher::FlasherStatus::Success) {
                lv_label_set_text(view->m_lblStatus, "¡Programación completada exitosamente!");
                lv_obj_set_style_text_color(view->m_lblStatus, lv_color_hex(0x10B981), 0);
            } else {
                lv_label_set_text(view->m_lblStatus, "Programando memoria Flash...");
                lv_obj_set_style_text_color(view->m_lblStatus, lv_color_hex(0x60A5FA), 0);
            }
        }

        if (status == cbdos::flasher::FlasherStatus::Success || status == cbdos::flasher::FlasherStatus::Failed) {
            if (view->m_btnStart && lv_obj_is_valid(view->m_btnStart)) {
                lv_obj_remove_state(view->m_btnStart, LV_STATE_DISABLED);
            }
            if (view->m_lblBtn && lv_obj_is_valid(view->m_lblBtn)) {
                lv_label_set_text(view->m_lblBtn, (status == cbdos::flasher::FlasherStatus::Success) ? LV_SYMBOL_REFRESH " Flashear Nuevamente" : LV_SYMBOL_REFRESH " Reintentar");
            }
        }
    });
}

} // namespace ui
} // namespace cbdos

