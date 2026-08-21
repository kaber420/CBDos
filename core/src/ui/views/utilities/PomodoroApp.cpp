#include "PomodoroApp.hpp"
#include "../../UIManager.hpp"
#include "../../themes/DefaultTheme.h"
#include "cbdos/audio.hpp"
#include <cstdio>
#include <vector>

namespace cbdos {
namespace ui {

// ── Estado del Pomodoro ──
static bool s_running = false;
static PomodoroPhase s_currentPhase = PomodoroPhase::WORK;
static PomodoroMode s_selectedMode = PomodoroMode::CLASSIC_25_5;
static PomodoroSound s_soundType = PomodoroSound::ZEN;

static int s_workMinutes = 25;
static int s_breakMinutes = 5;
static int s_longBreakMinutes = 15;
static int s_cycleCount = 1; // 1 to 4

static int32_t s_secondsLeft = 25 * 60;
static uint32_t s_lastMillis = 0;

// ── Punteros a elementos UI ──
static lv_obj_t* s_phaseBadgeLbl = nullptr;
static lv_obj_t* s_cycleLbl = nullptr;
static lv_obj_t* s_timeLbl = nullptr;
static lv_obj_t* s_startBtn = nullptr;
static lv_obj_t* s_startLbl = nullptr;
static lv_obj_t* s_clockCard = nullptr;

static lv_obj_t* s_presetBtns[4] = { nullptr, nullptr, nullptr, nullptr };
static lv_obj_t* s_soundBtns[4] = { nullptr, nullptr, nullptr, nullptr };

static lv_obj_t* s_customRow = nullptr;
static lv_obj_t* s_customWorkLbl = nullptr;
static lv_obj_t* s_customBreakLbl = nullptr;

static lv_timer_t* s_pomoTimer = nullptr;

void PomodoroApp::applyPreset(PomodoroMode mode) {
    s_selectedMode = mode;
    s_running = false;

    if (mode == PomodoroMode::CLASSIC_25_5) {
        s_workMinutes = 25;
        s_breakMinutes = 5;
        s_longBreakMinutes = 15;
    } else if (mode == PomodoroMode::DEEP_WORK_50_10) {
        s_workMinutes = 50;
        s_breakMinutes = 10;
        s_longBreakMinutes = 20;
    } else if (mode == PomodoroMode::SPRINT_15_3) {
        s_workMinutes = 15;
        s_breakMinutes = 3;
        s_longBreakMinutes = 10;
    }

    s_currentPhase = PomodoroPhase::WORK;
    s_secondsLeft = s_workMinutes * 60;
    updateUI();
}

void PomodoroApp::triggerAlert() {
    if (s_soundType == PomodoroSound::ZEN) {
        cbdos::audio::playTone(528, 300);
    } else if (s_soundType == PomodoroSound::CHIME) {
        cbdos::audio::playTone(880, 200);
    } else if (s_soundType == PomodoroSound::URGENT) {
        cbdos::audio::playTone(1200, 150);
    }
}

void PomodoroApp::updateUI() {
    // 1. Tiempo
    if (s_timeLbl && lv_obj_is_valid(s_timeLbl)) {
        int32_t s = s_secondsLeft < 0 ? 0 : s_secondsLeft;
        int mins = s / 60;
        int secs = s % 60;
        lv_label_set_text_fmt(s_timeLbl, "%02d:%02d", mins, secs);
    }

    // 2. Badge de Fase y Colores
    lv_color_t phaseColor = DefaultTheme::getPrimaryAccent();
    const char* phaseText = "🧠 MODO ENFOQUE";

    if (s_currentPhase == PomodoroPhase::WORK) {
        phaseColor = DefaultTheme::getPrimaryAccent();
        phaseText = "🧠 MODO ENFOQUE";
    } else if (s_currentPhase == PomodoroPhase::SHORT_BREAK) {
        phaseColor = lv_color_hex(0xFFB800); // Ámbar / Oro
        phaseText = "☕ DESCANSO CORTO";
    } else if (s_currentPhase == PomodoroPhase::LONG_BREAK) {
        phaseColor = DefaultTheme::getSecondaryAccent(); // Violeta
        phaseText = "🌴 DESCANSO LARGO";
    }

    if (s_phaseBadgeLbl && lv_obj_is_valid(s_phaseBadgeLbl)) {
        lv_label_set_text(s_phaseBadgeLbl, phaseText);
        lv_obj_set_style_text_color(s_phaseBadgeLbl, phaseColor, 0);
    }

    if (s_timeLbl && lv_obj_is_valid(s_timeLbl)) {
        lv_obj_set_style_text_color(s_timeLbl, phaseColor, 0);
    }

    // 3. Ciclo
    if (s_cycleLbl && lv_obj_is_valid(s_cycleLbl)) {
        lv_label_set_text_fmt(s_cycleLbl, "Sesion %d/4", s_cycleCount);
    }

    // 4. Botón Iniciar / Pausar
    if (s_startLbl && lv_obj_is_valid(s_startLbl)) {
        lv_label_set_text(s_startLbl, s_running ? "Pausar" : "Iniciar");
    }
    if (s_startBtn && lv_obj_is_valid(s_startBtn)) {
        lv_obj_set_style_bg_color(s_startBtn, s_running ? lv_color_hex(0xFF2E93) : phaseColor, 0);
    }

    // 5. Estado de botones de Presets
    for (int i = 0; i < 4; i++) {
        if (s_presetBtns[i] && lv_obj_is_valid(s_presetBtns[i])) {
            bool active = (i == (int)s_selectedMode);
            if (active) {
                lv_obj_add_state(s_presetBtns[i], LV_STATE_CHECKED);
            } else {
                lv_obj_remove_state(s_presetBtns[i], LV_STATE_CHECKED);
            }
        }
    }

    // 6. Fila Custom (visible solo si está en modo CUSTOM)
    if (s_customRow && lv_obj_is_valid(s_customRow)) {
        if (s_selectedMode == PomodoroMode::CUSTOM) {
            lv_obj_remove_flag(s_customRow, LV_OBJ_FLAG_HIDDEN);
            if (s_customWorkLbl && lv_obj_is_valid(s_customWorkLbl)) {
                lv_label_set_text_fmt(s_customWorkLbl, "%dm", s_workMinutes);
            }
            if (s_customBreakLbl && lv_obj_is_valid(s_customBreakLbl)) {
                lv_label_set_text_fmt(s_customBreakLbl, "%dm", s_breakMinutes);
            }
        } else {
            lv_obj_add_flag(s_customRow, LV_OBJ_FLAG_HIDDEN);
        }
    }

    // 7. Estado de botones de Sonido
    for (int i = 0; i < 4; i++) {
        if (s_soundBtns[i] && lv_obj_is_valid(s_soundBtns[i])) {
            int val = (i == 0) ? 1 : (i == 1) ? 2 : (i == 2) ? 3 : 0;
            bool active = ((int)s_soundType == val);
            if (active) {
                lv_obj_add_state(s_soundBtns[i], LV_STATE_CHECKED);
            } else {
                lv_obj_remove_state(s_soundBtns[i], LV_STATE_CHECKED);
            }
        }
    }
}

void PomodoroApp::timerCallback(lv_timer_t* t) {
    (void)t;
    if (!s_running) return;

    uint32_t now = lv_tick_get();
    if (now - s_lastMillis >= 1000) {
        uint32_t secPassed = (now - s_lastMillis) / 1000;
        s_lastMillis += secPassed * 1000;
        s_secondsLeft -= secPassed;

        if (s_secondsLeft <= 0) {
            s_secondsLeft = 0;
            triggerAlert();

            // Avanzar automáticamente a la siguiente fase
            if (s_currentPhase == PomodoroPhase::WORK) {
                if (s_cycleCount >= 4) {
                    s_currentPhase = PomodoroPhase::LONG_BREAK;
                    s_secondsLeft = s_longBreakMinutes * 60;
                    UIManager::showToast("¡4 ciclos completados! Descanso largo");
                } else {
                    s_currentPhase = PomodoroPhase::SHORT_BREAK;
                    s_secondsLeft = s_breakMinutes * 60;
                    UIManager::showToast("¡Enfoque terminado! Descanso corto");
                }
            } else {
                // Termina descanso
                if (s_currentPhase == PomodoroPhase::LONG_BREAK) {
                    s_cycleCount = 1;
                } else {
                    s_cycleCount++;
                }
                s_currentPhase = PomodoroPhase::WORK;
                s_secondsLeft = s_workMinutes * 60;
                UIManager::showToast("¡Descanso terminado! A enfocarse");
            }
        }

        updateUI();
    }
}

void PomodoroApp::toggleCb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        s_running = !s_running;
        if (s_running) {
            s_lastMillis = lv_tick_get();
        }
        updateUI();
    }
}

void PomodoroApp::nextPhaseCb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        s_running = false;
        if (s_currentPhase == PomodoroPhase::WORK) {
            s_currentPhase = (s_cycleCount >= 4) ? PomodoroPhase::LONG_BREAK : PomodoroPhase::SHORT_BREAK;
            s_secondsLeft = (s_currentPhase == PomodoroPhase::LONG_BREAK) ? (s_longBreakMinutes * 60) : (s_breakMinutes * 60);
        } else {
            if (s_currentPhase == PomodoroPhase::LONG_BREAK) s_cycleCount = 1;
            else s_cycleCount++;
            s_currentPhase = PomodoroPhase::WORK;
            s_secondsLeft = s_workMinutes * 60;
        }
        updateUI();
    }
}

void PomodoroApp::resetCb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        s_running = false;
        s_currentPhase = PomodoroPhase::WORK;
        s_cycleCount = 1;
        s_secondsLeft = s_workMinutes * 60;
        updateUI();
    }
}

void PomodoroApp::presetCb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
        int mode = (int)(intptr_t)lv_obj_get_user_data(btn);
        applyPreset((PomodoroMode)mode);
    }
}

void PomodoroApp::soundSelectCb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
        int snd = (int)(intptr_t)lv_obj_get_user_data(btn);
        s_soundType = (PomodoroSound)snd;
        updateUI();

        // Reproducir vista previa del sonido seleccionado
        triggerAlert();
    }
}

void PomodoroApp::customAdjustCb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
        int action = (int)(intptr_t)lv_obj_get_user_data(btn);

        if (action == 1 && s_workMinutes > 1) s_workMinutes -= 1;
        else if (action == 2 && s_workMinutes < 90) s_workMinutes += 1;
        else if (action == 3 && s_breakMinutes > 1) s_breakMinutes -= 1;
        else if (action == 4 && s_breakMinutes < 60) s_breakMinutes += 1;

        if (!s_running && s_currentPhase == PomodoroPhase::WORK) {
            s_secondsLeft = s_workMinutes * 60;
        } else if (!s_running && s_currentPhase == PomodoroPhase::SHORT_BREAK) {
            s_secondsLeft = s_breakMinutes * 60;
        }
        updateUI();
    }
}

void PomodoroApp::build(lv_obj_t* parent) {
    DefaultTheme::disableScroll(parent);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(parent, 6, 0);
    lv_obj_set_style_pad_row(parent, 6, 0);
    lv_obj_set_style_bg_opa(parent, 0, 0);

    // ── 1. Tarjeta Central del Reloj Pomodoro ──
    s_clockCard = lv_obj_create(parent);
    lv_obj_set_width(s_clockCard, lv_pct(100));
    lv_obj_set_height(s_clockCard, 140);
    DefaultTheme::applyRaisedCard(s_clockCard, 14);
    lv_obj_set_flex_flow(s_clockCard, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_clockCard, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(s_clockCard, 6, 0);
    lv_obj_set_style_pad_row(s_clockCard, 4, 0);

    // Cabecera con Estado y Contador de Sesión
    lv_obj_t* headerRow = lv_obj_create(s_clockCard);
    DefaultTheme::disableScroll(headerRow);
    lv_obj_set_width(headerRow, lv_pct(100));
    lv_obj_set_height(headerRow, 22);
    lv_obj_set_style_bg_opa(headerRow, 0, 0);
    lv_obj_set_style_border_width(headerRow, 0, 0);
    lv_obj_set_style_pad_all(headerRow, 0, 0);
    lv_obj_set_flex_flow(headerRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(headerRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    s_phaseBadgeLbl = lv_label_create(headerRow);
    lv_label_set_text(s_phaseBadgeLbl, "🧠 MODO ENFOQUE");
    lv_obj_set_style_text_color(s_phaseBadgeLbl, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_set_style_text_font(s_phaseBadgeLbl, &lv_font_montserrat_12, 0);

    s_cycleLbl = lv_label_create(headerRow);
    lv_label_set_text(s_cycleLbl, "Sesion 1/4");
    lv_obj_set_style_text_color(s_cycleLbl, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(s_cycleLbl, &lv_font_montserrat_12, 0);

    // Display Digital Grande
    s_timeLbl = lv_label_create(s_clockCard);
    lv_label_set_text(s_timeLbl, "25:00");
    lv_obj_set_style_text_color(s_timeLbl, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_set_style_text_font(s_timeLbl, &lv_font_montserrat_24, 0);

    // Botonera de Control
    lv_obj_t* pomoBtnRow = lv_obj_create(s_clockCard);
    DefaultTheme::disableScroll(pomoBtnRow);
    lv_obj_set_width(pomoBtnRow, lv_pct(100));
    lv_obj_set_height(pomoBtnRow, 38);
    lv_obj_set_style_bg_opa(pomoBtnRow, 0, 0);
    lv_obj_set_style_border_width(pomoBtnRow, 0, 0);
    lv_obj_set_style_pad_all(pomoBtnRow, 0, 0);
    lv_obj_set_flex_flow(pomoBtnRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(pomoBtnRow, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    s_startBtn = lv_button_create(pomoBtnRow);
    lv_obj_set_size(s_startBtn, 85, 34);
    DefaultTheme::applyButton(s_startBtn, 10);
    lv_obj_set_style_bg_color(s_startBtn, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_add_event_cb(s_startBtn, toggleCb, LV_EVENT_CLICKED, NULL);

    s_startLbl = lv_label_create(s_startBtn);
    lv_label_set_text(s_startLbl, "Iniciar");
    lv_obj_set_style_text_color(s_startLbl, lv_color_hex(0x000000), 0);
    lv_obj_center(s_startLbl);

    lv_obj_t* nextBtn = lv_button_create(pomoBtnRow);
    lv_obj_set_size(nextBtn, 85, 34);
    DefaultTheme::applyButton(nextBtn, 10);
    lv_obj_set_style_bg_color(nextBtn, lv_color_hex(0x3B2554), 0);
    lv_obj_add_event_cb(nextBtn, nextPhaseCb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* nextLbl = lv_label_create(nextBtn);
    lv_label_set_text(nextLbl, "Siguiente");
    lv_obj_set_style_text_color(nextLbl, DefaultTheme::getSecondaryAccent(), 0);
    lv_obj_center(nextLbl);

    lv_obj_t* rstBtn = lv_button_create(pomoBtnRow);
    lv_obj_set_size(rstBtn, 85, 34);
    DefaultTheme::applyButton(rstBtn, 10);
    lv_obj_set_style_bg_color(rstBtn, lv_color_hex(0x282C3C), 0);
    lv_obj_add_event_cb(rstBtn, resetCb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* rstLbl = lv_label_create(rstBtn);
    lv_label_set_text(rstLbl, "Reset");
    lv_obj_set_style_text_color(rstLbl, DefaultTheme::getTextColor(), 0);
    lv_obj_center(rstLbl);

    // ── 2. Tarjeta de Presets Populares y Personalización ──
    lv_obj_t* presetCard = lv_obj_create(parent);
    lv_obj_set_width(presetCard, lv_pct(100));
    DefaultTheme::applySunkenCard(presetCard, 12);
    lv_obj_set_flex_flow(presetCard, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(presetCard, 6, 0);
    lv_obj_set_style_pad_row(presetCard, 4, 0);

    lv_obj_t* pHeader = lv_label_create(presetCard);
    lv_label_set_text(pHeader, "MODOS DE PRODUCTIVIDAD:");
    lv_obj_set_style_text_color(pHeader, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(pHeader, &lv_font_montserrat_12, 0);

    lv_obj_t* pBtnRow = lv_obj_create(presetCard);
    DefaultTheme::disableScroll(pBtnRow);
    lv_obj_set_width(pBtnRow, lv_pct(100));
    lv_obj_set_height(pBtnRow, 32);
    lv_obj_set_style_bg_opa(pBtnRow, 0, 0);
    lv_obj_set_style_border_width(pBtnRow, 0, 0);
    lv_obj_set_style_pad_all(pBtnRow, 0, 0);
    lv_obj_set_flex_flow(pBtnRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(pBtnRow, 4, 0);

    const char* pLabels[4] = { "25/5m", "50/10m", "15/3m", "Custom" };
    for (int i = 0; i < 4; i++) {
        lv_obj_t* btn = lv_button_create(pBtnRow);
        lv_obj_set_flex_grow(btn, 1);
        lv_obj_set_height(btn, lv_pct(100));
        DefaultTheme::applyButton(btn, 8);
        lv_obj_set_user_data(btn, (void*)(intptr_t)i);
        lv_obj_add_event_cb(btn, presetCb, LV_EVENT_CLICKED, NULL);

        lv_obj_set_style_bg_color(btn, lv_color_hex(0x242838), LV_STATE_CHECKED);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_STATE_CHECKED);
        lv_obj_set_style_border_color(btn, DefaultTheme::getPrimaryAccent(), LV_STATE_CHECKED);
        lv_obj_set_style_border_width(btn, 1, LV_STATE_CHECKED);

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, pLabels[i]);
        lv_obj_set_style_text_color(lbl, DefaultTheme::getTextColor(), 0);
        lv_obj_set_style_text_color(lbl, DefaultTheme::getPrimaryAccent(), LV_STATE_CHECKED);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_center(lbl);

        s_presetBtns[i] = btn;
    }

    // Fila de Ajuste Custom (+/-)
    s_customRow = lv_obj_create(presetCard);
    DefaultTheme::disableScroll(s_customRow);
    lv_obj_set_width(s_customRow, lv_pct(100));
    lv_obj_set_height(s_customRow, 32);
    lv_obj_set_style_bg_opa(s_customRow, 0, 0);
    lv_obj_set_style_border_width(s_customRow, 0, 0);
    lv_obj_set_style_pad_all(s_customRow, 0, 0);
    lv_obj_set_flex_flow(s_customRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_customRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Ajuste Trabajo
    lv_obj_t* wSub = lv_button_create(s_customRow);
    lv_obj_set_size(wSub, 28, 28);
    DefaultTheme::applyButton(wSub, 6);
    lv_obj_set_user_data(wSub, (void*)(intptr_t)1);
    lv_obj_add_event_cb(wSub, customAdjustCb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* wSubLbl = lv_label_create(wSub);
    lv_label_set_text(wSubLbl, "-");
    lv_obj_center(wSubLbl);

    s_customWorkLbl = lv_label_create(s_customRow);
    lv_label_set_text_fmt(s_customWorkLbl, "%dm", s_workMinutes);
    lv_obj_set_style_text_color(s_customWorkLbl, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_set_style_text_font(s_customWorkLbl, &lv_font_montserrat_12, 0);

    lv_obj_t* wAdd = lv_button_create(s_customRow);
    lv_obj_set_size(wAdd, 28, 28);
    DefaultTheme::applyButton(wAdd, 6);
    lv_obj_set_user_data(wAdd, (void*)(intptr_t)2);
    lv_obj_add_event_cb(wAdd, customAdjustCb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* wAddLbl = lv_label_create(wAdd);
    lv_label_set_text(wAddLbl, "+");
    lv_obj_center(wAddLbl);

    // Separador
    lv_obj_t* sepLbl = lv_label_create(s_customRow);
    lv_label_set_text(sepLbl, "| Desc:");
    lv_obj_set_style_text_color(sepLbl, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(sepLbl, &lv_font_montserrat_12, 0);

    // Ajuste Descanso
    lv_obj_t* bSub = lv_button_create(s_customRow);
    lv_obj_set_size(bSub, 28, 28);
    DefaultTheme::applyButton(bSub, 6);
    lv_obj_set_user_data(bSub, (void*)(intptr_t)3);
    lv_obj_add_event_cb(bSub, customAdjustCb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* bSubLbl = lv_label_create(bSub);
    lv_label_set_text(bSubLbl, "-");
    lv_obj_center(bSubLbl);

    s_customBreakLbl = lv_label_create(s_customRow);
    lv_label_set_text_fmt(s_customBreakLbl, "%dm", s_breakMinutes);
    lv_obj_set_style_text_color(s_customBreakLbl, lv_color_hex(0xFFB800), 0);
    lv_obj_set_style_text_font(s_customBreakLbl, &lv_font_montserrat_12, 0);

    lv_obj_t* bAdd = lv_button_create(s_customRow);
    lv_obj_set_size(bAdd, 28, 28);
    DefaultTheme::applyButton(bAdd, 6);
    lv_obj_set_user_data(bAdd, (void*)(intptr_t)4);
    lv_obj_add_event_cb(bAdd, customAdjustCb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* bAddLbl = lv_label_create(bAdd);
    lv_label_set_text(bAddLbl, "+");
    lv_obj_center(bAddLbl);

    // ── 3. Tarjeta de Selector de Sonido Alerta I2S ──
    lv_obj_t* sndCard = lv_obj_create(parent);
    lv_obj_set_width(sndCard, lv_pct(100));
    DefaultTheme::applySunkenCard(sndCard, 12);
    lv_obj_set_flex_flow(sndCard, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(sndCard, 6, 0);
    lv_obj_set_style_pad_row(sndCard, 4, 0);

    lv_obj_t* sHeader = lv_label_create(sndCard);
    lv_label_set_text(sHeader, "ALERTA SONORA (I2S):");
    lv_obj_set_style_text_color(sHeader, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(sHeader, &lv_font_montserrat_12, 0);

    lv_obj_t* sBtnRow = lv_obj_create(sndCard);
    DefaultTheme::disableScroll(sBtnRow);
    lv_obj_set_width(sBtnRow, lv_pct(100));
    lv_obj_set_height(sBtnRow, 32);
    lv_obj_set_style_bg_opa(sBtnRow, 0, 0);
    lv_obj_set_style_border_width(sBtnRow, 0, 0);
    lv_obj_set_style_pad_all(sBtnRow, 0, 0);
    lv_obj_set_flex_flow(sBtnRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(sBtnRow, 4, 0);

    const char* sndLabels[4] = { "Zen", "Chime", "Alerta", "Mudo" };
    int sndValues[4] = { 1, 2, 3, 0 };

    for (int i = 0; i < 4; i++) {
        lv_obj_t* btn = lv_button_create(sBtnRow);
        lv_obj_set_flex_grow(btn, 1);
        lv_obj_set_height(btn, lv_pct(100));
        DefaultTheme::applyButton(btn, 8);
        lv_obj_set_user_data(btn, (void*)(intptr_t)sndValues[i]);
        lv_obj_add_event_cb(btn, soundSelectCb, LV_EVENT_CLICKED, NULL);

        lv_obj_set_style_bg_color(btn, lv_color_hex(0x242838), LV_STATE_CHECKED);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_STATE_CHECKED);
        lv_obj_set_style_border_color(btn, DefaultTheme::getSecondaryAccent(), LV_STATE_CHECKED);
        lv_obj_set_style_border_width(btn, 1, LV_STATE_CHECKED);

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, sndLabels[i]);
        lv_obj_set_style_text_color(lbl, DefaultTheme::getTextColor(), 0);
        lv_obj_set_style_text_color(lbl, DefaultTheme::getSecondaryAccent(), LV_STATE_CHECKED);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_center(lbl);

        s_soundBtns[i] = btn;
    }

    // Iniciar timer LVGL si no está activo
    if (!s_pomoTimer) {
        s_pomoTimer = lv_timer_create(timerCallback, 200, NULL);
    }

    updateUI();
}

void PomodoroApp::cleanup() {
    if (s_pomoTimer) {
        lv_timer_delete(s_pomoTimer);
        s_pomoTimer = nullptr;
    }
    s_phaseBadgeLbl = nullptr;
    s_cycleLbl = nullptr;
    s_timeLbl = nullptr;
    s_startBtn = nullptr;
    s_startLbl = nullptr;
    s_clockCard = nullptr;
    s_customRow = nullptr;
    s_customWorkLbl = nullptr;
    s_customBreakLbl = nullptr;
    for (int i = 0; i < 4; i++) {
        s_presetBtns[i] = nullptr;
        s_soundBtns[i] = nullptr;
    }
}

} // namespace ui
} // namespace cbdos
