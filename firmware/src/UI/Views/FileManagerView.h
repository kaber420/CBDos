#pragma once
#include <lvgl.h>
#include <string>
#include <vector>
#include "../Components/HeaderBar.h"
#include "../../Core/StorageManager.h"

class FileManagerView {
public:
    static lv_obj_t* create();
    static void refreshCurrentView();

private:
    static HeaderBar* headerBar;
    static StorageType currentStorage;
    static std::string currentPath;

    // Elementos de UI
    static lv_obj_t* unitInfoLabel;
    static lv_obj_t* pathLabel;
    static lv_obj_t* btnUnitSD;
    static lv_obj_t* btnUnitFlash;
    static lv_obj_t* listContainer;

    // Métodos internos
    static void renderUnitSelector(lv_obj_t* parent);
    static void renderPathBar(lv_obj_t* parent);
    static void renderFileList(lv_obj_t* parent);
    static void updateUnitButtons();
    static void updatePathLabel();
    static void updateStorageInfo();

    static StorageType getCurrentStorage() { return currentStorage; }

    // Modales
    static void showDeleteConfirmModal(const StorageFileInfo& file);
    static void showTextPreviewModal(const std::string& fileName, const std::string& content);

    // Callbacks
    static void modal_cancel_cb(lv_event_t* e);
    static void modal_confirm_delete_cb(lv_event_t* e);
    static void modal_close_preview_cb(lv_event_t* e);
    static void unit_sd_click_cb(lv_event_t* e);
    static void unit_flash_click_cb(lv_event_t* e);
    static void btn_up_click_cb(lv_event_t* e);
    static void btn_refresh_click_cb(lv_event_t* e);
    static void item_click_cb(lv_event_t* e);
    static void item_delete_click_cb(lv_event_t* e);
    static void screen_delete_cb(lv_event_t* e);
};
