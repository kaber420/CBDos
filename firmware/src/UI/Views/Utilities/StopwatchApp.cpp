#include "StopwatchApp.h"
#include "../../UIManager.h"
#include "../../Themes/DefaultTheme.h"
#include <cstdio>
#include <vector>
#include <cstdint>

static uint32_t s_swElapsedMs = 0;
static uint32_t s_swLastMillis = 0;
static bool s_swRunning = false;
static std::vector<uint32_t> s_swLaps;

static lv_obj_t* s_swTimeLabel = nullptr;
static lv_obj_t* s_swStartBtn = nullptr;
static lv_obj_t* s_swStartLbl = nullptr;
static lv_obj_t* s_swLapsCont = nullptr;
static lv_timer_t* s_swTimerTask = nullptr;

void StopwatchApp::updateSwTimeLabel() {
    if (!s_swTimeLabel || !lv_obj_is_valid(s_swTimeLabel)) return;
    uint32_t totalMs = s_swElapsedMs;
    uint32_t mins = totalMs / 60000;
    uint32_t secs = (totalMs % 60000) / 1000;
    uint32_t cs = (totalMs % 1000) / 10;
    lv_label_set_text_fmt(s_swTimeLabel, "%02lu:%02lu.%02lu", (unsigned long)mins, (unsigned long)secs, (unsigned long)cs);
}

void StopwatchApp::timerCallback(lv_timer_t* t) {
    uint32_t now = lv_tick_get();
    if (s_swRunning) {
        uint32_t delta = now - s_swLastMillis;
        s_swElapsedMs += delta;
        s_swLastMillis = now;
        updateSwTimeLabel();
    }
}

void StopwatchApp::swToggleCb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        s_swRunning = !s_swRunning;
        if (s_swRunning) {
            s_swLastMillis = lv_tick_get();
            if (s_swStartLbl) lv_label_set_text(s_swStartLbl, "Pausar");
            if (s_swStartBtn) lv_obj_set_style_bg_color(s_swStartBtn, lv_color_hex(0xFF2E93), 0);
        } else {
            if (s_swStartLbl) lv_label_set_text(s_swStartLbl, "Iniciar");
            if (s_swStartBtn) lv_obj_set_style_bg_color(s_swStartBtn, DefaultTheme::getPrimaryAccent(), 0);
        }
    }
}

void StopwatchApp::swResetCb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        s_swRunning = false;
        s_swElapsedMs = 0;
        s_swLaps.clear();
        updateSwTimeLabel();
        if (s_swStartLbl) lv_label_set_text(s_swStartLbl, "Iniciar");
        if (s_swStartBtn) lv_obj_set_style_bg_color(s_swStartBtn, DefaultTheme::getPrimaryAccent(), 0);
        if (s_swLapsCont && lv_obj_is_valid(s_swLapsCont)) {
            lv_obj_clean(s_swLapsCont);
        }
    }
}

void StopwatchApp::swLapCb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED && s_swRunning && s_swLapsCont && lv_obj_is_valid(s_swLapsCont)) {
        s_swLaps.push_back(s_swElapsedMs);
        size_t lapNum = s_swLaps.size();

        uint32_t totalMs = s_swElapsedMs;
        uint32_t mins = totalMs / 60000;
        uint32_t secs = (totalMs % 60000) / 1000;
        uint32_t cs = (totalMs % 1000) / 10;

        lv_obj_t* lapRow = lv_obj_create(s_swLapsCont);
        DefaultTheme::disableScroll(lapRow);
        lv_obj_set_width(lapRow, lv_pct(100));
        lv_obj_set_height(lapRow, 36);
        DefaultTheme::applySunkenCard(lapRow, 8);
        lv_obj_set_style_pad_hor(lapRow, 12, 0);
        lv_obj_set_style_pad_ver(lapRow, 2, 0);
        lv_obj_set_flex_flow(lapRow, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(lapRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t* lapNumLbl = lv_label_create(lapRow);
        lv_label_set_text_fmt(lapNumLbl, "Vuelta #%d", (int)lapNum);
        lv_obj_set_style_text_color(lapNumLbl, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_font(lapNumLbl, &lv_font_montserrat_14, 0);

        lv_obj_t* lapTimeLbl = lv_label_create(lapRow);
        lv_label_set_text_fmt(lapTimeLbl, "%02lu:%02lu.%02lu", (unsigned long)mins, (unsigned long)secs, (unsigned long)cs);
        lv_obj_set_style_text_color(lapTimeLbl, DefaultTheme::getPrimaryAccent(), 0);
        lv_obj_set_style_text_font(lapTimeLbl, &lv_font_montserrat_16, 0);

        // Desplazar automáticamente hacia la última vuelta agregada
        lv_obj_scroll_to_view(lapRow, LV_ANIM_ON);
    }
}

void StopwatchApp::build(lv_obj_t* parent) {
    DefaultTheme::disableScroll(parent);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(parent, 6, 0);
    lv_obj_set_style_pad_row(parent, 8, 0);
    lv_obj_set_style_bg_opa(parent, 0, 0);

    // ── Tarjeta Principal del Cronómetro ──
    lv_obj_t* swCard = lv_obj_create(parent);
    lv_obj_set_width(swCard, lv_pct(100));
    lv_obj_set_height(swCard, 130);
    DefaultTheme::applyRaisedCard(swCard, 14);
    lv_obj_set_flex_flow(swCard, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(swCard, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(swCard, 8, 0);
    lv_obj_set_style_pad_row(swCard, 8, 0);

    s_swTimeLabel = lv_label_create(swCard);
    lv_label_set_text(s_swTimeLabel, "00:00.00");
    lv_obj_set_style_text_color(s_swTimeLabel, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_set_style_text_font(s_swTimeLabel, &lv_font_montserrat_32, 0);
    updateSwTimeLabel();

    // Botonera de control
    lv_obj_t* swBtnRow = lv_obj_create(swCard);
    DefaultTheme::disableScroll(swBtnRow);
    lv_obj_set_width(swBtnRow, lv_pct(100));
    lv_obj_set_height(swBtnRow, 42);
    lv_obj_set_style_bg_opa(swBtnRow, 0, 0);
    lv_obj_set_style_border_width(swBtnRow, 0, 0);
    lv_obj_set_style_pad_all(swBtnRow, 0, 0);
    lv_obj_set_flex_flow(swBtnRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(swBtnRow, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    s_swStartBtn = lv_button_create(swBtnRow);
    lv_obj_set_size(s_swStartBtn, 85, 38);
    DefaultTheme::applyButton(s_swStartBtn, 10);
    lv_obj_set_style_bg_color(s_swStartBtn, s_swRunning ? lv_color_hex(0xFF2E93) : DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_add_event_cb(s_swStartBtn, swToggleCb, LV_EVENT_CLICKED, NULL);

    s_swStartLbl = lv_label_create(s_swStartBtn);
    lv_label_set_text(s_swStartLbl, s_swRunning ? "Pausar" : "Iniciar");
    lv_obj_set_style_text_color(s_swStartLbl, lv_color_hex(0x000000), 0);
    lv_obj_center(s_swStartLbl);

    lv_obj_t* lapBtn = lv_button_create(swBtnRow);
    lv_obj_set_size(lapBtn, 85, 38);
    DefaultTheme::applyButton(lapBtn, 10);
    lv_obj_set_style_bg_color(lapBtn, lv_color_hex(0x3B2554), 0);
    lv_obj_add_event_cb(lapBtn, swLapCb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* lapLbl = lv_label_create(lapBtn);
    lv_label_set_text(lapLbl, "Vuelta");
    lv_obj_set_style_text_color(lapLbl, DefaultTheme::getSecondaryAccent(), 0);
    lv_obj_center(lapLbl);

    lv_obj_t* resetBtn = lv_button_create(swBtnRow);
    lv_obj_set_size(resetBtn, 85, 38);
    DefaultTheme::applyButton(resetBtn, 10);
    lv_obj_set_style_bg_color(resetBtn, lv_color_hex(0x282C3C), 0);
    lv_obj_add_event_cb(resetBtn, swResetCb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* resetLbl = lv_label_create(resetBtn);
    lv_label_set_text(resetLbl, "Reset");
    lv_obj_set_style_text_color(resetLbl, DefaultTheme::getTextColor(), 0);
    lv_obj_center(resetLbl);

    // ── Tarjeta de Vueltas (Laps) con lista scrollable ──
    lv_obj_t* lapsCard = lv_obj_create(parent);
    lv_obj_set_width(lapsCard, lv_pct(100));
    lv_obj_set_flex_grow(lapsCard, 1);
    DefaultTheme::applyRaisedCard(lapsCard, 14);
    lv_obj_set_flex_flow(lapsCard, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(lapsCard, 8, 0);
    lv_obj_set_style_pad_row(lapsCard, 6, 0);

    lv_obj_t* lapsHeader = lv_label_create(lapsCard);
    lv_label_set_text(lapsHeader, "HISTORIAL DE VUELTAS:");
    lv_obj_set_style_text_color(lapsHeader, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(lapsHeader, &lv_font_montserrat_12, 0);

    s_swLapsCont = lv_obj_create(lapsCard);
    lv_obj_set_width(s_swLapsCont, lv_pct(100));
    lv_obj_set_flex_grow(s_swLapsCont, 1);
    lv_obj_set_style_bg_opa(s_swLapsCont, 0, 0);
    lv_obj_set_style_border_width(s_swLapsCont, 0, 0);
    lv_obj_set_style_pad_all(s_swLapsCont, 0, 0);
    lv_obj_set_flex_flow(s_swLapsCont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_swLapsCont, 4, 0);
    lv_obj_set_scrollbar_mode(s_swLapsCont, LV_SCROLLBAR_MODE_AUTO);

    // Reconstruir vueltas si ya existían
    if (!s_swLaps.empty()) {
        for (size_t i = 0; i < s_swLaps.size(); i++) {
            uint32_t tMs = s_swLaps[i];
            uint32_t m = tMs / 60000;
            uint32_t s = (tMs % 60000) / 1000;
            uint32_t c = (tMs % 1000) / 10;

            lv_obj_t* lapRow = lv_obj_create(s_swLapsCont);
            DefaultTheme::disableScroll(lapRow);
            lv_obj_set_width(lapRow, lv_pct(100));
            lv_obj_set_height(lapRow, 36);
            DefaultTheme::applySunkenCard(lapRow, 8);
            lv_obj_set_style_pad_hor(lapRow, 12, 0);
            lv_obj_set_style_pad_ver(lapRow, 2, 0);
            lv_obj_set_flex_flow(lapRow, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(lapRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

            lv_obj_t* lapNumLbl = lv_label_create(lapRow);
            lv_label_set_text_fmt(lapNumLbl, "Vuelta #%d", (int)(i + 1));
            lv_obj_set_style_text_color(lapNumLbl, DefaultTheme::getMutedTextColor(), 0);
            lv_obj_set_style_text_font(lapNumLbl, &lv_font_montserrat_14, 0);

            lv_obj_t* lapTimeLbl = lv_label_create(lapRow);
            lv_label_set_text_fmt(lapTimeLbl, "%02lu:%02lu.%02lu", (unsigned long)m, (unsigned long)s, (unsigned long)c);
            lv_obj_set_style_text_color(lapTimeLbl, DefaultTheme::getPrimaryAccent(), 0);
            lv_obj_set_style_text_font(lapTimeLbl, &lv_font_montserrat_16, 0);
        }
    }

    // Iniciar timer LVGL si no está activo
    if (!s_swTimerTask) {
        s_swTimerTask = lv_timer_create(timerCallback, 30, NULL);
    }
}

void StopwatchApp::cleanup() {
    if (s_swTimerTask) {
        lv_timer_del(s_swTimerTask);
        s_swTimerTask = nullptr;
    }
    s_swTimeLabel = nullptr;
    s_swStartBtn = nullptr;
    s_swStartLbl = nullptr;
    s_swLapsCont = nullptr;
}
