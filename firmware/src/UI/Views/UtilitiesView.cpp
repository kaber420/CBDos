#include "UtilitiesView.h"
#include "../UIManager.h"
#include "../Themes/DefaultTheme.h"
#include "../WallpaperManager.h"
#include "../../Core/LVFS_Driver.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <string>

#ifdef ARDUINO
#include <Arduino.h>
#include <SD.h>
#include <ArduinoJson.h>
#endif

HeaderBar* UtilitiesView::headerBar = nullptr;

// ═══════════════════════════════════════════════════════════════════════════
// 1. ESTRUCTURAS Y ESTADO DE NOTAS / LISTAS TO-DO
// ═══════════════════════════════════════════════════════════════════════════
struct TodoItem {
    std::string text;
    bool done;
};

struct TodoList {
    std::string name;
    std::vector<TodoItem> items;
};

static std::vector<TodoList> s_todoLists;
static size_t s_currentListIdx = 0;
static lv_obj_t* s_todoItemsCont = nullptr;
static lv_obj_t* s_todoDropdown = nullptr;
static lv_obj_t* s_todoTextArea = nullptr;

static void loadTodoFromStorage();
static void saveTodoToStorage();
static void refreshTodoListUI();

static void loadTodoFromStorage() {
    s_todoLists.clear();

#ifdef ARDUINO
    lv_fs_spi_lock();
    if (SD.exists("/notes/tasks.json")) {
        File file = SD.open("/notes/tasks.json", FILE_READ);
        if (file) {
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, file);
            file.close();
            lv_fs_spi_unlock();

            if (!err && doc.is<JsonObject>()) {
                JsonArray listsArr = doc["lists"].as<JsonArray>();
                for (JsonObject lObj : listsArr) {
                    TodoList l;
                    l.name = lObj["name"].as<const char*>() ? lObj["name"].as<const char*>() : "Lista";
                    JsonArray itemsArr = lObj["items"].as<JsonArray>();
                    for (JsonObject iObj : itemsArr) {
                        TodoItem it;
                        it.text = iObj["text"].as<const char*>() ? iObj["text"].as<const char*>() : "";
                        it.done = iObj["done"].as<bool>();
                        l.items.push_back(it);
                    }
                    s_todoLists.push_back(l);
                }
            }
        } else {
            lv_fs_spi_unlock();
        }
    } else {
        lv_fs_spi_unlock();
    }
#endif

    // Si no hay datos (primera vez o sin SD), crear listas por defecto
    if (s_todoLists.empty()) {
        TodoList compras;
        compras.name = "Compras";
        compras.items.push_back({"Leche y Cafe", false});
        compras.items.push_back({"Huevos y Frutas", false});
        compras.items.push_back({"Pan integral", true});
        s_todoLists.push_back(compras);

        TodoList tareas;
        tareas.name = "Tareas";
        tareas.items.push_back({"Revisar codigo espOS32", false});
        tareas.items.push_back({"Flashear firmware a ESP32-S3", false});
        s_todoLists.push_back(tareas);
    }
}

static void saveTodoToStorage() {
#ifdef ARDUINO
    lv_fs_spi_lock();
    if (!SD.exists("/notes")) {
        SD.mkdir("/notes");
    }
    File file = SD.open("/notes/tasks.json", FILE_WRITE);
    if (file) {
        JsonDocument doc;
        JsonArray listsArr = doc["lists"].to<JsonArray>();
        for (const auto& l : s_todoLists) {
            JsonObject lObj = listsArr.add<JsonObject>();
            lObj["name"] = l.name.c_str();
            JsonArray itemsArr = lObj["items"].to<JsonArray>();
            for (const auto& it : l.items) {
                JsonObject iObj = itemsArr.add<JsonObject>();
                iObj["text"] = it.text.c_str();
                iObj["done"] = it.done;
            }
        }
        serializeJson(doc, file);
        file.close();
    }
    lv_fs_spi_unlock();
#endif
}

static void todo_checkbox_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t* cb = (lv_obj_t*)lv_event_get_target(e);
        size_t itemIdx = (size_t)(intptr_t)lv_obj_get_user_data(cb);
        if (s_currentListIdx < s_todoLists.size() && itemIdx < s_todoLists[s_currentListIdx].items.size()) {
            bool checked = lv_obj_has_state(cb, LV_STATE_CHECKED);
            s_todoLists[s_currentListIdx].items[itemIdx].done = checked;
            saveTodoToStorage();
            refreshTodoListUI();
        }
    }
}

static void todo_delete_item_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
        size_t itemIdx = (size_t)(intptr_t)lv_obj_get_user_data(btn);
        if (s_currentListIdx < s_todoLists.size() && itemIdx < s_todoLists[s_currentListIdx].items.size()) {
            s_todoLists[s_currentListIdx].items.erase(s_todoLists[s_currentListIdx].items.begin() + itemIdx);
            saveTodoToStorage();
            refreshTodoListUI();
            UIManager::showToast("Tarea eliminada");
        }
    }
}

static void refreshTodoListUI() {
    if (!s_todoItemsCont || !lv_obj_is_valid(s_todoItemsCont)) return;
    lv_obj_clean(s_todoItemsCont);

    if (s_currentListIdx >= s_todoLists.size()) {
        s_currentListIdx = 0;
    }

    if (s_todoLists.empty()) {
        lv_obj_t* emptyLbl = lv_label_create(s_todoItemsCont);
        lv_label_set_text(emptyLbl, "No hay tareas en esta lista");
        lv_obj_set_style_text_color(emptyLbl, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_center(emptyLbl);
        return;
    }

    const auto& currentItems = s_todoLists[s_currentListIdx].items;
    if (currentItems.empty()) {
        lv_obj_t* emptyLbl = lv_label_create(s_todoItemsCont);
        lv_label_set_text(emptyLbl, "Lista vacia.\nUsa la barra inferior para agregar.");
        lv_obj_set_style_text_color(emptyLbl, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_align(emptyLbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(emptyLbl);
        return;
    }

    for (size_t i = 0; i < currentItems.size(); i++) {
        const auto& item = currentItems[i];

        lv_obj_t* row = lv_obj_create(s_todoItemsCont);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, LV_SIZE_CONTENT);
        DefaultTheme::applyRaisedCard(row, 12);
        lv_obj_set_style_pad_all(row, 8, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        // Checkbox con texto
        lv_obj_t* cb = lv_checkbox_create(row);
        lv_checkbox_set_text(cb, item.text.c_str());
        lv_obj_set_flex_grow(cb, 1);
        lv_obj_set_user_data(cb, (void*)(intptr_t)i);
        lv_obj_add_event_cb(cb, todo_checkbox_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

        if (item.done) {
            lv_obj_add_state(cb, LV_STATE_CHECKED);
            lv_obj_set_style_text_color(cb, DefaultTheme::getMutedTextColor(), 0);
        } else {
            lv_obj_remove_state(cb, LV_STATE_CHECKED);
            lv_obj_set_style_text_color(cb, DefaultTheme::getTextColor(), 0);
        }
        lv_obj_set_style_text_font(cb, &lv_font_montserrat_14, 0);

        // Botón Borrar (Papelera)
        lv_obj_t* delBtn = lv_button_create(row);
        lv_obj_set_size(delBtn, 36, 36);
        DefaultTheme::applyButton(delBtn, 8);
        lv_obj_set_style_bg_color(delBtn, lv_color_hex(0x2B1A24), 0);
        lv_obj_set_user_data(delBtn, (void*)(intptr_t)i);
        lv_obj_add_event_cb(delBtn, todo_delete_item_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t* trashIcon = lv_label_create(delBtn);
        lv_label_set_text(trashIcon, LV_SYMBOL_TRASH);
        lv_obj_set_style_text_color(trashIcon, lv_color_hex(0xFF4B6E), 0);
        lv_obj_center(trashIcon);
    }
}

static void todo_add_btn_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED || code == LV_EVENT_READY) {
        if (!s_todoTextArea) return;
        const char* text = lv_textarea_get_text(s_todoTextArea);
        if (text && strlen(text) > 0) {
            if (s_currentListIdx < s_todoLists.size()) {
                s_todoLists[s_currentListIdx].items.push_back({text, false});
                saveTodoToStorage();
                refreshTodoListUI();
                lv_textarea_set_text(s_todoTextArea, "");
                UIManager::showToast("Tarea agregada");
            }
        }
    }
}

static void todo_clear_completed_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        if (s_currentListIdx < s_todoLists.size()) {
            auto& items = s_todoLists[s_currentListIdx].items;
            size_t before = items.size();
            for (auto it = items.begin(); it != items.end(); ) {
                if (it->done) {
                    it = items.erase(it);
                } else {
                    ++it;
                }
            }
            if (items.size() < before) {
                saveTodoToStorage();
                refreshTodoListUI();
                UIManager::showToast("Tareas completadas limpiadas");
            }
        }
    }
}

static void todo_dropdown_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t* dd = (lv_obj_t*)lv_event_get_target(e);
        s_currentListIdx = lv_dropdown_get_selected(dd);
        refreshTodoListUI();
    }
}

void UtilitiesView::buildTodoTab(lv_obj_t* parent) {
    loadTodoFromStorage();

    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(parent, 6, 0);
    lv_obj_set_style_pad_row(parent, 8, 0);
    lv_obj_set_style_bg_opa(parent, 0, 0);

    // Barra superior: Dropdown selector de categoría + botón limpiar
    lv_obj_t* topRow = lv_obj_create(parent);
    lv_obj_set_width(topRow, lv_pct(100));
    lv_obj_set_height(topRow, 46);
    lv_obj_set_style_bg_opa(topRow, 0, 0);
    lv_obj_set_style_border_width(topRow, 0, 0);
    lv_obj_set_style_pad_all(topRow, 0, 0);
    lv_obj_set_flex_flow(topRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(topRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Dropdown
    s_todoDropdown = lv_dropdown_create(topRow);
    lv_obj_set_width(s_todoDropdown, 160);
    DefaultTheme::applySunkenCard(s_todoDropdown, 10);
    lv_obj_set_style_text_color(s_todoDropdown, DefaultTheme::getTextColor(), 0);

    std::string ddOptions = "";
    for (size_t i = 0; i < s_todoLists.size(); i++) {
        if (i > 0) ddOptions += "\n";
        ddOptions += s_todoLists[i].name;
    }
    lv_dropdown_set_options(s_todoDropdown, ddOptions.c_str());
    lv_dropdown_set_selected(s_todoDropdown, s_currentListIdx);
    lv_obj_add_event_cb(s_todoDropdown, todo_dropdown_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Botón Limpiar Completadas
    lv_obj_t* clearBtn = lv_button_create(topRow);
    lv_obj_set_size(clearBtn, 110, 40);
    DefaultTheme::applyButton(clearBtn, 10);
    lv_obj_add_event_cb(clearBtn, todo_clear_completed_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* clearLbl = lv_label_create(clearBtn);
    lv_label_set_text(clearLbl, "Limpiar");
    lv_obj_set_style_text_color(clearLbl, DefaultTheme::getSecondaryAccent(), 0);
    lv_obj_set_style_text_font(clearLbl, &lv_font_montserrat_12, 0);
    lv_obj_center(clearLbl);

    // Contenedor scrollable de tareas
    s_todoItemsCont = lv_obj_create(parent);
    lv_obj_set_width(s_todoItemsCont, lv_pct(100));
    lv_obj_set_flex_grow(s_todoItemsCont, 1);
    lv_obj_set_flex_flow(s_todoItemsCont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(s_todoItemsCont, 4, 0);
    lv_obj_set_style_pad_row(s_todoItemsCont, 6, 0);
    lv_obj_set_style_bg_opa(s_todoItemsCont, 0, 0);
    lv_obj_set_style_border_width(s_todoItemsCont, 0, 0);

    refreshTodoListUI();

    // Barra inferior de entrada para nueva tarea
    lv_obj_t* inputRow = lv_obj_create(parent);
    lv_obj_set_width(inputRow, lv_pct(100));
    lv_obj_set_height(inputRow, 50);
    lv_obj_set_style_bg_opa(inputRow, 0, 0);
    lv_obj_set_style_border_width(inputRow, 0, 0);
    lv_obj_set_style_pad_all(inputRow, 0, 0);
    lv_obj_set_flex_flow(inputRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(inputRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    s_todoTextArea = lv_textarea_create(inputRow);
    lv_obj_set_flex_grow(s_todoTextArea, 1);
    lv_obj_set_height(s_todoTextArea, 44);
    DefaultTheme::applySunkenCard(s_todoTextArea, 10);
    lv_textarea_set_placeholder_text(s_todoTextArea, "Nueva tarea...");
    lv_textarea_set_one_line(s_todoTextArea, true);
    lv_obj_set_style_text_color(s_todoTextArea, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(s_todoTextArea, &lv_font_montserrat_14, 0);
    UIManager::attachKeyboard(s_todoTextArea);
    lv_obj_add_event_cb(s_todoTextArea, todo_add_btn_cb, LV_EVENT_READY, NULL);

    lv_obj_t* addBtn = lv_button_create(inputRow);
    lv_obj_set_size(addBtn, 44, 44);
    DefaultTheme::applyButton(addBtn, 10);
    lv_obj_set_style_bg_color(addBtn, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_add_event_cb(addBtn, todo_add_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* addIcon = lv_label_create(addBtn);
    lv_label_set_text(addIcon, LV_SYMBOL_PLUS);
    lv_obj_set_style_text_color(addIcon, lv_color_hex(0x000000), 0);
    lv_obj_center(addIcon);
}

// ═══════════════════════════════════════════════════════════════════════════
// 2. CALCULADORA TÁCTIL MODERNA
// ═══════════════════════════════════════════════════════════════════════════
static std::string s_calcInput = "0";
static std::string s_calcHistory = "";
static double s_calcOperand1 = 0.0;
static char s_calcPendingOp = 0;
static bool s_calcStartNew = true;

static lv_obj_t* s_calcHistLabel = nullptr;
static lv_obj_t* s_calcMainLabel = nullptr;

static void updateCalcDisplay() {
    if (s_calcHistLabel && lv_obj_is_valid(s_calcHistLabel)) {
        lv_label_set_text(s_calcHistLabel, s_calcHistory.c_str());
    }
    if (s_calcMainLabel && lv_obj_is_valid(s_calcMainLabel)) {
        lv_label_set_text(s_calcMainLabel, s_calcInput.c_str());
    }
}

static std::string formatCalcNumber(double val) {
    if (std::isnan(val) || std::isinf(val)) return "Error";
    char buf[32];
    snprintf(buf, sizeof(buf), "%.6g", val);
    return std::string(buf);
}

static void calc_btn_event_cb(lv_event_t* e) {
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
            if (s_calcInput.length() < 12) {
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
            s_calcInput += ".";
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
        s_calcInput = formatCalcNumber(val);
        s_calcStartNew = true;
    }
    // Operadores (+, -, *, /)
    else if (strcmp(txt, "+") == 0 || strcmp(txt, "-") == 0 || strcmp(txt, "*") == 0 || strcmp(txt, "/") == 0) {
        double currentVal = atof(s_calcInput.c_str());
        if (s_calcPendingOp != 0 && !s_calcStartNew) {
            // Ejecutar operación anterior encadenada
            if (s_calcPendingOp == '+') s_calcOperand1 += currentVal;
            else if (s_calcPendingOp == '-') s_calcOperand1 -= currentVal;
            else if (s_calcPendingOp == '*') s_calcOperand1 *= currentVal;
            else if (s_calcPendingOp == '/') {
                if (currentVal == 0.0) {
                    s_calcInput = "Error";
                    s_calcHistory = "";
                    s_calcPendingOp = 0;
                    s_calcStartNew = true;
                    updateCalcDisplay();
                    return;
                }
                s_calcOperand1 /= currentVal;
            }
            s_calcInput = formatCalcNumber(s_calcOperand1);
        } else {
            s_calcOperand1 = currentVal;
        }
        s_calcPendingOp = txt[0];
        s_calcHistory = formatCalcNumber(s_calcOperand1) + " " + txt;
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
                    updateCalcDisplay();
                    return;
                }
                res = s_calcOperand1 / currentVal;
            }
            s_calcHistory = formatCalcNumber(s_calcOperand1) + " " + s_calcPendingOp + " " + formatCalcNumber(currentVal) + " =";
            s_calcInput = formatCalcNumber(res);
            s_calcPendingOp = 0;
            s_calcStartNew = true;
        }
    }

    updateCalcDisplay();
}

void UtilitiesView::buildCalcTab(lv_obj_t* parent) {
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

    // Matriz de Botones de la Calculadora (5 filas x 4 columnas)
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
            lv_obj_add_event_cb(btn, calc_btn_event_cb, LV_EVENT_CLICKED, NULL);

            lv_obj_t* lbl = lv_label_create(btn);
            lv_label_set_text(lbl, key);
            lv_obj_center(lbl);

            // Estilos diferenciados según el tipo de tecla
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

// ═══════════════════════════════════════════════════════════════════════════
// 3. CRONÓMETRO Y TEMPORIZADOR
// ═══════════════════════════════════════════════════════════════════════════
static uint32_t s_swElapsedMs = 0;
static uint32_t s_swLastMillis = 0;
static bool s_swRunning = false;
static std::vector<uint32_t> s_swLaps;

static int32_t s_timerSecondsLeft = 0;
static uint32_t s_timerLastMillis = 0;
static bool s_timerRunning = false;

static lv_obj_t* s_swTimeLabel = nullptr;
static lv_obj_t* s_swStartBtn = nullptr;
static lv_obj_t* s_swStartLbl = nullptr;
static lv_obj_t* s_swLapsCont = nullptr;
static lv_obj_t* s_timerDisplayLabel = nullptr;
static lv_timer_t* s_swTimerTask = nullptr;

static void updateSwTimeLabel() {
    if (!s_swTimeLabel || !lv_obj_is_valid(s_swTimeLabel)) return;
    uint32_t totalMs = s_swElapsedMs;
    uint32_t mins = totalMs / 60000;
    uint32_t secs = (totalMs % 60000) / 1000;
    uint32_t cs = (totalMs % 1000) / 10;
    lv_label_set_text_fmt(s_swTimeLabel, "%02lu:%02lu.%02lu", (unsigned long)mins, (unsigned long)secs, (unsigned long)cs);
}

static void updateTimerDisplayLabel() {
    if (!s_timerDisplayLabel || !lv_obj_is_valid(s_timerDisplayLabel)) return;
    int32_t s = s_timerSecondsLeft;
    if (s < 0) s = 0;
    uint32_t mins = s / 60;
    uint32_t secs = s % 60;
    lv_label_set_text_fmt(s_timerDisplayLabel, "%02lu:%02lu", (unsigned long)mins, (unsigned long)secs);
}

static void sw_timer_callback(lv_timer_t* t) {
    uint32_t now = lv_tick_get();

    // Cronómetro
    if (s_swRunning) {
        uint32_t delta = now - s_swLastMillis;
        s_swElapsedMs += delta;
        s_swLastMillis = now;
        updateSwTimeLabel();
    }

    // Temporizador
    if (s_timerRunning) {
        if (now - s_timerLastMillis >= 1000) {
            uint32_t secPassed = (now - s_timerLastMillis) / 1000;
            s_timerLastMillis += secPassed * 1000;
            s_timerSecondsLeft -= secPassed;

            if (s_timerSecondsLeft <= 0) {
                s_timerSecondsLeft = 0;
                s_timerRunning = false;
                UIManager::showToast("¡Tiempo completado!");
            }
            updateTimerDisplayLabel();
        }
    }
}

static void sw_toggle_cb(lv_event_t* e) {
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

static void sw_reset_cb(lv_event_t* e) {
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

static void sw_lap_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED && s_swRunning && s_swLapsCont && lv_obj_is_valid(s_swLapsCont)) {
        s_swLaps.push_back(s_swElapsedMs);
        size_t lapNum = s_swLaps.size();

        uint32_t totalMs = s_swElapsedMs;
        uint32_t mins = totalMs / 60000;
        uint32_t secs = (totalMs % 60000) / 1000;
        uint32_t cs = (totalMs % 1000) / 10;

        lv_obj_t* lapRow = lv_obj_create(s_swLapsCont);
        lv_obj_set_width(lapRow, lv_pct(100));
        lv_obj_set_height(lapRow, 34);
        DefaultTheme::applySunkenCard(lapRow, 8);
        lv_obj_set_style_pad_hor(lapRow, 10, 0);
        lv_obj_set_style_pad_ver(lapRow, 2, 0);
        lv_obj_set_flex_flow(lapRow, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(lapRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t* lapNumLbl = lv_label_create(lapRow);
        lv_label_set_text_fmt(lapNumLbl, "Vuelta #%d", (int)lapNum);
        lv_obj_set_style_text_color(lapNumLbl, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_font(lapNumLbl, &lv_font_montserrat_12, 0);

        lv_obj_t* lapTimeLbl = lv_label_create(lapRow);
        lv_label_set_text_fmt(lapTimeLbl, "%02lu:%02lu.%02lu", (unsigned long)mins, (unsigned long)secs, (unsigned long)cs);
        lv_obj_set_style_text_color(lapTimeLbl, DefaultTheme::getSecondaryAccent(), 0);
        lv_obj_set_style_text_font(lapTimeLbl, &lv_font_montserrat_14, 0);
    }
}

static void timer_preset_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
        int mins = (int)(intptr_t)lv_obj_get_user_data(btn);
        s_timerSecondsLeft = mins * 60;
        s_timerLastMillis = lv_tick_get();
        s_timerRunning = true;
        updateTimerDisplayLabel();
        UIManager::showToast("Temporizador iniciado");
    }
}

static void timer_stop_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        s_timerRunning = false;
        s_timerSecondsLeft = 0;
        updateTimerDisplayLabel();
    }
}

void UtilitiesView::buildStopwatchTab(lv_obj_t* parent) {
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(parent, 6, 0);
    lv_obj_set_style_pad_row(parent, 8, 0);
    lv_obj_set_style_bg_opa(parent, 0, 0);

    // ── Sección Cronómetro ──
    lv_obj_t* swCard = lv_obj_create(parent);
    lv_obj_set_width(swCard, lv_pct(100));
    lv_obj_set_height(swCard, 175);
    DefaultTheme::applyRaisedCard(swCard, 14);
    lv_obj_set_flex_flow(swCard, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(swCard, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(swCard, 8, 0);
    lv_obj_set_style_pad_row(swCard, 6, 0);

    s_swTimeLabel = lv_label_create(swCard);
    lv_label_set_text(s_swTimeLabel, "00:00.00");
    lv_obj_set_style_text_color(s_swTimeLabel, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_set_style_text_font(s_swTimeLabel, &lv_font_montserrat_24, 0);
    updateSwTimeLabel();

    // Botonera de control del Cronómetro
    lv_obj_t* swBtnRow = lv_obj_create(swCard);
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
    lv_obj_add_event_cb(s_swStartBtn, sw_toggle_cb, LV_EVENT_CLICKED, NULL);

    s_swStartLbl = lv_label_create(s_swStartBtn);
    lv_label_set_text(s_swStartLbl, s_swRunning ? "Pausar" : "Iniciar");
    lv_obj_set_style_text_color(s_swStartLbl, lv_color_hex(0x000000), 0);
    lv_obj_center(s_swStartLbl);

    lv_obj_t* lapBtn = lv_button_create(swBtnRow);
    lv_obj_set_size(lapBtn, 85, 38);
    DefaultTheme::applyButton(lapBtn, 10);
    lv_obj_set_style_bg_color(lapBtn, lv_color_hex(0x3B2554), 0);
    lv_obj_add_event_cb(lapBtn, sw_lap_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* lapLbl = lv_label_create(lapBtn);
    lv_label_set_text(lapLbl, "Vuelta");
    lv_obj_set_style_text_color(lapLbl, DefaultTheme::getSecondaryAccent(), 0);
    lv_obj_center(lapLbl);

    lv_obj_t* resetBtn = lv_button_create(swBtnRow);
    lv_obj_set_size(resetBtn, 85, 38);
    DefaultTheme::applyButton(resetBtn, 10);
    lv_obj_set_style_bg_color(resetBtn, lv_color_hex(0x282C3C), 0);
    lv_obj_add_event_cb(resetBtn, sw_reset_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* resetLbl = lv_label_create(resetBtn);
    lv_label_set_text(resetLbl, "Reset");
    lv_obj_set_style_text_color(resetLbl, DefaultTheme::getTextColor(), 0);
    lv_obj_center(resetLbl);

    // Contenedor de Vueltas (Laps)
    s_swLapsCont = lv_obj_create(swCard);
    lv_obj_set_width(s_swLapsCont, lv_pct(100));
    lv_obj_set_flex_grow(s_swLapsCont, 1);
    lv_obj_set_style_bg_opa(s_swLapsCont, 0, 0);
    lv_obj_set_style_border_width(s_swLapsCont, 0, 0);
    lv_obj_set_style_pad_all(s_swLapsCont, 0, 0);
    lv_obj_set_flex_flow(s_swLapsCont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_swLapsCont, 4, 0);

    // ── Sección Temporizador / Pomodoro ──
    lv_obj_t* timerCard = lv_obj_create(parent);
    lv_obj_set_width(timerCard, lv_pct(100));
    lv_obj_set_flex_grow(timerCard, 1);
    DefaultTheme::applyRaisedCard(timerCard, 14);
    lv_obj_set_flex_flow(timerCard, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(timerCard, 8, 0);
    lv_obj_set_style_pad_row(timerCard, 6, 0);

    lv_obj_t* tHeader = lv_label_create(timerCard);
    lv_label_set_text(tHeader, "Temporizador Rapido / Pomodoro:");
    lv_obj_set_style_text_color(tHeader, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(tHeader, &lv_font_montserrat_12, 0);

    lv_obj_t* tMidRow = lv_obj_create(timerCard);
    lv_obj_set_width(tMidRow, lv_pct(100));
    lv_obj_set_height(tMidRow, 38);
    lv_obj_set_style_bg_opa(tMidRow, 0, 0);
    lv_obj_set_style_border_width(tMidRow, 0, 0);
    lv_obj_set_style_pad_all(tMidRow, 0, 0);
    lv_obj_set_flex_flow(tMidRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(tMidRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    s_timerDisplayLabel = lv_label_create(tMidRow);
    lv_label_set_text(s_timerDisplayLabel, "00:00");
    lv_obj_set_style_text_color(s_timerDisplayLabel, lv_color_hex(0xFFB800), 0);
    lv_obj_set_style_text_font(s_timerDisplayLabel, &lv_font_montserrat_24, 0);
    updateTimerDisplayLabel();

    lv_obj_t* stopTmrBtn = lv_button_create(tMidRow);
    lv_obj_set_size(stopTmrBtn, 85, 34);
    DefaultTheme::applyButton(stopTmrBtn, 8);
    lv_obj_set_style_bg_color(stopTmrBtn, lv_color_hex(0x282C3C), 0);
    lv_obj_add_event_cb(stopTmrBtn, timer_stop_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* stopLbl = lv_label_create(stopTmrBtn);
    lv_label_set_text(stopLbl, "Detener");
    lv_obj_set_style_text_color(stopLbl, DefaultTheme::getTextColor(), 0);
    lv_obj_center(stopLbl);

    // Botones de presets
    lv_obj_t* presetRow = lv_obj_create(timerCard);
    lv_obj_set_width(presetRow, lv_pct(100));
    lv_obj_set_flex_grow(presetRow, 1);
    lv_obj_set_style_bg_opa(presetRow, 0, 0);
    lv_obj_set_style_border_width(presetRow, 0, 0);
    lv_obj_set_style_pad_all(presetRow, 0, 0);
    lv_obj_set_flex_flow(presetRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(presetRow, 6, 0);

    struct Preset {
        const char* label;
        int mins;
    };
    Preset presets[] = {
        {"1m", 1},
        {"5m", 5},
        {"15m", 15},
        {"25m", 25}
    };

    for (int i = 0; i < 4; i++) {
        lv_obj_t* pBtn = lv_button_create(presetRow);
        lv_obj_set_flex_grow(pBtn, 1);
        lv_obj_set_height(pBtn, lv_pct(100));
        DefaultTheme::applyButton(pBtn, 10);
        lv_obj_set_user_data(pBtn, (void*)(intptr_t)presets[i].mins);
        lv_obj_add_event_cb(pBtn, timer_preset_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t* pLbl = lv_label_create(pBtn);
        lv_label_set_text(pLbl, presets[i].label);
        lv_obj_set_style_text_color(pLbl, DefaultTheme::getPrimaryAccent(), 0);
        lv_obj_center(pLbl);
    }
}

void UtilitiesView::screen_delete_cb(lv_event_t* e) {
    if (s_swTimerTask) {
        lv_timer_del(s_swTimerTask);
        s_swTimerTask = nullptr;
    }
    s_todoItemsCont = nullptr;
    s_todoDropdown = nullptr;
    s_todoTextArea = nullptr;
    s_calcHistLabel = nullptr;
    s_calcMainLabel = nullptr;
    s_swTimeLabel = nullptr;
    s_swStartBtn = nullptr;
    s_swStartLbl = nullptr;
    s_swLapsCont = nullptr;
    s_timerDisplayLabel = nullptr;
}

lv_obj_t* UtilitiesView::create() {
    lv_obj_t* screen = lv_obj_create(NULL);
    DefaultTheme::applyFlatBg(screen);
    DefaultTheme::disableScroll(screen);

    WallpaperManager::getInstance().applyWallpaper(screen);

    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(screen, 8, 0);
    lv_obj_set_style_pad_row(screen, 6, 0);

    headerBar = HeaderBar::create(screen, "Utilidades", true, true);

    // Tabview principal con 3 pestañas
    lv_obj_t* tabview = lv_tabview_create(screen);
    lv_tabview_set_tab_bar_position(tabview, LV_DIR_TOP);
    lv_tabview_set_tab_bar_size(tabview, 42);
    lv_obj_set_width(tabview, lv_pct(100));
    lv_obj_set_flex_grow(tabview, 1);
    lv_obj_set_style_bg_opa(tabview, 0, 0);
    lv_obj_set_style_border_width(tabview, 0, 0);

    // Estilizar barra de pestañas
    lv_obj_t* tab_bar = lv_tabview_get_tab_bar(tabview);
    DefaultTheme::applySunkenCard(tab_bar, 10);
    lv_obj_set_style_pad_all(tab_bar, 2, 0);
    lv_obj_set_style_text_color(tab_bar, DefaultTheme::getTextColor(), 0);

    lv_obj_t* tab_todo = lv_tabview_add_tab(tabview, "Notas");
    lv_obj_t* tab_calc = lv_tabview_add_tab(tabview, "Calculadora");
    lv_obj_t* tab_sw   = lv_tabview_add_tab(tabview, "Cronometro");

    buildTodoTab(tab_todo);
    buildCalcTab(tab_calc);
    buildStopwatchTab(tab_sw);

    // Iniciar timer global para el cronómetro/temporizador a 30ms
    if (!s_swTimerTask) {
        s_swTimerTask = lv_timer_create(sw_timer_callback, 30, NULL);
    }

    lv_obj_add_event_cb(screen, screen_delete_cb, LV_EVENT_DELETE, NULL);

    return screen;
}
