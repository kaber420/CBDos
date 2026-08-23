#include "TodoApp.hpp"
#include "../../UIManager.hpp"
#include "../../themes/DefaultTheme.h"
#include "cbdos/storage.hpp"
#include "cbdos/msgpack_util.hpp"
#include <cstdio>
#include <vector>
#include <string>
#include <cstring>
#include <esp_log.h>

static const char* TAG = "TodoApp";
static const char* TASKS_STORAGE_PATH = "notes/tasks.msgpack";

namespace cbdos {
namespace ui {

static std::vector<TodoList> s_todoLists;
static size_t s_currentListIdx = 0;
static lv_obj_t* s_todoItemsCont = nullptr;
static lv_obj_t* s_todoDropdown = nullptr;
static lv_obj_t* s_todoTextArea = nullptr;

const std::vector<TodoList>& TodoApp::getLists() {
    return s_todoLists;
}

void TodoApp::addList(const std::string& name) {
    TodoList l;
    l.name = name;
    s_todoLists.push_back(l);
    saveToStorage();
}

void TodoApp::deleteList(size_t index) {
    if (index < s_todoLists.size()) {
        s_todoLists.erase(s_todoLists.begin() + index);
        saveToStorage();
    }
}

// ────────────────────────────────────────────────────────────────
//  Serialización y Deserialización MessagePack para Listas de Tareas
// ────────────────────────────────────────────────────────────────
static std::string serializeTodoListsToMsgPack(const std::vector<TodoList>& lists) {
    cbdos::msgpack::MsgPackWriter writer;
    writer.writeArrayHeader(lists.size());

    for (const auto& list : lists) {
        writer.writeMapHeader(2);

        writer.writeString("name");
        writer.writeString(list.name);

        writer.writeString("items");
        writer.writeArrayHeader(list.items.size());
        for (const auto& item : list.items) {
            writer.writeMapHeader(2);
            writer.writeString("text");
            writer.writeString(item.text);
            writer.writeString("done");
            writer.writeBool(item.done);
        }
    }

    return writer.toString();
}

static bool deserializeTodoListsFromMsgPack(const std::string& data, std::vector<TodoList>& outLists) {
    if (data.empty()) return false;

    cbdos::msgpack::MsgPackReader reader(data);
    size_t listCount = 0;
    if (!reader.readArrayHeader(listCount)) return false;

    outLists.clear();
    for (size_t i = 0; i < listCount; i++) {
        size_t mapSize = 0;
        if (!reader.readMapHeader(mapSize)) break;

        TodoList list;
        for (size_t m = 0; m < mapSize; m++) {
            std::string key;
            if (!reader.readString(key)) break;

            if (key == "name") {
                reader.readString(list.name);
            } else if (key == "items") {
                size_t itemCount = 0;
                if (reader.readArrayHeader(itemCount)) {
                    for (size_t j = 0; j < itemCount; j++) {
                        size_t itemMapSize = 0;
                        if (!reader.readMapHeader(itemMapSize)) break;

                        TodoItem item;
                        for (size_t im = 0; im < itemMapSize; im++) {
                            std::string iKey;
                            if (!reader.readString(iKey)) break;

                            if (iKey == "text") {
                                reader.readString(item.text);
                            } else if (iKey == "done") {
                                reader.readBool(item.done);
                            } else {
                                reader.skipValue();
                            }
                        }
                        list.items.push_back(item);
                    }
                }
            } else {
                reader.skipValue();
            }
        }
        outLists.push_back(list);
    }

    return !outLists.empty();
}

void TodoApp::loadFromStorage() {
    s_todoLists.clear();

    // 1. Cargar desde Flash interna SPIFFS en formato MessagePack
    if (cbdos::storage::fileExists(TASKS_STORAGE_PATH)) {
        std::string rawData = cbdos::storage::readFile(TASKS_STORAGE_PATH);
        if (deserializeTodoListsFromMsgPack(rawData, s_todoLists)) {
            ESP_LOGI(TAG, "Cargadas %d listas de tareas desde %s (MessagePack)", (int)s_todoLists.size(), TASKS_STORAGE_PATH);
            return;
        }
    }

    // 2. Si no hay datos (primer arranque), inicializar listas predeterminadas
    ESP_LOGI(TAG, "Inicializando listas de tareas predeterminadas en Flash SPIFFS...");
    TodoList compras;
    compras.name = "Compras";
    compras.items.push_back({"Leche y Cafe", false});
    compras.items.push_back({"Huevos y Frutas", false});
    compras.items.push_back({"Pan integral", true});
    s_todoLists.push_back(compras);

    TodoList tareas;
    tareas.name = "Tareas";
    tareas.items.push_back({"Revisar codigo CBDos", false});
    tareas.items.push_back({"Probar emuladores", false});
    s_todoLists.push_back(tareas);

    saveToStorage();
}

void TodoApp::saveToStorage() {
    std::string binData = serializeTodoListsToMsgPack(s_todoLists);
    if (!binData.empty()) {
        bool ok = cbdos::storage::writeFile(TASKS_STORAGE_PATH, binData);
        if (ok) {
            ESP_LOGI(TAG, "Guardadas %d listas de tareas en %s (MessagePack, %d bytes)", (int)s_todoLists.size(), TASKS_STORAGE_PATH, (int)binData.size());
        } else {
            ESP_LOGE(TAG, "Error al escribir tareas en %s", TASKS_STORAGE_PATH);
        }
    }
}

bool TodoApp::exportToSd(const std::string& sdPath) {
    if (!cbdos::storage::isSdMounted()) {
        ESP_LOGW(TAG, "exportToSd: MicroSD no montada");
        return false;
    }
    std::string binData = serializeTodoListsToMsgPack(s_todoLists);
    return cbdos::storage::writeFile(sdPath.c_str(), binData);
}

bool TodoApp::importFromSd(const std::string& sdPath) {
    if (!cbdos::storage::fileExists(sdPath.c_str())) return false;

    std::string rawData = cbdos::storage::readFile(sdPath.c_str());
    std::vector<TodoList> imported;
    if (!deserializeTodoListsFromMsgPack(rawData, imported)) return false;

    for (const auto& l : imported) {
        s_todoLists.push_back(l);
    }
    saveToStorage();
    refreshListUI();
    return true;
}

void TodoApp::checkboxEventCb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t* cb = (lv_obj_t*)lv_event_get_target(e);
        size_t itemIdx = (size_t)(intptr_t)lv_obj_get_user_data(cb);
        if (s_currentListIdx < s_todoLists.size() && itemIdx < s_todoLists[s_currentListIdx].items.size()) {
            bool checked = lv_obj_has_state(cb, LV_STATE_CHECKED);
            s_todoLists[s_currentListIdx].items[itemIdx].done = checked;
            saveToStorage();
            refreshListUI();
        }
    }
}

void TodoApp::deleteItemCb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
        size_t itemIdx = (size_t)(intptr_t)lv_obj_get_user_data(btn);
        if (s_currentListIdx < s_todoLists.size() && itemIdx < s_todoLists[s_currentListIdx].items.size()) {
            s_todoLists[s_currentListIdx].items.erase(s_todoLists[s_currentListIdx].items.begin() + itemIdx);
            saveToStorage();
            refreshListUI();
            UIManager::showToast("Tarea eliminada");
        }
    }
}

void TodoApp::refreshListUI() {
    if (!s_todoItemsCont || !lv_obj_is_valid(s_todoItemsCont)) return;
    lv_obj_clean(s_todoItemsCont);

    if (s_currentListIdx >= s_todoLists.size()) {
        s_currentListIdx = 0;
    }

    if (s_todoLists.empty()) {
        lv_obj_t* emptyLbl = lv_label_create(s_todoItemsCont);
        lv_label_set_text(emptyLbl, "No hay tareas");
        lv_obj_set_style_text_color(emptyLbl, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_center(emptyLbl);
        return;
    }

    const auto& currentItems = s_todoLists[s_currentListIdx].items;
    if (currentItems.empty()) {
        lv_obj_t* emptyLbl = lv_label_create(s_todoItemsCont);
        lv_label_set_text(emptyLbl, "Lista vacia.\nEscribe abajo para agregar.");
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
        lv_obj_add_event_cb(cb, checkboxEventCb, LV_EVENT_VALUE_CHANGED, NULL);

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
        lv_obj_add_event_cb(delBtn, deleteItemCb, LV_EVENT_CLICKED, NULL);

        lv_obj_t* trashIcon = lv_label_create(delBtn);
        lv_label_set_text(trashIcon, LV_SYMBOL_TRASH);
        lv_obj_set_style_text_color(trashIcon, lv_color_hex(0xFF4B6E), 0);
        lv_obj_center(trashIcon);
    }
}

void TodoApp::addBtnCb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED || code == LV_EVENT_READY) {
        if (!s_todoTextArea) return;
        const char* text = lv_textarea_get_text(s_todoTextArea);
        if (text && strlen(text) > 0) {
            if (s_currentListIdx < s_todoLists.size()) {
                s_todoLists[s_currentListIdx].items.push_back({text, false});
                saveToStorage();
                refreshListUI();
                lv_textarea_set_text(s_todoTextArea, "");
                UIManager::showToast("Tarea agregada");
            }
        }
    }
}

void TodoApp::clearCompletedCb(lv_event_t* e) {
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
                saveToStorage();
                refreshListUI();
                UIManager::showToast("Tareas completadas limpiadas");
            }
        }
    }
}

void TodoApp::dropdownCb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t* dd = (lv_obj_t*)lv_event_get_target(e);
        s_currentListIdx = lv_dropdown_get_selected(dd);
        refreshListUI();
    }
}

void TodoApp::build(lv_obj_t* parent) {
    loadFromStorage();

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
    lv_obj_add_event_cb(s_todoDropdown, dropdownCb, LV_EVENT_VALUE_CHANGED, NULL);

    // Botón Limpiar Completadas
    lv_obj_t* clearBtn = lv_button_create(topRow);
    lv_obj_set_size(clearBtn, 110, 40);
    DefaultTheme::applyButton(clearBtn, 10);
    lv_obj_add_event_cb(clearBtn, clearCompletedCb, LV_EVENT_CLICKED, NULL);

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

    refreshListUI();

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
    lv_obj_add_event_cb(s_todoTextArea, addBtnCb, LV_EVENT_READY, NULL);

    lv_obj_t* addBtn = lv_button_create(inputRow);
    lv_obj_set_size(addBtn, 44, 44);
    DefaultTheme::applyButton(addBtn, 10);
    lv_obj_set_style_bg_color(addBtn, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_add_event_cb(addBtn, addBtnCb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* addIcon = lv_label_create(addBtn);
    lv_label_set_text(addIcon, LV_SYMBOL_PLUS);
    lv_obj_set_style_text_color(addIcon, lv_color_hex(0x000000), 0);
    lv_obj_center(addIcon);
}

void TodoApp::cleanup() {
    s_todoItemsCont = nullptr;
    s_todoDropdown = nullptr;
    s_todoTextArea = nullptr;
}

} // namespace ui
} // namespace cbdos
