#include "LuaRunnerView.h"
#include "../UIManager.h"
#include "../Components/HeaderBar.h"
#include "../Themes/DefaultTheme.h"
#include "../WallpaperManager.h"
#include "Core/LuaRunner.h"
#include "Core/StorageManager.h"
#include <algorithm>

lv_obj_t* LuaRunnerView::logContainer = nullptr;
lv_obj_t* LuaRunnerView::statusBadge = nullptr;
lv_obj_t* LuaRunnerView::scriptLabel = nullptr;
lv_timer_t* LuaRunnerView::refreshTimer = nullptr;
std::string LuaRunnerView::activeScript = "";

void LuaRunnerView::setInitialScript(const std::string& path) {
    activeScript = path;
}

void LuaRunnerView::updateStatusBadge() {
    if (!statusBadge || !lv_obj_is_valid(statusBadge)) return;

    LuaRunnerState state = LuaRunner::getInstance().getState();
    const char* text = LuaRunner::getInstance().getStateString();
    uint32_t color = 0x90A4AE; // Gris

    if (state == LuaRunnerState::RUNNING) {
        color = 0x00E676; // Verde
    } else if (state == LuaRunnerState::FINISHED) {
        color = 0x40C4FF; // Azul
    } else if (state == LuaRunnerState::ERROR) {
        color = 0xFF5252; // Rojo
    } else if (state == LuaRunnerState::STOPPED) {
        color = 0xFFB74D; // Ámbar
    }

    lv_label_set_text(statusBadge, text);
    lv_obj_set_style_text_color(statusBadge, lv_color_hex(color), 0);
}

void LuaRunnerView::appendLogLine(const std::string& line) {
    if (!logContainer || !lv_obj_is_valid(logContainer)) return;

    lv_obj_t* lbl = lv_label_create(logContainer);
    lv_label_set_text(lbl, line.c_str());
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl, lv_pct(100));
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);

    // Colorear según el tipo de log
    if (line.rfind("[Error]", 0) == 0) {
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xFF5252), 0);
    } else if (line.rfind("[Sistema]", 0) == 0) {
        lv_obj_set_style_text_color(lbl, lv_color_hex(0x40C4FF), 0);
    } else if (line.rfind("[OK]", 0) == 0) {
        lv_obj_set_style_text_color(lbl, lv_color_hex(0x00E676), 0);
    } else {
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xE0E6ED), 0);
    }

    // Auto-scroll al fondo
    lv_obj_scroll_to_y(logContainer, LV_COORD_MAX, LV_ANIM_OFF);
}

void LuaRunnerView::timer_cb(lv_timer_t* timer) {
    std::vector<std::string> logs;
    if (LuaRunner::getInstance().drainLogs(logs)) {
        for (const auto& l : logs) {
            appendLogLine(l);
        }
    }
    updateStatusBadge();
}

void LuaRunnerView::screen_delete_cb(lv_event_t* e) {
    if (refreshTimer) {
        lv_timer_delete(refreshTimer);
        refreshTimer = nullptr;
    }
    logContainer = nullptr;
    statusBadge = nullptr;
    scriptLabel = nullptr;
}

void LuaRunnerView::btn_run_cb(lv_event_t* e) {
    if (activeScript.empty()) {
        UIManager::showToast("Selecciona un script primero (📁)");
        return;
    }

    if (LuaRunner::getInstance().getState() == LuaRunnerState::RUNNING) {
        UIManager::showToast("El script ya está corriendo");
        return;
    }

    LuaRunner::getInstance().startScript(activeScript);
    updateStatusBadge();
}

void LuaRunnerView::btn_stop_cb(lv_event_t* e) {
    if (LuaRunner::getInstance().getState() == LuaRunnerState::RUNNING) {
        LuaRunner::getInstance().stop();
        UIManager::showToast("Deteniendo script...");
    } else {
        UIManager::showToast("No hay script corriendo");
    }
    updateStatusBadge();
}

void LuaRunnerView::btn_edit_cb(lv_event_t* e) {
    if (activeScript.empty()) {
        UIManager::getInstance().loadTextEditor();
    } else {
        UIManager::getInstance().loadTextEditor(activeScript, StorageType::SD_CARD);
    }
}

void LuaRunnerView::btn_clear_cb(lv_event_t* e) {
    if (logContainer && lv_obj_is_valid(logContainer)) {
        lv_obj_clean(logContainer);
    }
    LuaRunner::getInstance().clearLogs();
}

void LuaRunnerView::modal_close_cb(lv_event_t* e) {
    lv_obj_t* mask = (lv_obj_t*)lv_event_get_user_data(e);
    if (mask && lv_obj_is_valid(mask)) {
        lv_obj_delete_async(mask);
    }
}

void LuaRunnerView::file_select_cb(lv_event_t* e) {
    const char* path = (const char*)lv_event_get_user_data(e);
    if (path) {
        activeScript = path;
        if (scriptLabel && lv_obj_is_valid(scriptLabel)) {
            lv_label_set_text(scriptLabel, activeScript.c_str());
        }
        UIManager::showToast("Script seleccionado");
    }
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
    lv_obj_t* mask = (lv_obj_t*)lv_obj_get_user_data(btn);
    if (mask && lv_obj_is_valid(mask)) {
        lv_obj_delete_async(mask);
    }
}

void LuaRunnerView::showFilePickerModal() {
    lv_obj_t* mask = lv_obj_create(lv_layer_top());
    lv_obj_set_size(mask, 320, 480);
    lv_obj_set_pos(mask, 0, 0);
    lv_obj_set_style_bg_color(mask, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(mask, LV_OPA_80, 0);
    lv_obj_set_style_border_width(mask, 0, 0);
    lv_obj_set_style_pad_all(mask, 0, 0);

    lv_obj_t* modal = lv_obj_create(mask);
    lv_obj_set_size(modal, 290, 380);
    DefaultTheme::applyRaisedCard(modal, 16);
    lv_obj_center(modal);
    lv_obj_set_flex_flow(modal, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(modal, 12, 0);
    lv_obj_set_style_pad_row(modal, 8, 0);

    // Título Modal
    lv_obj_t* header = lv_obj_create(modal);
    lv_obj_set_size(header, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(header, 0, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(header, 0, 0);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, "Scripts en SD (.lua)");
    lv_obj_set_style_text_color(title, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);

    lv_obj_t* btnClose = lv_button_create(header);
    lv_obj_set_size(btnClose, 30, 30);
    DefaultTheme::applyButton(btnClose, 15);
    lv_obj_t* lblX = lv_label_create(btnClose);
    lv_label_set_text(lblX, LV_SYMBOL_CLOSE);
    lv_obj_center(lblX);
    lv_obj_add_event_cb(btnClose, modal_close_cb, LV_EVENT_CLICKED, mask);

    // Lista de archivos
    lv_obj_t* list = lv_obj_create(modal);
    lv_obj_set_size(list, lv_pct(100), lv_pct(80));
    DefaultTheme::applySunkenCard(list, 10);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(list, 6, 0);
    lv_obj_set_style_pad_row(list, 4, 0);

    // Escaneo recursivo de archivos .lua en la tarjeta SD (hasta 30 carpetas)
    std::vector<StorageFileInfo> luaFiles;
    std::vector<std::string> dirsToScan = {"/"};

    for (size_t d = 0; d < dirsToScan.size() && dirsToScan.size() < 30; d++) {
        auto entries = StorageManager::listDirectory(StorageType::SD_CARD, dirsToScan[d]);
        for (const auto& f : entries) {
            if (f.isDirectory) {
                dirsToScan.push_back(f.path);
            } else {
                std::string nameLower = f.name;
                std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                if (nameLower.size() >= 4 && nameLower.rfind(".lua") == nameLower.size() - 4) {
                    luaFiles.push_back(f);
                }
            }
        }
    }

    if (luaFiles.empty()) {
        lv_obj_t* emptyLbl = lv_label_create(list);
        lv_label_set_text(emptyLbl, "No se encontraron archivos .lua\nen ninguna carpeta de la SD.");
        lv_obj_set_style_text_color(emptyLbl, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_align(emptyLbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(emptyLbl);
    } else {
        for (const auto& f : luaFiles) {
            lv_obj_t* item = lv_button_create(list);
            lv_obj_set_size(item, lv_pct(100), 40);
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

            // Almacenar path estático copiado
            char* pathAlloc = strdup(f.path.c_str());
            lv_obj_set_user_data(item, mask);
            lv_obj_add_event_cb(item, file_select_cb, LV_EVENT_CLICKED, (void*)pathAlloc);
        }
    }
}

void LuaRunnerView::btn_sd_cb(lv_event_t* e) {
    showFilePickerModal();
}

lv_obj_t* LuaRunnerView::create(const std::string& initialScript) {
    if (!initialScript.empty()) {
        activeScript = initialScript;
    }

    lv_obj_t* screen = lv_obj_create(NULL);
    DefaultTheme::applyFlatBg(screen);
    DefaultTheme::disableScroll(screen);
    WallpaperManager::getInstance().applyWallpaper(screen);

    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(screen, 8, 0);
    lv_obj_set_style_pad_row(screen, 6, 0);

    // 1. HeaderBar
    HeaderBar::create(screen, "Lua Runner", true, true);

    // 2. Info Bar (Script activo + Badge de Estado)
    lv_obj_t* infoBar = lv_obj_create(screen);
    lv_obj_set_size(infoBar, lv_pct(100), 32);
    DefaultTheme::applySunkenCard(infoBar, 8);
    lv_obj_set_flex_flow(infoBar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(infoBar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(infoBar, 10, 0);
    lv_obj_set_style_pad_ver(infoBar, 2, 0);

    scriptLabel = lv_label_create(infoBar);
    lv_label_set_text(scriptLabel, activeScript.empty() ? "(Sin script seleccionado)" : activeScript.c_str());
    lv_label_set_long_mode(scriptLabel, LV_LABEL_LONG_DOT);
    lv_obj_set_width(scriptLabel, 200);
    lv_obj_set_style_text_color(scriptLabel, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(scriptLabel, &lv_font_montserrat_12, 0);

    statusBadge = lv_label_create(infoBar);
    lv_label_set_text(statusBadge, "IDLE");
    lv_obj_set_style_text_color(statusBadge, lv_color_hex(0x90A4AE), 0);
    lv_obj_set_style_text_font(statusBadge, &lv_font_montserrat_12, 0);

    // 3. Consola de Logs (Centro)
    logContainer = lv_obj_create(screen);
    lv_obj_set_width(logContainer, lv_pct(100));
    lv_obj_set_flex_grow(logContainer, 1);
    DefaultTheme::applySunkenCard(logContainer, 12);
    lv_obj_set_style_bg_color(logContainer, lv_color_hex(0x0E1217), 0);
    lv_obj_set_flex_flow(logContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(logContainer, 8, 0);
    lv_obj_set_style_pad_row(logContainer, 4, 0);
    lv_obj_set_scroll_dir(logContainer, LV_DIR_VER);

    // Mensaje de bienvenida inicial
    appendLogLine("[Sistema] Consola de Lua 5.4 lista.");

    // 4. Barra de Acciones Inferior (Dock de botones táctiles)
    lv_obj_t* dock = lv_obj_create(screen);
    lv_obj_set_size(dock, lv_pct(100), 52);
    lv_obj_set_style_bg_opa(dock, 0, 0);
    lv_obj_set_style_border_width(dock, 0, 0);
    lv_obj_set_flex_flow(dock, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dock, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(dock, 0, 0);

    // Botón Run
    lv_obj_t* btnRun = lv_button_create(dock);
    lv_obj_set_size(btnRun, 54, 44);
    DefaultTheme::applyButton(btnRun, 8);
    lv_obj_set_style_bg_color(btnRun, lv_color_hex(0x1B5E20), 0); // Verde oscuro
    lv_obj_t* lblRun = lv_label_create(btnRun);
    lv_label_set_text(lblRun, LV_SYMBOL_PLAY);
    lv_obj_set_style_text_color(lblRun, lv_color_hex(0x00E676), 0);
    lv_obj_center(lblRun);
    lv_obj_add_event_cb(btnRun, btn_run_cb, LV_EVENT_CLICKED, NULL);

    // Botón Stop
    lv_obj_t* btnStop = lv_button_create(dock);
    lv_obj_set_size(btnStop, 54, 44);
    DefaultTheme::applyButton(btnStop, 8);
    lv_obj_set_style_bg_color(btnStop, lv_color_hex(0xB71C1C), 0); // Rojo oscuro
    lv_obj_t* lblStop = lv_label_create(btnStop);
    lv_label_set_text(lblStop, LV_SYMBOL_STOP);
    lv_obj_set_style_text_color(lblStop, lv_color_hex(0xFF5252), 0);
    lv_obj_center(lblStop);
    lv_obj_add_event_cb(btnStop, btn_stop_cb, LV_EVENT_CLICKED, NULL);

    // Botón Editar
    lv_obj_t* btnEdit = lv_button_create(dock);
    lv_obj_set_size(btnEdit, 54, 44);
    DefaultTheme::applyButton(btnEdit, 8);
    lv_obj_t* lblEdit = lv_label_create(btnEdit);
    lv_label_set_text(lblEdit, LV_SYMBOL_EDIT);
    lv_obj_set_style_text_color(lblEdit, lv_color_hex(0xFFB800), 0);
    lv_obj_center(lblEdit);
    lv_obj_add_event_cb(btnEdit, btn_edit_cb, LV_EVENT_CLICKED, NULL);

    // Botón Cargar SD
    lv_obj_t* btnSd = lv_button_create(dock);
    lv_obj_set_size(btnSd, 54, 44);
    DefaultTheme::applyButton(btnSd, 8);
    lv_obj_t* lblSd = lv_label_create(btnSd);
    lv_label_set_text(lblSd, LV_SYMBOL_DIRECTORY);
    lv_obj_set_style_text_color(lblSd, DefaultTheme::getTextColor(), 0);
    lv_obj_center(lblSd);
    lv_obj_add_event_cb(btnSd, btn_sd_cb, LV_EVENT_CLICKED, NULL);

    // Botón Limpiar
    lv_obj_t* btnClear = lv_button_create(dock);
    lv_obj_set_size(btnClear, 54, 44);
    DefaultTheme::applyButton(btnClear, 8);
    lv_obj_t* lblClear = lv_label_create(btnClear);
    lv_label_set_text(lblClear, LV_SYMBOL_TRASH);
    lv_obj_set_style_text_color(lblClear, DefaultTheme::getTextColor(), 0);
    lv_obj_center(lblClear);
    lv_obj_add_event_cb(btnClear, btn_clear_cb, LV_EVENT_CLICKED, NULL);

    // 5. Timer de refresco de logs y estado cada 50ms
    refreshTimer = lv_timer_create(timer_cb, 50, screen);

    lv_obj_add_event_cb(screen, screen_delete_cb, LV_EVENT_DELETE, NULL);

    updateStatusBadge();
    return screen;
}
