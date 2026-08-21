#include "TextEditorView.hpp"
#include "LuaRunnerView.hpp"
#include "cbdos/storage.hpp"
#include "cbdos/display.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <cstdio>

namespace cbdos {
namespace ui {

TextEditorView::TextEditorView(const std::string& initialPath)
    : BaseView("Editor"),
      m_fileLabel(nullptr),
      m_btnRun(nullptr),
      m_btnKb(nullptr),
      m_textArea(nullptr),
      m_keyboard(nullptr),
      m_modalMask(nullptr),
      m_saveAsTa(nullptr),
      m_currentFilePath(initialPath),
      m_isModified(false),
      m_keyboardVisible(true) {
}

void TextEditorView::updateTitle() {
    if (!m_fileLabel || !lv_obj_is_valid(m_fileLabel)) return;

    std::string prefix = "[SD] ";
    std::string name = m_currentFilePath.empty() ? "(Sin Titulo)" : m_currentFilePath;
    if (m_isModified) {
        name += " *";
    }
    std::string fullTitle = prefix + name;
    lv_label_set_text(m_fileLabel, fullTitle.c_str());
    updateRunButtonVisibility();
}

void TextEditorView::updateRunButtonVisibility() {
    if (!m_btnRun || !lv_obj_is_valid(m_btnRun)) return;

    std::string pathLower = m_currentFilePath;
    std::transform(pathLower.begin(), pathLower.end(), pathLower.begin(), ::tolower);

    bool isLua = (pathLower.size() >= 4 && pathLower.rfind(".lua") == pathLower.size() - 4);
    if (isLua) {
        lv_obj_remove_flag(m_btnRun, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(m_btnRun, LV_OBJ_FLAG_HIDDEN);
    }
}

void TextEditorView::loadFile(const std::string& path) {
    m_currentFilePath = path;
    m_isModified = false;

    if (m_textArea && lv_obj_is_valid(m_textArea)) {
        if (!path.empty()) {
            std::string content = cbdos::storage::readFile(path.c_str());
            lv_textarea_set_text(m_textArea, content.c_str());
        } else {
            lv_textarea_set_text(m_textArea, "");
        }
    }
    updateTitle();
}

void TextEditorView::taEventCb(lv_event_t* e) {
    TextEditorView* self = static_cast<TextEditorView*>(lv_event_get_user_data(e));
    if (!self) return;

    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED) {
        if (!self->m_isModified) {
            self->m_isModified = true;
            self->updateTitle();
        }
    } else if (code == LV_EVENT_FOCUSED || code == LV_EVENT_CLICKED) {
        if (self->m_keyboard && lv_obj_is_valid(self->m_keyboard)) {
            lv_keyboard_set_textarea(self->m_keyboard, self->m_textArea);
            if (!self->m_keyboardVisible) {
                self->m_keyboardVisible = true;
                lv_obj_remove_flag(self->m_keyboard, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
}

void TextEditorView::btnToggleKbCb(lv_event_t* e) {
    TextEditorView* self = static_cast<TextEditorView*>(lv_event_get_user_data(e));
    if (!self || !self->m_keyboard || !lv_obj_is_valid(self->m_keyboard)) return;

    self->m_keyboardVisible = !self->m_keyboardVisible;
    if (self->m_keyboardVisible) {
        lv_obj_remove_flag(self->m_keyboard, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(self->m_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}

void TextEditorView::editorKbEventCb(lv_event_t* e) {
    TextEditorView* self = static_cast<TextEditorView*>(lv_event_get_user_data(e));
    if (!self) return;

    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        if (self->m_keyboard && lv_obj_is_valid(self->m_keyboard)) {
            self->m_keyboardVisible = false;
            lv_obj_add_flag(self->m_keyboard, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void TextEditorView::btnSaveCb(lv_event_t* e) {
    TextEditorView* self = static_cast<TextEditorView*>(lv_event_get_user_data(e));
    if (!self) return;

    if (self->m_currentFilePath.empty()) {
        self->showSaveAsModal();
        return;
    }

    if (!self->m_textArea || !lv_obj_is_valid(self->m_textArea)) return;

    const char* txt = lv_textarea_get_text(self->m_textArea);
    std::string content = txt ? txt : "";

    bool ok = cbdos::storage::writeFile(self->m_currentFilePath.c_str(), content);
    if (ok) {
        self->m_isModified = false;
        self->updateTitle();
        UIManager::showToast("Archivo guardado con exito");
    } else {
        UIManager::showToast("Error al guardar archivo");
    }
}

void TextEditorView::btnSaveAsCb(lv_event_t* e) {
    TextEditorView* self = static_cast<TextEditorView*>(lv_event_get_user_data(e));
    if (self) {
        self->showSaveAsModal();
    }
}

void TextEditorView::btnNewCb(lv_event_t* e) {
    TextEditorView* self = static_cast<TextEditorView*>(lv_event_get_user_data(e));
    if (!self) return;

    self->m_currentFilePath = "";
    self->m_isModified = false;
    if (self->m_textArea && lv_obj_is_valid(self->m_textArea)) {
        lv_textarea_set_text(self->m_textArea, "");
    }
    self->updateTitle();
    UIManager::showToast("Nuevo documento creado");
}

void TextEditorView::btnOpenCb(lv_event_t* e) {
    TextEditorView* self = static_cast<TextEditorView*>(lv_event_get_user_data(e));
    if (self) {
        self->showOpenFileModal();
    }
}

void TextEditorView::btnRunCb(lv_event_t* e) {
    TextEditorView* self = static_cast<TextEditorView*>(lv_event_get_user_data(e));
    if (!self) return;

    if (self->m_currentFilePath.empty()) {
        UIManager::showToast("Guarda el script (.lua) antes de ejecutar");
        return;
    }

    // Auto-guardar si hubo cambios antes de ejecutar
    if (self->m_isModified && self->m_textArea && lv_obj_is_valid(self->m_textArea)) {
        const char* txt = lv_textarea_get_text(self->m_textArea);
        std::string content = txt ? txt : "";
        cbdos::storage::writeFile(self->m_currentFilePath.c_str(), content);
        self->m_isModified = false;
        self->updateTitle();
    }

    UIManager::getInstance().pushView(std::make_shared<LuaRunnerView>(self->m_currentFilePath));
}

// ── Modal: Guardar Como ──────────────────────────────────────────
void TextEditorView::modalSaveConfirmCb(lv_event_t* e) {
    TextEditorView* self = static_cast<TextEditorView*>(lv_event_get_user_data(e));
    if (!self || !self->m_saveAsTa || !lv_obj_is_valid(self->m_saveAsTa)) return;

    const char* p = lv_textarea_get_text(self->m_saveAsTa);
    std::string path = p ? p : "";
    if (path.empty() || path == "/") {
        UIManager::showToast("Ruta o nombre no valido");
        return;
    }

    if (path[0] != '/') {
        path = "/" + path;
    }

    self->m_currentFilePath = path;

    const char* txt = lv_textarea_get_text(self->m_textArea);
    std::string content = txt ? txt : "";

    bool ok = cbdos::storage::writeFile(self->m_currentFilePath.c_str(), content);
    if (ok) {
        self->m_isModified = false;
        self->updateTitle();
        UIManager::showToast("Guardado correctamente");
    } else {
        UIManager::showToast("Error al guardar archivo");
    }

    if (self->m_modalMask && lv_obj_is_valid(self->m_modalMask)) {
        lv_obj_delete_async(self->m_modalMask);
        self->m_modalMask = nullptr;
    }
    self->m_saveAsTa = nullptr;
}

void TextEditorView::modalSaveCancelCb(lv_event_t* e) {
    TextEditorView* self = static_cast<TextEditorView*>(lv_event_get_user_data(e));
    if (self && self->m_modalMask && lv_obj_is_valid(self->m_modalMask)) {
        lv_obj_delete_async(self->m_modalMask);
        self->m_modalMask = nullptr;
        self->m_saveAsTa = nullptr;
    }
}

void TextEditorView::showSaveAsModal() {
    if (m_modalMask && lv_obj_is_valid(m_modalMask)) {
        lv_obj_delete(m_modalMask);
        m_modalMask = nullptr;
    }

    // Ocultar teclado base para evitar superposiciones
    if (m_keyboard && lv_obj_is_valid(m_keyboard)) {
        lv_obj_add_flag(m_keyboard, LV_OBJ_FLAG_HIDDEN);
        m_keyboardVisible = false;
    }

    auto caps = cbdos::display::getCapabilities();
    int32_t screenW = caps.width > 0 ? caps.width : 480;
    int32_t screenH = caps.height > 0 ? caps.height : 800;

    m_modalMask = lv_obj_create(lv_layer_top());
    lv_obj_set_size(m_modalMask, screenW, screenH);
    lv_obj_set_pos(m_modalMask, 0, 0);
    lv_obj_set_style_bg_color(m_modalMask, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(m_modalMask, LV_OPA_80, 0);
    lv_obj_set_style_border_width(m_modalMask, 0, 0);
    lv_obj_set_style_pad_all(m_modalMask, 0, 0);

    int32_t modalW = (screenW >= 480) ? 420 : 300;
    int32_t modalH = (screenH >= 800) ? 220 : 190;

    lv_obj_t* modal = lv_obj_create(m_modalMask);
    lv_obj_set_size(modal, modalW, modalH);
    DefaultTheme::applyRaisedCard(modal, 16);
    lv_obj_set_align(modal, LV_ALIGN_TOP_MID);
    lv_obj_set_style_margin_top(modal, (screenH >= 800) ? 30 : 12, 0);
    lv_obj_set_flex_flow(modal, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(modal, 10, 0);
    lv_obj_set_style_pad_row(modal, 8, 0);
    DefaultTheme::disableScroll(modal);

    // Título
    lv_obj_t* title = lv_label_create(modal);
    lv_label_set_text(title, "Guardar Archivo Como...");
    lv_obj_set_style_text_color(title, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);

    // Campo de texto de ruta
    m_saveAsTa = lv_textarea_create(modal);
    lv_obj_set_size(m_saveAsTa, LV_PCT(100), 38);
    DefaultTheme::applySunkenCard(m_saveAsTa, 8);
    lv_textarea_set_one_line(m_saveAsTa, true);
    std::string defaultPath = m_currentFilePath.empty() ? "/sdcard/scripts/nuevo.lua" : m_currentFilePath;
    lv_textarea_set_text(m_saveAsTa, defaultPath.c_str());
    lv_obj_set_style_text_font(m_saveAsTa, &lv_font_montserrat_12, 0);

    // Fila de botones Cancelar / Guardar
    lv_obj_t* btnRow = lv_obj_create(modal);
    lv_obj_set_size(btnRow, LV_PCT(100), 40);
    lv_obj_set_style_bg_opa(btnRow, 0, 0);
    lv_obj_set_style_border_width(btnRow, 0, 0);
    lv_obj_set_flex_flow(btnRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btnRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(btnRow, 0, 0);
    DefaultTheme::disableScroll(btnRow);

    int32_t btnW = (modalW / 2) - 10;

    lv_obj_t* btnCancel = lv_button_create(btnRow);
    lv_obj_set_size(btnCancel, btnW, 36);
    DefaultTheme::applyButton(btnCancel, 8);
    lv_obj_t* lblC = lv_label_create(btnCancel);
    lv_label_set_text(lblC, "Cancelar");
    lv_obj_center(lblC);
    lv_obj_add_event_cb(btnCancel, modalSaveCancelCb, LV_EVENT_CLICKED, this);

    lv_obj_t* btnSave = lv_button_create(btnRow);
    lv_obj_set_size(btnSave, btnW, 36);
    DefaultTheme::applyButton(btnSave, 8);
    lv_obj_set_style_bg_color(btnSave, lv_color_hex(0x1B5E20), 0);
    lv_obj_t* lblS = lv_label_create(btnSave);
    lv_label_set_text(lblS, "Guardar");
    lv_obj_set_style_text_color(lblS, lv_color_hex(0x00E676), 0);
    lv_obj_center(lblS);
    lv_obj_add_event_cb(btnSave, modalSaveConfirmCb, LV_EVENT_CLICKED, this);

    // Teclado virtual en la parte inferior del modal
    lv_obj_t* kb = lv_keyboard_create(m_modalMask);
    lv_obj_set_size(kb, LV_PCT(100), (screenH >= 800) ? 320 : 230);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(kb, lv_color_hex(0x1B1E29), 0);
    lv_obj_set_style_border_color(kb, lv_color_hex(0x2E3444), 0);
    lv_obj_set_style_border_width(kb, 1, 0);
    lv_obj_set_style_radius(kb, 0, 0);
    lv_keyboard_set_textarea(kb, m_saveAsTa);
    lv_obj_add_event_cb(kb, modalSaveConfirmCb, LV_EVENT_READY, this);
    lv_obj_add_event_cb(kb, modalSaveCancelCb, LV_EVENT_CANCEL, this);
}

// ── Modal: Abrir Archivo ──────────────────────────────────────────
void TextEditorView::modalOpenCloseCb(lv_event_t* e) {
    TextEditorView* self = static_cast<TextEditorView*>(lv_event_get_user_data(e));
    if (self && self->m_modalMask && lv_obj_is_valid(self->m_modalMask)) {
        lv_obj_delete_async(self->m_modalMask);
        self->m_modalMask = nullptr;
    }
}

void TextEditorView::modalFileItemCb(lv_event_t* e) {
    TextEditorView* self = static_cast<TextEditorView*>(lv_event_get_user_data(e));
    lv_obj_t* btn = static_cast<lv_obj_t*>(lv_event_get_target(e));
    if (!self || !btn) return;

    int idx = (int)(intptr_t)lv_obj_get_user_data(btn);
    if (idx >= 0 && idx < (int)self->m_foundFiles.size()) {
        self->loadFile(self->m_foundFiles[idx]);
        UIManager::showToast("Archivo cargado");
    }

    if (self->m_modalMask && lv_obj_is_valid(self->m_modalMask)) {
        lv_obj_delete_async(self->m_modalMask);
        self->m_modalMask = nullptr;
    }
}

void TextEditorView::modalScrollUpCb(lv_event_t* e) {
    lv_obj_t* targetList = static_cast<lv_obj_t*>(lv_event_get_user_data(e));
    if (targetList && lv_obj_is_valid(targetList)) {
        lv_obj_scroll_by_bounded(targetList, 0, 120, LV_ANIM_ON);
    }
}

void TextEditorView::modalScrollDownCb(lv_event_t* e) {
    lv_obj_t* targetList = static_cast<lv_obj_t*>(lv_event_get_user_data(e));
    if (targetList && lv_obj_is_valid(targetList)) {
        lv_obj_scroll_by_bounded(targetList, 0, -120, LV_ANIM_ON);
    }
}

void TextEditorView::scanTextFilesSD() {
    m_foundFiles.clear();

    if (!cbdos::storage::isSdMounted()) {
        cbdos::storage::mountSd();
    }

    if (!cbdos::storage::isSdMounted()) {
        return;
    }

    std::vector<std::string> dirsToScan = {"/sdcard"};

    for (size_t d = 0; d < dirsToScan.size() && dirsToScan.size() < 50; d++) {
        std::string currentDir = dirsToScan[d];
        auto entries = cbdos::storage::listDir(currentDir.c_str());
        for (const auto& f : entries) {
            std::string fullPath = currentDir;
            if (fullPath.back() != '/') fullPath += '/';
            fullPath += f.name;

            if (f.isDirectory) {
                if (f.name != "System Volume Information" && f.name != ".Spotlight-V100" && 
                    f.name != ".Trashes" && f.name[0] != '.') {
                    dirsToScan.push_back(fullPath);
                }
            } else {
                std::string nameLower = f.name;
                std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                size_t dotPos = nameLower.rfind('.');
                if (dotPos != std::string::npos) {
                    std::string ext = nameLower.substr(dotPos);
                    if (ext == ".lua" || ext == ".txt" || ext == ".json" || 
                        ext == ".ini" || ext == ".log" || ext == ".csv" || ext == ".md") {
                        m_foundFiles.push_back(fullPath);
                    }
                }
            }
        }
    }
}

void TextEditorView::showOpenFileModal() {
    if (m_modalMask && lv_obj_is_valid(m_modalMask)) {
        lv_obj_delete(m_modalMask);
        m_modalMask = nullptr;
    }

    // Ocultar teclado si está visible para liberar espacio
    if (m_keyboard && lv_obj_is_valid(m_keyboard)) {
        lv_obj_add_flag(m_keyboard, LV_OBJ_FLAG_HIDDEN);
        m_keyboardVisible = false;
    }

    scanTextFilesSD();

    auto caps = cbdos::display::getCapabilities();
    int32_t screenW = caps.width > 0 ? caps.width : 480;
    int32_t screenH = caps.height > 0 ? caps.height : 800;

    m_modalMask = lv_obj_create(lv_layer_top());
    lv_obj_set_size(m_modalMask, screenW, screenH);
    lv_obj_set_pos(m_modalMask, 0, 0);
    lv_obj_set_style_bg_color(m_modalMask, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(m_modalMask, LV_OPA_80, 0);
    lv_obj_set_style_border_width(m_modalMask, 0, 0);
    lv_obj_set_style_pad_all(m_modalMask, 0, 0);

    int32_t modalW = (screenW >= 480) ? 420 : 300;
    int32_t modalH = (screenH >= 800) ? 520 : 400;

    lv_obj_t* modal = lv_obj_create(m_modalMask);
    lv_obj_set_size(modal, modalW, modalH);
    DefaultTheme::applyRaisedCard(modal, 16);
    lv_obj_center(modal);
    lv_obj_set_flex_flow(modal, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(modal, 10, 0);
    lv_obj_set_style_pad_row(modal, 8, 0);

    // Header del modal
    lv_obj_t* header = lv_obj_create(modal);
    lv_obj_set_size(header, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(header, 0, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(header, 0, 0);
    DefaultTheme::disableScroll(header);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, "Abrir Archivo");
    lv_obj_set_style_text_color(title, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);

    lv_obj_t* navRow = lv_obj_create(header);
    lv_obj_set_size(navRow, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(navRow, 0, 0);
    lv_obj_set_style_border_width(navRow, 0, 0);
    lv_obj_set_flex_flow(navRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(navRow, 0, 0);
    lv_obj_set_style_pad_column(navRow, 6, 0);
    DefaultTheme::disableScroll(navRow);

    // Botón Scroll Arriba (⬆️)
    lv_obj_t* btnUp = lv_button_create(navRow);
    lv_obj_set_size(btnUp, 32, 30);
    DefaultTheme::applyButton(btnUp, 8);
    lv_obj_t* lblUp = lv_label_create(btnUp);
    lv_label_set_text(lblUp, LV_SYMBOL_UP);
    lv_obj_center(lblUp);

    // Botón Scroll Abajo (⬇️)
    lv_obj_t* btnDown = lv_button_create(navRow);
    lv_obj_set_size(btnDown, 32, 30);
    DefaultTheme::applyButton(btnDown, 8);
    lv_obj_t* lblDown = lv_label_create(btnDown);
    lv_label_set_text(lblDown, LV_SYMBOL_DOWN);
    lv_obj_center(lblDown);

    // Botón Cerrar (✖️)
    lv_obj_t* btnClose = lv_button_create(navRow);
    lv_obj_set_size(btnClose, 32, 30);
    DefaultTheme::applyButton(btnClose, 8);
    lv_obj_t* lblX = lv_label_create(btnClose);
    lv_label_set_text(lblX, LV_SYMBOL_CLOSE);
    lv_obj_center(lblX);
    lv_obj_add_event_cb(btnClose, modalOpenCloseCb, LV_EVENT_CLICKED, this);

    // Lista de archivos
    lv_obj_t* list = lv_obj_create(modal);
    lv_obj_set_size(list, LV_PCT(100), LV_PCT(82));
    DefaultTheme::applySunkenCard(list, 10);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(list, 6, 0);
    lv_obj_set_style_pad_row(list, 4, 0);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);

    lv_obj_add_event_cb(btnUp, modalScrollUpCb, LV_EVENT_CLICKED, list);
    lv_obj_add_event_cb(btnDown, modalScrollDownCb, LV_EVENT_CLICKED, list);

    if (m_foundFiles.empty()) {
        lv_obj_t* emptyLbl = lv_label_create(list);
        lv_label_set_text(emptyLbl, "No se encontraron archivos de texto\no scripts en la MicroSD.");
        lv_obj_set_style_text_color(emptyLbl, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_align(emptyLbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(emptyLbl);
    } else {
        for (size_t i = 0; i < m_foundFiles.size(); i++) {
            lv_obj_t* item = lv_button_create(list);
            lv_obj_set_size(item, LV_PCT(100), 40);
            DefaultTheme::applyButton(item, 8);
            lv_obj_set_user_data(item, (void*)(intptr_t)i);
            lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(item, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_all(item, 6, 0);

            lv_obj_t* icon = lv_label_create(item);
            lv_label_set_text(icon, LV_SYMBOL_FILE);
            lv_obj_set_style_text_color(icon, DefaultTheme::getPrimaryAccent(), 0);

            lv_obj_t* name = lv_label_create(item);
            lv_label_set_text(name, m_foundFiles[i].c_str());
            lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
            lv_obj_set_flex_grow(name, 1);
            lv_obj_set_style_text_color(name, DefaultTheme::getTextColor(), 0);
            lv_obj_set_style_text_font(name, &lv_font_montserrat_12, 0);
            lv_obj_set_style_margin_left(name, 6, 0);

            lv_obj_add_event_cb(item, modalFileItemCb, LV_EVENT_CLICKED, this);
        }
    }
}

// ── Creación de la Vista Principal ────────────────────────────────
bool TextEditorView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;

    UIManager::getInstance().getHeaderBar().showWifi(false);

    // Contenedor principal de la vista
    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_container, 0, 0);
    lv_obj_set_style_pad_all(m_container, 6, 0);
    lv_obj_set_style_pad_row(m_container, 4, 0);
    lv_obj_set_flex_flow(m_container, LV_FLEX_FLOW_COLUMN);
    DefaultTheme::disableScroll(m_container);

    // 1. Info Bar (Ruta del archivo actual)
    lv_obj_t* infoBar = lv_obj_create(m_container);
    lv_obj_set_size(infoBar, LV_PCT(100), 28);
    DefaultTheme::applySunkenCard(infoBar, 6);
    lv_obj_set_flex_flow(infoBar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(infoBar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(infoBar, 8, 0);
    lv_obj_set_style_pad_ver(infoBar, 2, 0);
    DefaultTheme::disableScroll(infoBar);

    m_fileLabel = lv_label_create(infoBar);
    lv_label_set_long_mode(m_fileLabel, LV_LABEL_LONG_DOT);
    lv_obj_set_width(m_fileLabel, LV_PCT(100));
    lv_obj_set_style_text_color(m_fileLabel, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(m_fileLabel, &lv_font_montserrat_12, 0);

    // 2. Toolbar de botones de acción
    lv_obj_t* toolbar = lv_obj_create(m_container);
    lv_obj_set_size(toolbar, LV_PCT(100), 38);
    lv_obj_set_style_bg_opa(toolbar, 0, 0);
    lv_obj_set_style_border_width(toolbar, 0, 0);
    lv_obj_set_flex_flow(toolbar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(toolbar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(toolbar, 0, 0);
    DefaultTheme::disableScroll(toolbar);

    auto caps = cbdos::display::getCapabilities();
    int32_t btnW = (caps.width >= 480) ? 68 : 50;

    // Botón Guardar (Save)
    lv_obj_t* btnSave = lv_button_create(toolbar);
    lv_obj_set_size(btnSave, btnW, 34);
    DefaultTheme::applyButton(btnSave, 6);
    lv_obj_t* lblSave = lv_label_create(btnSave);
    lv_label_set_text(lblSave, LV_SYMBOL_SAVE);
    lv_obj_set_style_text_color(lblSave, lv_color_hex(0x00E676), 0);
    lv_obj_center(lblSave);
    lv_obj_add_event_cb(btnSave, btnSaveCb, LV_EVENT_CLICKED, this);

    // Botón Guardar Como (Save As)
    lv_obj_t* btnSaveAs = lv_button_create(toolbar);
    lv_obj_set_size(btnSaveAs, btnW, 34);
    DefaultTheme::applyButton(btnSaveAs, 6);
    lv_obj_t* lblSaveAs = lv_label_create(btnSaveAs);
    lv_label_set_text(lblSaveAs, LV_SYMBOL_SAVE "...");
    lv_obj_set_style_text_font(lblSaveAs, &lv_font_montserrat_12, 0);
    lv_obj_center(lblSaveAs);
    lv_obj_add_event_cb(btnSaveAs, btnSaveAsCb, LV_EVENT_CLICKED, this);

    // Botón Abrir (Open)
    lv_obj_t* btnOpen = lv_button_create(toolbar);
    lv_obj_set_size(btnOpen, btnW, 34);
    DefaultTheme::applyButton(btnOpen, 6);
    lv_obj_t* lblOpen = lv_label_create(btnOpen);
    lv_label_set_text(lblOpen, LV_SYMBOL_DIRECTORY);
    lv_obj_center(lblOpen);
    lv_obj_add_event_cb(btnOpen, btnOpenCb, LV_EVENT_CLICKED, this);

    // Botón Nuevo (New)
    lv_obj_t* btnNew = lv_button_create(toolbar);
    lv_obj_set_size(btnNew, btnW, 34);
    DefaultTheme::applyButton(btnNew, 6);
    lv_obj_t* lblNew = lv_label_create(btnNew);
    lv_label_set_text(lblNew, LV_SYMBOL_PLUS);
    lv_obj_center(lblNew);
    lv_obj_add_event_cb(btnNew, btnNewCb, LV_EVENT_CLICKED, this);

    // Botón Toggle Teclado
    m_btnKb = lv_button_create(toolbar);
    lv_obj_set_size(m_btnKb, btnW, 34);
    DefaultTheme::applyButton(m_btnKb, 6);
    lv_obj_t* lblKb = lv_label_create(m_btnKb);
    lv_label_set_text(lblKb, LV_SYMBOL_KEYBOARD);
    lv_obj_center(lblKb);
    lv_obj_add_event_cb(m_btnKb, btnToggleKbCb, LV_EVENT_CLICKED, this);

    // Botón Ejecutar Script (Run)
    m_btnRun = lv_button_create(toolbar);
    lv_obj_set_size(m_btnRun, (caps.width >= 480) ? 75 : 56, 34);
    DefaultTheme::applyButton(m_btnRun, 6);
    lv_obj_set_style_bg_color(m_btnRun, lv_color_hex(0x1B5E20), 0);
    lv_obj_t* lblRun = lv_label_create(m_btnRun);
    lv_label_set_text(lblRun, LV_SYMBOL_PLAY " Run");
    lv_obj_set_style_text_color(lblRun, lv_color_hex(0x00E676), 0);
    lv_obj_set_style_text_font(lblRun, &lv_font_montserrat_12, 0);
    lv_obj_center(lblRun);
    lv_obj_add_event_cb(m_btnRun, btnRunCb, LV_EVENT_CLICKED, this);

    // 3. Área de Texto Principal (Textarea)
    m_textArea = lv_textarea_create(m_container);
    lv_obj_set_width(m_textArea, LV_PCT(100));
    lv_obj_set_flex_grow(m_textArea, 1);
    DefaultTheme::applySunkenCard(m_textArea, 10);
    lv_obj_set_style_bg_color(m_textArea, lv_color_hex(0x0E1217), 0);
    lv_obj_set_style_text_color(m_textArea, lv_color_hex(0xE0E6ED), 0);
    lv_obj_set_style_text_font(m_textArea, &lv_font_montserrat_12, 0);
    lv_textarea_set_one_line(m_textArea, false);
    lv_textarea_set_cursor_click_pos(m_textArea, true);
    lv_obj_add_event_cb(m_textArea, taEventCb, LV_EVENT_ALL, this);

    // 4. Teclado Virtual Integrado
    m_keyboard = lv_keyboard_create(m_container);
    lv_obj_set_size(m_keyboard, LV_PCT(100), (caps.height >= 800) ? 310 : 210);
    lv_obj_set_style_bg_color(m_keyboard, lv_color_hex(0x1B1E29), 0);
    lv_obj_set_style_border_color(m_keyboard, lv_color_hex(0x2E3444), 0);
    lv_obj_set_style_border_width(m_keyboard, 1, 0);
    lv_obj_set_style_radius(m_keyboard, 0, 0);
    lv_keyboard_set_textarea(m_keyboard, m_textArea);
    lv_obj_add_event_cb(m_keyboard, editorKbEventCb, LV_EVENT_ALL, this);

    // Cargar contenido inicial
    if (!m_currentFilePath.empty()) {
        loadFile(m_currentFilePath);
    } else {
        updateTitle();
    }

    return true;
}

void TextEditorView::onDestroy() {
    if (m_modalMask && lv_obj_is_valid(m_modalMask)) {
        lv_obj_delete(m_modalMask);
        m_modalMask = nullptr;
    }

    m_fileLabel = nullptr;
    m_btnRun = nullptr;
    m_btnKb = nullptr;
    m_textArea = nullptr;
    m_keyboard = nullptr;
    m_saveAsTa = nullptr;

    UIManager::getInstance().getHeaderBar().clearRightAction();
    UIManager::getInstance().getHeaderBar().showWifi(true);
    BaseView::onDestroy();
}

void TextEditorView::onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) {
    (void)theme;
    (void)palette;
    if (m_container && lv_obj_is_valid(m_container)) {
        lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
    }
}

} // namespace ui
} // namespace cbdos
