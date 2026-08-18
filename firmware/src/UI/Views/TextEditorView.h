#pragma once

#include <lvgl.h>
#include <string>
#include "../../Core/StorageManager.h"

class TextEditorView {
public:
    static lv_obj_t* create(const std::string& initialPath = "", StorageType storage = StorageType::SD_CARD);

private:
    static void screen_delete_cb(lv_event_t* e);
    static void ta_event_cb(lv_event_t* e);
    static void btn_save_cb(lv_event_t* e);
    static void btn_save_as_cb(lv_event_t* e);
    static void btn_new_cb(lv_event_t* e);
    static void btn_open_cb(lv_event_t* e);
    static void btn_toggle_kb_cb(lv_event_t* e);
    static void editor_kb_event_cb(lv_event_t* e);
    static void btn_run_cb(lv_event_t* e);

    // Modales y herramientas
    static void showSaveAsModal();
    static void showOpenFileModal();
    static void modal_save_confirm_cb(lv_event_t* e);
    static void modal_save_cancel_cb(lv_event_t* e);
    static void modal_storage_toggle_cb(lv_event_t* e);
    static void modal_open_close_cb(lv_event_t* e);
    static void modal_file_item_cb(lv_event_t* e);
    static void updateTitle();
    static void updateRunButtonVisibility();
    static void loadFileContent(const std::string& path, StorageType storage);

    static lv_obj_t* textArea;
    static lv_obj_t* keyboard;
    static lv_obj_t* fileLabel;
    static lv_obj_t* btnRun;
    static lv_obj_t* btnKb;
    static std::string currentFilePath;
    static StorageType currentStorage;
    static bool isModified;
    static bool keyboardVisible;
};
