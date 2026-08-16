#include "CalculatorApp.h"
#include "../../Themes/DefaultTheme.h"
#include <cstdio>
#include <cmath>
#include <string>
#include <cstdlib>
#include <cstring>

static std::string s_calcInput = "0";
static std::string s_calcHistory = "";
static double s_calcOperand1 = 0.0;
static char s_calcPendingOp = 0;
static bool s_calcStartNew = true;

static lv_obj_t* s_calcHistLabel = nullptr;
static lv_obj_t* s_calcMainLabel = nullptr;

void CalculatorApp::updateDisplay() {
    if (s_calcHistLabel && lv_obj_is_valid(s_calcHistLabel)) {
        lv_label_set_text(s_calcHistLabel, s_calcHistory.c_str());
    }
    if (s_calcMainLabel && lv_obj_is_valid(s_calcMainLabel)) {
        lv_label_set_text(s_calcMainLabel, s_calcInput.c_str());
        size_t len = s_calcInput.length();
        if (len <= 11) {
            lv_obj_set_style_text_font(s_calcMainLabel, &lv_font_montserrat_24, 0);
        } else if (len <= 15) {
            lv_obj_set_style_text_font(s_calcMainLabel, &lv_font_montserrat_16, 0);
        } else {
            lv_obj_set_style_text_font(s_calcMainLabel, &lv_font_montserrat_14, 0);
        }
    }
}

std::string CalculatorApp::formatNumber(double val) {
    if (std::isnan(val) || std::isinf(val)) return "Error";
    if (fabs(val) < 1e-15) return "0";

    char buf[64];
    double absVal = fabs(val);

    // Entero exacto dentro de rango de 15 dígitos
    if (fabs(val - std::round(val)) < 1e-11 && absVal < 1e15) {
        snprintf(buf, sizeof(buf), "%.0f", std::round(val));
        return std::string(buf);
    }

    // Decimales estándar de alta precisión (hasta 10 cifras decimales)
    if (absVal < 1e15 && absVal >= 1e-6) {
        snprintf(buf, sizeof(buf), "%.10f", val);
        char* p = strchr(buf, '.');
        if (p) {
            char* end = buf + strlen(buf) - 1;
            while (end > p && *end == '0') {
                *end = '\0';
                end--;
            }
            if (end == p) {
                *p = '\0';
            }
        }
        return std::string(buf);
    }

    // Para números extremos fuera de escala estándar
    snprintf(buf, sizeof(buf), "%.10g", val);
    return std::string(buf);
}

void CalculatorApp::btnEventCb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) return;

    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
    const char* txt = (const char*)lv_obj_get_user_data(btn);
    if (!txt) return;

    // Números 0-9
    if (txt[0] >= '0' && txt[0] <= '9' && txt[1] == '\0') {
        if (s_calcStartNew || s_calcInput == "0" || s_calcInput == "Error") {
            s_calcInput = txt;
            s_calcStartNew = false;
        } else {
            if (s_calcInput.length() < 18) {
                s_calcInput += txt;
            }
        }
    }
    // Punto decimal
    else if (strcmp(txt, ".") == 0) {
        if (s_calcStartNew || s_calcInput == "Error") {
            s_calcInput = "0.";
            s_calcStartNew = false;
        } else if (s_calcInput.find('.') == std::string::npos) {
            if (s_calcInput.length() < 18) {
                s_calcInput += ".";
            }
        }
    }
    // Clear (C)
    else if (strcmp(txt, "C") == 0) {
        s_calcInput = "0";
        s_calcHistory = "";
        s_calcOperand1 = 0.0;
        s_calcPendingOp = 0;
        s_calcStartNew = true;
    }
    // Backspace (DEL)
    else if (strcmp(txt, "DEL") == 0) {
        if (!s_calcStartNew && s_calcInput.length() > 1 && s_calcInput != "Error") {
            s_calcInput.pop_back();
        } else {
            s_calcInput = "0";
            s_calcStartNew = true;
        }
    }
    // Cambio de signo (+/-)
    else if (strcmp(txt, "+/-") == 0) {
        if (s_calcInput != "0" && s_calcInput != "Error") {
            if (s_calcInput[0] == '-') s_calcInput.erase(0, 1);
            else s_calcInput = "-" + s_calcInput;
        }
    }
    // Porcentaje (%)
    else if (strcmp(txt, "%") == 0) {
        double val = atof(s_calcInput.c_str()) / 100.0;
        s_calcInput = formatNumber(val);
        s_calcStartNew = true;
    }
    // Operadores (+, -, *, /)
    else if (strcmp(txt, "+") == 0 || strcmp(txt, "-") == 0 || strcmp(txt, "*") == 0 || strcmp(txt, "/") == 0) {
        double currentVal = atof(s_calcInput.c_str());
        if (s_calcPendingOp != 0 && !s_calcStartNew) {
            if (s_calcPendingOp == '+') s_calcOperand1 += currentVal;
            else if (s_calcPendingOp == '-') s_calcOperand1 -= currentVal;
            else if (s_calcPendingOp == '*') s_calcOperand1 *= currentVal;
            else if (s_calcPendingOp == '/') {
                if (currentVal == 0.0) {
                    s_calcInput = "Error";
                    s_calcHistory = "";
                    s_calcPendingOp = 0;
                    s_calcStartNew = true;
                    updateDisplay();
                    return;
                }
                s_calcOperand1 /= currentVal;
            }
            s_calcInput = formatNumber(s_calcOperand1);
        } else {
            s_calcOperand1 = currentVal;
        }
        s_calcPendingOp = txt[0];
        s_calcHistory = formatNumber(s_calcOperand1) + " " + txt;
        s_calcStartNew = true;
    }
    // Igual (=)
    else if (strcmp(txt, "=") == 0) {
        if (s_calcPendingOp != 0) {
            double currentVal = atof(s_calcInput.c_str());
            double res = 0.0;
            if (s_calcPendingOp == '+') res = s_calcOperand1 + currentVal;
            else if (s_calcPendingOp == '-') res = s_calcOperand1 - currentVal;
            else if (s_calcPendingOp == '*') res = s_calcOperand1 * currentVal;
            else if (s_calcPendingOp == '/') {
                if (currentVal == 0.0) {
                    s_calcInput = "Error";
                    s_calcHistory = "";
                    s_calcPendingOp = 0;
                    s_calcStartNew = true;
                    updateDisplay();
                    return;
                }
                res = s_calcOperand1 / currentVal;
            }
            s_calcHistory = formatNumber(s_calcOperand1) + " " + s_calcPendingOp + " " + formatNumber(currentVal) + " =";
            s_calcInput = formatNumber(res);
            s_calcPendingOp = 0;
            s_calcStartNew = true;
        }
    }

    updateDisplay();
}

void CalculatorApp::build(lv_obj_t* parent) {
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(parent, 6, 0);
    lv_obj_set_style_pad_row(parent, 8, 0);
    lv_obj_set_style_bg_opa(parent, 0, 0);

    // Pantalla Digital de la Calculadora
    lv_obj_t* dispCard = lv_obj_create(parent);
    lv_obj_set_width(dispCard, lv_pct(100));
    lv_obj_set_height(dispCard, 68);
    DefaultTheme::applySunkenCard(dispCard, 12);
    lv_obj_set_flex_flow(dispCard, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(dispCard, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_all(dispCard, 8, 0);

    s_calcHistLabel = lv_label_create(dispCard);
    lv_label_set_text(s_calcHistLabel, s_calcHistory.c_str());
    lv_obj_set_style_text_color(s_calcHistLabel, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(s_calcHistLabel, &lv_font_montserrat_12, 0);

    s_calcMainLabel = lv_label_create(dispCard);
    lv_label_set_text(s_calcMainLabel, s_calcInput.c_str());
    lv_obj_set_style_text_color(s_calcMainLabel, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_set_style_text_font(s_calcMainLabel, &lv_font_montserrat_24, 0);

    // Matriz de Botones (5 filas x 4 columnas)
    const char* const btnLayout[5][4] = {
        {"C", "DEL", "%", "/"},
        {"7", "8", "9", "*"},
        {"4", "5", "6", "-"},
        {"1", "2", "3", "+"},
        {"+/-", "0", ".", "="}
    };

    lv_obj_t* grid = lv_obj_create(parent);
    lv_obj_set_width(grid, lv_pct(100));
    lv_obj_set_flex_grow(grid, 1);
    lv_obj_set_style_bg_opa(grid, 0, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, 0, 0);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(grid, 6, 0);

    for (int r = 0; r < 5; r++) {
        lv_obj_t* row = lv_obj_create(grid);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_flex_grow(row, 1);
        lv_obj_set_style_bg_opa(row, 0, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(row, 6, 0);

        for (int c = 0; c < 4; c++) {
            const char* key = btnLayout[r][c];
            lv_obj_t* btn = lv_button_create(row);
            lv_obj_set_flex_grow(btn, 1);
            lv_obj_set_height(btn, lv_pct(100));
            DefaultTheme::applyButton(btn, 12);
            lv_obj_set_user_data(btn, (void*)key);
            lv_obj_add_event_cb(btn, btnEventCb, LV_EVENT_CLICKED, NULL);

            lv_obj_t* lbl = lv_label_create(btn);
            lv_label_set_text(lbl, key);
            lv_obj_center(lbl);

            if (strcmp(key, "=") == 0) {
                lv_obj_set_style_bg_color(btn, DefaultTheme::getPrimaryAccent(), 0);
                lv_obj_set_style_text_color(lbl, lv_color_hex(0x000000), 0);
                lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
            } else if (strcmp(key, "/") == 0 || strcmp(key, "*") == 0 || strcmp(key, "-") == 0 || strcmp(key, "+") == 0) {
                lv_obj_set_style_bg_color(btn, lv_color_hex(0x3B2554), 0);
                lv_obj_set_style_text_color(lbl, DefaultTheme::getSecondaryAccent(), 0);
                lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
            } else if (strcmp(key, "C") == 0 || strcmp(key, "DEL") == 0 || strcmp(key, "%") == 0) {
                lv_obj_set_style_bg_color(btn, lv_color_hex(0x282C3C), 0);
                lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFB800), 0);
                lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
            } else {
                lv_obj_set_style_text_color(lbl, DefaultTheme::getTextColor(), 0);
                lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
            }
        }
    }
}

void CalculatorApp::cleanup() {
    s_calcHistLabel = nullptr;
    s_calcMainLabel = nullptr;
}
