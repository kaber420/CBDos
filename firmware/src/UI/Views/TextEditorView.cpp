#include "TextEditorView.h"
#include "../UIManager.h"
#include "../Components/HeaderBar.h"
#include "../Themes/DefaultTheme.h"
#include "../WallpaperManager.h"
#include <algorithm>
#include <cstring>

lv_obj_t* TextEditorView::textArea = nullptr;
lv_obj_t* TextEditorView::keyboard = nullptr;
lv_obj_t* TextEditorView::fileLabel = nullptr;
lv_obj_t* TextEditorView::btnRun = nullptr;
lv_obj_t* TextEditorView::btnKb = nullptr;
std::string TextEditorView::currentFilePath = "";
StorageType TextEditorView::currentStorage = StorageType::SD_CARD;
bool TextEditorView::isModified = false;
bool TextEditorView::keyboardVisible = true;

void TextEditorView::updateTitle() {
    if (!fileLabel || !lv_obj_is_valid(fileLabel)) return;

    std::string prefix = (currentStorage == StorageType::SD_CARD) ? "[SD] " : "[Flash] ";
    std::string name = currentFilePath.empty() ? "(Sin Título)" : currentFilePath;
    if (isModified) {
        name += " *";
    }
    std::string fullTitle = prefix + name;
    lv_label_set_text(fileLabel, fullTitle.c_str());
    updateRunButtonVisibility();
}

void TextEditorView::updateRunButtonVisibility() {
    if (!btnRun || !lv_obj_is_valid(btnRun)) return;

    std::string pathLower = currentFilePath;
    std::transform(pathLower.begin(), pathLower.end(), pathLower.begin(), ::tolower);

    bool isLua = (pathLower.size() >= 4 && pathLower.rfind(".lua") == pathLower.size() - 4);
    if (isLua) {
        lv_obj_remove_flag(btnRun, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(btnRun, LV_OBJ_FLAG_HIDDEN);
    }
}

void TextEditorView::loadFileContent(const std::string& path, StorageType storage) {
    currentFilePath = path;
    currentStorage = storage;
    isModified = false;

    if (textArea && lv_obj_is_valid(textArea)) {
        if (!path.empty()) {
            std::string content = StorageManager::readFile(storage, path);
            lv_textarea_set_text(textArea, content.c_str());
        } else {
            lv_textarea_set_text(textArea, "");
        }
    }
    updateTitle();
}

void TextEditorView::screen_delete_cb(lv_event_t* e) {
    textArea = nullptr;
    keyboard = nullptr;
    fileLabel = nullptr;
    btnRun = nullptr;
    btnKb = nullptr;
}

void TextEditorView::ta_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED) {
        if (!isModified) {
            isModified = true;
            updateTitle();
        }
    } else if (code == LV_EVENT_FOCUSED || code == LV_EVENT_CLICKED) {
        if (keyboard && lv_obj_is_valid(keyboard)) {
            lv_keyboard_set_textarea(keyboard, textArea);
            if (!keyboardVisible) {
                keyboardVisible = true;
                lv_obj_remove_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
}

void TextEditorView::btn_toggle_kb_cb(lv_event_t* e) {
    if (!keyboard || !lv_obj_is_valid(keyboard)) return;

    keyboardVisible = !keyboardVisible;
    if (keyboardVisible) {
        lv_obj_remove_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}

void TextEditorView::btn_save_cb(lv_event_t* e) {
    if (currentFilePath.empty()) {
        showSaveAsModal();
        return;
    }

    if (!textArea || !lv_obj_is_valid(textArea)) return;

    const char* txt = lv_textarea_get_text(textArea);
    std::string content = txt ? txt : "";

    bool ok = StorageManager::writeFile(currentStorage, currentFilePath, content);
    if (ok) {
        isModified = false;
        updateTitle();
        UIManager::showToast("Archivo guardado con éxito");
    } else {
        UIManager::showToast("Error al guardar archivo");
    }
}

void TextEditorView::btn_save_as_cb(lv_event_t* e) {
    showSaveAsModal();
}

void TextEditorView::btn_new_cb(lv_event_t* e) {
    currentFilePath = "";
    isModified = false;
    if (textArea && lv_obj_is_valid(textArea)) {
        lv_textarea_set_text(textArea, "");
    }
    updateTitle();
    UIManager::showToast("Nuevo documento creado");
}

void TextEditorView::btn_open_cb(lv_event_t* e) {
    showOpenFileModal();
}

void TextEditorView::btn_run_cb(lv_event_t* e) {
    if (currentFilePath.empty()) {
        UIManager::showToast("Guarda el script (.lua) antes de ejecutar");
        return;
    }

    // Auto-guardar si hubo cambios antes de ejecutar
    if (isModified && textArea && lv_obj_is_valid(textArea)) {
        const char* txt = lv_textarea_get_text(textArea);
        std::string content = txt ? txt : "";
        StorageManager::writeFile(currentStorage, currentFilePath, content);
        isModified = false;
        updateTitle();
    }

    UIManager::getInstance().loadLuaRunner(currentFilePath);
}

// ── Modal: Guardar Como ──────────────────────────────────────────
struct SaveAsContext {
    lv_obj_t* mask;
    lv_obj_t* taPath;
    StorageType targetStorage;
};

void TextEditorView::modal_save_confirm_cb(lv_event_t* e) {
    SaveAsContext* ctx = (SaveAsContext*)lv_event_get_user_data(e);
    if (!ctx) return;

    const char* p = lv_textarea_get_text(ctx->taPath);
    std::string path = p ? p : "";
    if (path.empty() || path == "/") {
        UIManager::showToast("Ruta o nombre de archivo no válido");
        return;
    }

    // Asegurar que comience con '/'
    if (path[0] != '/') {
        path = "/" + path;
    }

    currentFilePath = path;
    currentStorage = ctx->targetStorage;

    const char* txt = lv_textarea_get_text(textArea);
    std::string content = txt ? txt : "";

    bool ok = StorageManager::writeFile(currentStorage, currentFilePath, content);
    if (ok) {
        isModified = false;
        updateTitle();
        UIManager::showToast("Guardado correctamente");
    } else {
        UIManager::showToast("Error al guardar archivo");
    }

    lv_obj_delete(ctx->mask);
    delete ctx;
}

void TextEditorView::modal_save_cancel_cb(lv_event_t* e) {
    SaveAsContext* ctx = (SaveAsContext*)lv_event_get_user_data(e);
    if (ctx) {
        lv_obj_delete(ctx->mask);
        delete ctx;
    }
}

void TextEditorView::modal_storage_toggle_cb(lv_event_t* e) {
    SaveAsContext* ctx = (SaveAsContext*)lv_event_get_user_data(e);
    if (!ctx) return;

    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
    if (ctx->targetStorage == StorageType::SD_CARD) {
        ctx->targetStorage = StorageType::FLASH_FS;
        lv_obj_t* lbl = lv_obj_get_child(btn, 0);
        if (lbl) lv_label_set_text(lbl, "Unidad: Flash");
    } else {
        ctx->targetStorage = StorageType::SD_CARD;
        lv_obj_t* lbl = lv_obj_get_child(btn, 0);
        if (lbl) lv_label_set_text(lbl, "Unidad: MicroSD");
    }
}

void TextEditorView::showSaveAsModal() {
    lv_obj_t* activeScr = lv_screen_active();
    if (!activeScr) return;

    SaveAsContext* ctx = new SaveAsContext();
    ctx->targetStorage = currentStorage;

    // Máscara oscura
    ctx->mask = lv_obj_create(activeScr);
    lv_obj_set_size(ctx->mask, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(ctx->mask, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(ctx->mask, LV_OPA_70, 0);
    lv_obj_center(ctx->mask);
    lv_obj_add_flag(ctx->mask, LV_OBJ_FLAG_CLICKABLE);

    // Contenedor modal
    lv_obj_t* modal = lv_obj_create(ctx->mask);
    lv_obj_set_size(modal, 290, 420);
    DefaultTheme::applyRaisedCard(modal, 16);
    lv_obj_center(modal);
    lv_obj_set_flex_flow(modal, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(modal, 12, 0);
    lv_obj_set_style_pad_row(modal, 8, 0);

    // Título
    lv_obj_t* title = lv_label_create(modal);
    lv_label_set_text(title, "Guardar Archivo Como...");
    lv_obj_set_style_text_color(title, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);

    // Botón selector de unidad
    lv_obj_t* btnUnit = lv_button_create(modal);
    lv_obj_set_size(btnUnit, lv_pct(100), 32);
    DefaultTheme::applyButton(btnUnit, 8);
    lv_obj_t* lblUnit = lv_label_create(btnUnit);
    lv_label_set_text(lblUnit, (ctx->targetStorage == StorageType::SD_CARD) ? "Unidad: MicroSD" : "Unidad: Flash");
    lv_obj_center(lblUnit);
    lv_obj_add_event_cb(btnUnit, modal_storage_toggle_cb, LV_EVENT_CLICKED, ctx);

    // Campo de texto de ruta/nombre
    ctx->taPath = lv_textarea_create(modal);
    lv_obj_set_size(ctx->taPath, lv_pct(100), 36);
    DefaultTheme::applySunkenCard(ctx->taPath, 8);
    lv_textarea_set_one_line(ctx->taPath, true);
    std::string defaultPath = currentFilePath.empty() ? "/scripts/lua/nuevo.lua" : currentFilePath;
    lv_textarea_set_text(ctx->taPath, defaultPath.c_str());
    lv_obj_set_style_text_font(ctx->taPath, &lv_font_montserrat_12, 0);

    // Teclado modal para ingresar ruta
    lv_obj_t* kb = lv_keyboard_create(modal);
    lv_obj_set_size(kb, lv_pct(100), 220);
    lv_keyboard_set_textarea(kb, ctx->taPath);

    // Botones Cancelar / Guardar
    lv_obj_t* btnRow = lv_obj_create(modal);
    lv_obj_set_size(btnRow, lv_pct(100), 40);
    lv_obj_set_style_bg_opa(btnRow, 0, 0);
    lv_obj_set_style_border_width(btnRow, 0, 0);
    lv_obj_set_flex_flow(btnRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btnRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(btnRow, 0, 0);

    lv_obj_t* btnCancel = lv_button_create(btnRow);
    lv_obj_set_size(btnCancel, 120, 36);
    DefaultTheme::applyButton(btnCancel, 8);
    lv_obj_t* lblC = lv_label_create(btnCancel);
    lv_label_set_text(lblC, "Cancelar");
    lv_obj_center(lblC);
    lv_obj_add_event_cb(btnCancel, modal_save_cancel_cb, LV_EVENT_CLICKED, ctx);

    lv_obj_t* btnSave = lv_button_create(btnRow);
    lv_obj_set_size(btnSave, 120, 36);
    DefaultTheme::applyButton(btnSave, 8);
    lv_obj_set_style_bg_color(btnSave, lv_color_hex(0x1B5E20), 0);
    lv_obj_t* lblS = lv_label_create(btnSave);
    lv_label_set_text(lblS, "Guardar");
    lv_obj_set_style_text_color(lblS, lv_color_hex(0x00E676), 0);
    lv_obj_center(lblS);
    lv_obj_add_event_cb(btnSave, modal_save_confirm_cb, LV_EVENT_CLICKED, ctx);
}

// ── Modal: Abrir Archivo ──────────────────────────────────────────
struct OpenFileContext {
    lv_obj_t* mask;
    StorageType targetStorage;
};

void TextEditorView::modal_open_close_cb(lv_event_t* e) {
    lv_obj_t* mask = (lv_obj_t*)lv_event_get_user_data(e);
    if (mask && lv_obj_is_valid(mask)) {
        lv_obj_delete(mask);
    }
}

void TextEditorView::modal_file_item_cb(lv_event_t* e) {
    char* path = (char*)lv_event_get_user_data(e);
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
    lv_obj_t* mask = (lv_obj_t*)lv_obj_get_user_data(btn);

    if (path) {
        loadFileContent(path, StorageType::SD_CARD);
        free(path);
    }

    if (mask && lv_obj_is_valid(mask)) {
        lv_obj_delete(mask);
    }
}

void TextEditorView::showOpenFileModal() {
    lv_obj_t* activeScr = lv_screen_active();
    if (!activeScr) return;

    // Máscara
    lv_obj_t* mask = lv_obj_create(activeScr);
    lv_obj_set_size(mask, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(mask, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(mask, LV_OPA_70, 0);
    lv_obj_center(mask);
    lv_obj_add_flag(mask, LV_OBJ_FLAG_CLICKABLE);

    // Contenedor modal
    lv_obj_t* modal = lv_obj_create(mask);
    lv_obj_set_size(modal, 290, 380);
    DefaultTheme::applyRaisedCard(modal, 16);
    lv_obj_center(modal);
    lv_obj_set_flex_flow(modal, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(modal, 12, 0);
    lv_obj_set_style_pad_row(modal, 8, 0);

    // Header del modal
    lv_obj_t* header = lv_obj_create(modal);
    lv_obj_set_size(header, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(header, 0, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(header, 0, 0);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, "Abrir Archivo de Texto / Script");
    lv_obj_set_style_text_color(title, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);

    lv_obj_t* btnClose = lv_button_create(header);
    lv_obj_set_size(btnClose, 28, 28);
    DefaultTheme::applyButton(btnClose, 14);
    lv_obj_t* lblX = lv_label_create(btnClose);
    lv_label_set_text(lblX, LV_SYMBOL_CLOSE);
    lv_obj_center(lblX);
    lv_obj_add_event_cb(btnClose, modal_open_close_cb, LV_EVENT_CLICKED, mask);

    // Lista de archivos
    lv_obj_t* list = lv_obj_create(modal);
    lv_obj_set_size(list, lv_pct(100), lv_pct(82));
    DefaultTheme::applySunkenCard(list, 10);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(list, 6, 0);
    lv_obj_set_style_pad_row(list, 4, 0);

    // Escanear SD en busca de archivos editables
    std::vector<StorageFileInfo> textFiles;
    std::vector<std::string> dirsToScan = {"/"};

    for (size_t d = 0; d < dirsToScan.size() && dirsToScan.size() < 30; d++) {
        auto entries = StorageManager::listDirectory(StorageType::SD_CARD, dirsToScan[d]);
        for (const auto& f : entries) {
            if (f.isDirectory) {
                dirsToScan.push_back(f.path);
            } else {
                std::string nameLower = f.name;
                std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                size_t dotPos = nameLower.rfind('.');
                if (dotPos != std::string::npos) {
                    std::string ext = nameLower.substr(dotPos);
                    if (ext == ".lua" || ext == ".txt" || ext == ".json" || ext == ".ini" || ext == ".log" || ext == ".csv") {
                        textFiles.push_back(f);
                    }
                }
            }
        }
    }

    if (textFiles.empty()) {
        lv_obj_t* emptyLbl = lv_label_create(list);
        lv_label_set_text(emptyLbl, "No se encontraron archivos de texto\no scripts en la SD.");
        lv_obj_set_style_text_color(emptyLbl, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_align(emptyLbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(emptyLbl);
    } else {
        for (const auto& f : textFiles) {
            lv_obj_t* item = lv_button_create(list);
            lv_obj_set_size(item, lv_pct(100), 38);
            DefaultTheme::applyButton(item, 8);
            lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(item, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_all(item, 6, 0);

            lv_obj_t* icon = lv_label_create(item);
            lv_label_set_text(icon, LV_SYMBOL_FILE);
            lv_obj_set_style_text_color(icon, DefaultTheme::getPrimaryAccent(), 0);

            lv_obj_t* name = lv_label_create(item);
            lv_label_set_text(name, f.path.c_str());
            lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
            lv_obj_set_width(name, 210);
            lv_obj_set_style_text_color(name, DefaultTheme::getTextColor(), 0);
            lv_obj_set_style_text_font(name, &lv_font_montserrat_12, 0);
            lv_obj_set_style_margin_left(name, 6, 0);

            char* pathAlloc = strdup(f.path.c_str());
            lv_obj_set_user_data(item, mask);
            lv_obj_add_event_cb(item, modal_file_item_cb, LV_EVENT_CLICKED, (void*)pathAlloc);
        }
    }
}

// ── Creación de la pantalla del Editor ─────────────────────────────
lv_obj_t* TextEditorView::create(const std::string& initialPath, StorageType storage) {
    currentFilePath = initialPath;
    currentStorage = storage;
    isModified = false;
    keyboardVisible = true;

    lv_obj_t* screen = lv_obj_create(NULL);
    DefaultTheme::applyFlatBg(screen);
    DefaultTheme::disableScroll(screen);
    WallpaperManager::getInstance().applyWallpaper(screen);

    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(screen, 6, 0);
    lv_obj_set_style_pad_row(screen, 4, 0);

    // 1. HeaderBar
    HeaderBar::create(screen, "Editor", true, true);

    // 2. Info Bar (Ruta del archivo actual)
    lv_obj_t* infoBar = lv_obj_create(screen);
    lv_obj_set_size(infoBar, lv_pct(100), 28);
    DefaultTheme::applySunkenCard(infoBar, 6);
    lv_obj_set_flex_flow(infoBar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(infoBar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(infoBar, 8, 0);
    lv_obj_set_style_pad_ver(infoBar, 2, 0);

    fileLabel = lv_label_create(infoBar);
    lv_label_set_long_mode(fileLabel, LV_LABEL_LONG_DOT);
    lv_obj_set_width(fileLabel, lv_pct(100));
    lv_obj_set_style_text_color(fileLabel, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(fileLabel, &lv_font_montserrat_12, 0);

    // 3. Toolbar de botones de acción
    lv_obj_t* toolbar = lv_obj_create(screen);
    lv_obj_set_size(toolbar, lv_pct(100), 38);
    lv_obj_set_style_bg_opa(toolbar, 0, 0);
    lv_obj_set_style_border_width(toolbar, 0, 0);
    lv_obj_set_flex_flow(toolbar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(toolbar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(toolbar, 0, 0);

    // Botón Guardar (Save)
    lv_obj_t* btnSave = lv_button_create(toolbar);
    lv_obj_set_size(btnSave, 54, 34);
    DefaultTheme::applyButton(btnSave, 6);
    lv_obj_t* lblSave = lv_label_create(btnSave);
    lv_label_set_text(lblSave, LV_SYMBOL_SAVE);
    lv_obj_set_style_text_color(lblSave, lv_color_hex(0x00E676), 0);
    lv_obj_center(lblSave);
    lv_obj_add_event_cb(btnSave, btn_save_cb, LV_EVENT_CLICKED, NULL);

    // Botón Guardar Como (Save As)
    lv_obj_t* btnSaveAs = lv_button_create(toolbar);
    lv_obj_set_size(btnSaveAs, 54, 34);
    DefaultTheme::applyButton(btnSaveAs, 6);
    lv_obj_t* lblSaveAs = lv_label_create(btnSaveAs);
    lv_label_set_text(lblSaveAs, LV_SYMBOL_SAVE "...");
    lv_obj_set_style_text_font(lblSaveAs, &lv_font_montserrat_12, 0);
    lv_obj_center(lblSaveAs);
    lv_obj_add_event_cb(btnSaveAs, btn_save_as_cb, LV_EVENT_CLICKED, NULL);

    // Botón Abrir (Open)
    lv_obj_t* btnOpen = lv_button_create(toolbar);
    lv_obj_set_size(btnOpen, 54, 34);
    DefaultTheme::applyButton(btnOpen, 6);
    lv_obj_t* lblOpen = lv_label_create(btnOpen);
    lv_label_set_text(lblOpen, LV_SYMBOL_DIRECTORY);
    lv_obj_center(lblOpen);
    lv_obj_add_event_cb(btnOpen, btn_open_cb, LV_EVENT_CLICKED, NULL);

    // Botón Nuevo (New)
    lv_obj_t* btnNew = lv_button_create(toolbar);
    lv_obj_set_size(btnNew, 48, 34);
    DefaultTheme::applyButton(btnNew, 6);
    lv_obj_t* lblNew = lv_label_create(btnNew);
    lv_label_set_text(lblNew, LV_SYMBOL_PLUS);
    lv_obj_center(lblNew);
    lv_obj_add_event_cb(btnNew, btn_new_cb, LV_EVENT_CLICKED, NULL);

    // Botón Toggle Teclado
    btnKb = lv_button_create(toolbar);
    lv_obj_set_size(btnKb, 48, 34);
    DefaultTheme::applyButton(btnKb, 6);
    lv_obj_t* lblKb = lv_label_create(btnKb);
    lv_label_set_text(lblKb, LV_SYMBOL_KEYBOARD);
    lv_obj_center(lblKb);
    lv_obj_add_event_cb(btnKb, btn_toggle_kb_cb, LV_EVENT_CLICKED, NULL);

    // Botón Ejecutar Script (Run) - sólo si es .lua
    btnRun = lv_button_create(toolbar);
    lv_obj_set_size(btnRun, 54, 34);
    DefaultTheme::applyButton(btnRun, 6);
    lv_obj_set_style_bg_color(btnRun, lv_color_hex(0x1B5E20), 0);
    lv_obj_t* lblRun = lv_label_create(btnRun);
    lv_label_set_text(lblRun, LV_SYMBOL_PLAY " Run");
    lv_obj_set_style_text_color(lblRun, lv_color_hex(0x00E676), 0);
    lv_obj_set_style_text_font(lblRun, &lv_font_montserrat_12, 0);
    lv_obj_center(lblRun);
    lv_obj_add_event_cb(btnRun, btn_run_cb, LV_EVENT_CLICKED, NULL);

    // 4. Área de Texto Principal (Textarea)
    textArea = lv_textarea_create(screen);
    lv_obj_set_width(textArea, lv_pct(100));
    lv_obj_set_flex_grow(textArea, 1);
    DefaultTheme::applySunkenCard(textArea, 10);
    lv_obj_set_style_bg_color(textArea, lv_color_hex(0x0E1217), 0);
    lv_obj_set_style_text_color(textArea, lv_color_hex(0xE0E6ED), 0);
    lv_obj_set_style_text_font(textArea, &lv_font_montserrat_12, 0);
    lv_textarea_set_one_line(textArea, false);
    lv_textarea_set_cursor_click_pos(textArea, true);
    lv_obj_add_event_cb(textArea, ta_event_cb, LV_EVENT_ALL, NULL);

    // 5. Teclado Virtual Acoplado
    keyboard = lv_keyboard_create(screen);
    lv_obj_set_size(keyboard, lv_pct(100), 190);
    lv_keyboard_set_textarea(keyboard, textArea);

    // 6. Cargar contenido inicial si se proporcionó una ruta
    if (!initialPath.empty()) {
        loadFileContent(initialPath, storage);
    } else {
        updateTitle();
    }

    lv_obj_add_event_cb(screen, screen_delete_cb, LV_EVENT_DELETE, NULL);
    return screen;
}
