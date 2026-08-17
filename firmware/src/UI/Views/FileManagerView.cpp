#include "FileManagerView.h"
#include "../Themes/DefaultTheme.h"
#include "../WallpaperManager.h"
#include "../UIManager.h"
#include <algorithm>
#include <cstdio>

HeaderBar*   FileManagerView::headerBar         = nullptr;
StorageType  FileManagerView::currentStorage    = StorageType::SD_CARD;
std::string  FileManagerView::currentPath       = "/";

lv_obj_t*    FileManagerView::unitInfoLabel     = nullptr;
lv_obj_t*    FileManagerView::pathLabel         = nullptr;
lv_obj_t*    FileManagerView::btnUnitSD         = nullptr;
lv_obj_t*    FileManagerView::btnUnitFlash      = nullptr;
lv_obj_t*    FileManagerView::listContainer     = nullptr;

// Helper para obtener icono y color según extensión
static void getFileIconAndColor(const std::string& name, bool isDir, const char** outIcon, uint32_t* outColor) {
    if (isDir) {
        *outIcon = LV_SYMBOL_DIRECTORY;
        *outColor = 0xFFB800; // Amarillo / Ámbar
        return;
    }

    std::string ext = "";
    size_t dotPos = name.rfind('.');
    if (dotPos != std::string::npos) {
        ext = name.substr(dotPos);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    }

    if (ext == ".mp3" || ext == ".wav") {
        *outIcon = LV_SYMBOL_AUDIO;
        *outColor = 0x70E000; // Verde Lima
    } else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp") {
        *outIcon = LV_SYMBOL_IMAGE;
        *outColor = 0xFF2E93; // Rosa / Fucsia
    } else if (ext == ".gbc" || ext == ".gb" || ext == ".wad") {
        *outIcon = LV_SYMBOL_PLAY;
        *outColor = 0xE50000; // Rojo
    } else if (ext == ".txt" || ext == ".json" || ext == ".enc" || ext == ".log" || ext == ".csv") {
        *outIcon = LV_SYMBOL_FILE;
        *outColor = 0x00F5D4; // Turquesa
    } else {
        *outIcon = LV_SYMBOL_FILE;
        *outColor = 0x9D4EDD; // Violeta
    }
}

void FileManagerView::screen_delete_cb(lv_event_t* e) {
    headerBar = nullptr;
    unitInfoLabel = nullptr;
    pathLabel = nullptr;
    btnUnitSD = nullptr;
    btnUnitFlash = nullptr;
    listContainer = nullptr;
}

lv_obj_t* FileManagerView::create() {
    lv_obj_t* scr = lv_obj_create(NULL);
    DefaultTheme::applyFlatBg(scr);
    DefaultTheme::disableScroll(scr);

    WallpaperManager::getInstance().applyWallpaper(scr);

    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(scr, 8, 0);
    lv_obj_set_style_pad_row(scr, 6, 0);

    // Cabecera superior
    HeaderBarConfig cfg;
    cfg.title = "Explorador";
    cfg.showBackButton = true;
    cfg.showTime = true;
    cfg.showWifi = false;
    cfg.showCartButton = false;
    headerBar = HeaderBar::create(scr, cfg);
    HeaderBar::setActiveHeader(headerBar);

    // Selector de unidad (SD / Flash)
    renderUnitSelector(scr);

    // Barra de ruta (Breadcrumb y navegación de subida)
    renderPathBar(scr);

    // Contenedor scrollable de la lista de archivos
    listContainer = lv_obj_create(scr);
    lv_obj_set_width(listContainer, lv_pct(100));
    lv_obj_set_flex_grow(listContainer, 1);
    DefaultTheme::applyRaisedCard(listContainer, 12);
    lv_obj_set_flex_flow(listContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(listContainer, 6, 0);
    lv_obj_set_style_pad_row(listContainer, 6, 0);
    lv_obj_add_flag(listContainer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(listContainer, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(listContainer, LV_SCROLLBAR_MODE_ACTIVE);

    renderFileList(listContainer);
    updateUnitButtons();
    updateStorageInfo();

    lv_obj_add_event_cb(scr, screen_delete_cb, LV_EVENT_DELETE, NULL);

    return scr;
}

void FileManagerView::renderUnitSelector(lv_obj_t* parent) {
    lv_obj_t* unitCard = lv_obj_create(parent);
    lv_obj_set_width(unitCard, lv_pct(100));
    lv_obj_set_height(unitCard, LV_SIZE_CONTENT);
    DefaultTheme::applySunkenCard(unitCard, 10);
    DefaultTheme::disableScroll(unitCard);
    lv_obj_set_flex_flow(unitCard, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(unitCard, 6, 0);
    lv_obj_set_style_pad_row(unitCard, 4, 0);

    // Fila de botones de unidad
    lv_obj_t* btnRow = lv_obj_create(unitCard);
    lv_obj_set_width(btnRow, lv_pct(100));
    lv_obj_set_height(btnRow, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(btnRow, 0, 0);
    lv_obj_set_style_border_width(btnRow, 0, 0);
    lv_obj_set_flex_flow(btnRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btnRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(btnRow, 0, 0);
    DefaultTheme::disableScroll(btnRow);

    // Botón MicroSD
    btnUnitSD = lv_button_create(btnRow);
    lv_obj_set_width(btnUnitSD, lv_pct(48));
    lv_obj_set_height(btnUnitSD, 34);
    DefaultTheme::applyButton(btnUnitSD, 8);
    lv_obj_add_event_cb(btnUnitSD, unit_sd_click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* lblSD = lv_label_create(btnUnitSD);
    lv_label_set_text(lblSD, LV_SYMBOL_SD_CARD " MicroSD");
    lv_obj_set_style_text_font(lblSD, &lv_font_montserrat_12, 0);
    lv_obj_center(lblSD);

    // Botón Flash Interna
    btnUnitFlash = lv_button_create(btnRow);
    lv_obj_set_width(btnUnitFlash, lv_pct(48));
    lv_obj_set_height(btnUnitFlash, 34);
    DefaultTheme::applyButton(btnUnitFlash, 8);
    lv_obj_add_event_cb(btnUnitFlash, unit_flash_click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* lblFlash = lv_label_create(btnUnitFlash);
    lv_label_set_text(lblFlash, LV_SYMBOL_SAVE " Flash");
    lv_obj_set_style_text_font(lblFlash, &lv_font_montserrat_12, 0);
    lv_obj_center(lblFlash);

    // Etiqueta de información de almacenamiento (Espacio libre / total)
    unitInfoLabel = lv_label_create(unitCard);
    lv_label_set_text(unitInfoLabel, "Espacio: Calculando...");
    lv_obj_set_style_text_color(unitInfoLabel, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(unitInfoLabel, &lv_font_montserrat_12, 0);
    lv_obj_set_style_pad_left(unitInfoLabel, 2, 0);
}

void FileManagerView::renderPathBar(lv_obj_t* parent) {
    lv_obj_t* pathBar = lv_obj_create(parent);
    lv_obj_set_width(pathBar, lv_pct(100));
    lv_obj_set_height(pathBar, 36);
    DefaultTheme::applySunkenCard(pathBar, 8);
    DefaultTheme::disableScroll(pathBar);
    lv_obj_set_flex_flow(pathBar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(pathBar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(pathBar, 2, 0);
    lv_obj_set_style_pad_column(pathBar, 6, 0);

    // Botón Subir nivel
    lv_obj_t* btnUp = lv_button_create(pathBar);
    lv_obj_set_size(btnUp, 32, 30);
    DefaultTheme::applyButton(btnUp, 6);
    lv_obj_add_event_cb(btnUp, btn_up_click_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lblUp = lv_label_create(btnUp);
    lv_label_set_text(lblUp, LV_SYMBOL_UP);
    lv_obj_set_style_text_font(lblUp, &lv_font_montserrat_12, 0);
    lv_obj_center(lblUp);

    // Etiqueta de ruta
    pathLabel = lv_label_create(pathBar);
    lv_obj_set_flex_grow(pathLabel, 1);
    lv_label_set_long_mode(pathLabel, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(pathLabel, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(pathLabel, &lv_font_montserrat_12, 0);
    updatePathLabel();

    // Botón Refrescar
    lv_obj_t* btnRef = lv_button_create(pathBar);
    lv_obj_set_size(btnRef, 32, 30);
    DefaultTheme::applyButton(btnRef, 6);
    lv_obj_add_event_cb(btnRef, btn_refresh_click_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lblRef = lv_label_create(btnRef);
    lv_label_set_text(lblRef, LV_SYMBOL_REFRESH);
    lv_obj_set_style_text_font(lblRef, &lv_font_montserrat_12, 0);
    lv_obj_center(lblRef);
}

void FileManagerView::updatePathLabel() {
    if (!pathLabel) return;
    std::string prefix = (currentStorage == StorageType::SD_CARD) ? "[SD] " : "[Flash] ";
    std::string display = prefix + currentPath;
    lv_label_set_text(pathLabel, display.c_str());
}

void FileManagerView::updateUnitButtons() {
    if (!btnUnitSD || !btnUnitFlash) return;

    if (currentStorage == StorageType::SD_CARD) {
        lv_obj_set_style_bg_color(btnUnitSD, lv_color_hex(0x242838), 0);
        lv_obj_set_style_border_color(btnUnitSD, DefaultTheme::getPrimaryAccent(), 0);
        lv_obj_set_style_border_width(btnUnitSD, 1, 0);

        lv_obj_set_style_bg_color(btnUnitFlash, lv_color_hex(0x1B1E29), 0);
        lv_obj_set_style_border_width(btnUnitFlash, 0, 0);
    } else {
        lv_obj_set_style_bg_color(btnUnitFlash, lv_color_hex(0x242838), 0);
        lv_obj_set_style_border_color(btnUnitFlash, DefaultTheme::getPrimaryAccent(), 0);
        lv_obj_set_style_border_width(btnUnitFlash, 1, 0);

        lv_obj_set_style_bg_color(btnUnitSD, lv_color_hex(0x1B1E29), 0);
        lv_obj_set_style_border_width(btnUnitSD, 0, 0);
    }
}

void FileManagerView::updateStorageInfo() {
    if (!unitInfoLabel) return;

    char infoBuf[128];
    if (currentStorage == StorageType::SD_CARD) {
        if (StorageManager::isSdAvailable()) {
            uint64_t total = StorageManager::getSdTotalBytes();
            uint64_t used = StorageManager::getSdUsedBytes();
            uint64_t freeB = (total >= used) ? (total - used) : 0;
            snprintf(infoBuf, sizeof(infoBuf), "SD: %s libre de %s (%s usado)",
                     StorageManager::formatBytes(freeB).c_str(),
                     StorageManager::formatBytes(total).c_str(),
                     StorageManager::formatBytes(used).c_str());
        } else {
            snprintf(infoBuf, sizeof(infoBuf), "MicroSD: No insertada o no montada");
        }
    } else {
        if (StorageManager::isFlashAvailable()) {
            uint64_t total = StorageManager::getFlashTotalBytes();
            uint64_t used = StorageManager::getFlashUsedBytes();
            uint64_t freeB = (total >= used) ? (total - used) : 0;
            snprintf(infoBuf, sizeof(infoBuf), "Flash: %s libre de %s (%s usado)",
                     StorageManager::formatBytes(freeB).c_str(),
                     StorageManager::formatBytes(total).c_str(),
                     StorageManager::formatBytes(used).c_str());
        } else {
            snprintf(infoBuf, sizeof(infoBuf), "Flash LittleFS: No inicializada");
        }
    }
    lv_label_set_text(unitInfoLabel, infoBuf);
}

void FileManagerView::renderFileList(lv_obj_t* parent) {
    if (!parent) return;

    // Limpiar hijos existentes
    lv_obj_clean(parent);

    std::vector<StorageFileInfo> files = StorageManager::listDirectory(currentStorage, currentPath);

    if (files.empty()) {
        lv_obj_t* emptyLabel = lv_label_create(parent);
        lv_label_set_text(emptyLabel, "(Carpeta vacía o sin acceso)");
        lv_obj_set_style_text_color(emptyLabel, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_font(emptyLabel, &lv_font_montserrat_12, 0);
        lv_obj_set_style_margin_top(emptyLabel, 20, 0);
        lv_obj_align(emptyLabel, LV_ALIGN_CENTER, 0, 0);
        return;
    }

    for (const auto& item : files) {
        lv_obj_t* itemRow = lv_obj_create(parent);
        lv_obj_set_width(itemRow, lv_pct(100));
        lv_obj_set_height(itemRow, 46);
        DefaultTheme::applySunkenCard(itemRow, 8);
        DefaultTheme::disableScroll(itemRow);
        lv_obj_set_flex_flow(itemRow, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(itemRow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(itemRow, 4, 0);
        lv_obj_set_style_pad_column(itemRow, 8, 0);

        // Icono
        const char* icon = LV_SYMBOL_FILE;
        uint32_t iconColor = 0xFFFFFF;
        getFileIconAndColor(item.name, item.isDirectory, &icon, &iconColor);

        lv_obj_t* iconLbl = lv_label_create(itemRow);
        lv_label_set_text(iconLbl, icon);
        lv_obj_set_style_text_color(iconLbl, lv_color_hex(iconColor), 0);
        lv_obj_set_style_text_font(iconLbl, &lv_font_montserrat_16, 0);

        // Contenedor de Textos (Nombre + Tamaño/Info)
        lv_obj_t* textCont = lv_obj_create(itemRow);
        lv_obj_set_flex_grow(textCont, 1);
        lv_obj_set_height(textCont, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(textCont, 0, 0);
        lv_obj_set_style_border_width(textCont, 0, 0);
        lv_obj_set_flex_flow(textCont, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_all(textCont, 0, 0);
        lv_obj_set_style_pad_row(textCont, 2, 0);
        DefaultTheme::disableScroll(textCont);

        lv_obj_t* nameLbl = lv_label_create(textCont);
        lv_label_set_text(nameLbl, item.name.c_str());
        lv_label_set_long_mode(nameLbl, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_color(nameLbl, DefaultTheme::getTextColor(), 0);
        lv_obj_set_style_text_font(nameLbl, &lv_font_montserrat_12, 0);

        lv_obj_t* sizeLbl = lv_label_create(textCont);
        if (item.isDirectory) {
            lv_label_set_text(sizeLbl, "<Directorio>");
        } else {
            lv_label_set_text(sizeLbl, StorageManager::formatBytes(item.size).c_str());
        }
        lv_obj_set_style_text_color(sizeLbl, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_font(sizeLbl, &lv_font_montserrat_12, 0);

        // Permitir click en la tarjeta para abrir
        lv_obj_add_flag(itemRow, LV_OBJ_FLAG_CLICKABLE);
        StorageFileInfo* pInfo = new StorageFileInfo(item);
        lv_obj_set_user_data(itemRow, pInfo);
        lv_obj_add_event_cb(itemRow, item_click_cb, LV_EVENT_CLICKED, pInfo);

        // Botón Eliminar (Papelera)
        lv_obj_t* delBtn = lv_button_create(itemRow);
        lv_obj_set_size(delBtn, 32, 32);
        DefaultTheme::applyButton(delBtn, 6);
        lv_obj_set_style_bg_color(delBtn, lv_color_hex(0x3B1D22), 0);
        lv_obj_set_style_border_color(delBtn, lv_color_hex(0xFF453A), 0);
        lv_obj_set_style_border_width(delBtn, 1, 0);

        lv_obj_t* delIcon = lv_label_create(delBtn);
        lv_label_set_text(delIcon, LV_SYMBOL_TRASH);
        lv_obj_set_style_text_color(delIcon, lv_color_hex(0xFF453A), 0);
        lv_obj_set_style_text_font(delIcon, &lv_font_montserrat_12, 0);
        lv_obj_center(delIcon);

        lv_obj_add_event_cb(delBtn, item_delete_click_cb, LV_EVENT_CLICKED, pInfo);
    }
}

void FileManagerView::refreshCurrentView() {
    if (listContainer) {
        renderFileList(listContainer);
    }
    updatePathLabel();
    updateStorageInfo();
}

void FileManagerView::unit_sd_click_cb(lv_event_t* e) {
    if (currentStorage != StorageType::SD_CARD) {
        currentStorage = StorageType::SD_CARD;
        currentPath = "/";
        updateUnitButtons();
        refreshCurrentView();
    }
}

void FileManagerView::unit_flash_click_cb(lv_event_t* e) {
    if (currentStorage != StorageType::FLASH_FS) {
        currentStorage = StorageType::FLASH_FS;
        currentPath = "/";
        updateUnitButtons();
        refreshCurrentView();
    }
}

void FileManagerView::btn_up_click_cb(lv_event_t* e) {
    if (currentPath == "/" || currentPath.empty()) {
        UIManager::showToast("Ya estás en el directorio raíz");
        return;
    }

    size_t lastSlash = currentPath.rfind('/');
    if (lastSlash == 0 || lastSlash == std::string::npos) {
        currentPath = "/";
    } else {
        currentPath = currentPath.substr(0, lastSlash);
    }
    refreshCurrentView();
}

void FileManagerView::btn_refresh_click_cb(lv_event_t* e) {
    refreshCurrentView();
    UIManager::showToast("Directorio actualizado");
}

void FileManagerView::item_click_cb(lv_event_t* e) {
    StorageFileInfo* pInfo = (StorageFileInfo*)lv_event_get_user_data(e);
    if (!pInfo) return;

    if (pInfo->isDirectory) {
        currentPath = pInfo->path;
        refreshCurrentView();
    } else {
        // Detectar tipo y realizar acción contextual
        std::string ext = "";
        size_t dotPos = pInfo->name.rfind('.');
        if (dotPos != std::string::npos) {
            ext = pInfo->name.substr(dotPos);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        }

        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg") {
            if (currentStorage == StorageType::SD_CARD) {
                std::string fullSdPath = "A:" + pInfo->path;
                UIManager::getInstance().loadImageViewer(fullSdPath, pInfo->name);
            } else {
                UIManager::showToast("Visor disponible para imágenes en SD");
            }
        } else if (ext == ".txt" || ext == ".json" || ext == ".enc" || ext == ".log" || ext == ".csv") {
            std::string content = StorageManager::readFilePreview(currentStorage, pInfo->path, 2048);
            showTextPreviewModal(pInfo->name, content);
        } else if (ext == ".mp3" || ext == ".wav") {
            UIManager::showToast("Abriendo reproductor de música...");
            UIManager::getInstance().loadMusicPlayer();
        } else if (ext == ".wad" || ext == ".gbc") {
            UIManager::showToast("Cartucho disponible en menú principal");
        } else {
            char msg[64];
            snprintf(msg, sizeof(msg), "Archivo: %s (%s)", pInfo->name.c_str(), StorageManager::formatBytes(pInfo->size).c_str());
            UIManager::showToast(msg);
        }
    }
}

void FileManagerView::item_delete_click_cb(lv_event_t* e) {
    StorageFileInfo* pInfo = (StorageFileInfo*)lv_event_get_user_data(e);
    if (!pInfo) return;
    showDeleteConfirmModal(*pInfo);
}

struct DeleteModalContext {
    StorageFileInfo fileInfo;
    lv_obj_t* mask;
};

void FileManagerView::modal_cancel_cb(lv_event_t* e) {
    DeleteModalContext* ctx = (DeleteModalContext*)lv_event_get_user_data(e);
    if (ctx) {
        if (lv_obj_is_valid(ctx->mask)) {
            lv_obj_delete_async(ctx->mask);
        }
        delete ctx;
    }
}

void FileManagerView::modal_confirm_delete_cb(lv_event_t* e) {
    DeleteModalContext* ctx = (DeleteModalContext*)lv_event_get_user_data(e);
    if (ctx) {
        bool ok = false;
        if (ctx->fileInfo.isDirectory) {
            ok = StorageManager::deleteDirectory(currentStorage, ctx->fileInfo.path);
        } else {
            ok = StorageManager::deleteFile(currentStorage, ctx->fileInfo.path);
        }

        if (ok) {
            UIManager::showToast("Elemento eliminado con éxito");
            FileManagerView::refreshCurrentView();
        } else {
            UIManager::showToast("Error al eliminar");
        }

        if (lv_obj_is_valid(ctx->mask)) {
            lv_obj_delete_async(ctx->mask);
        }
        delete ctx;
    }
}

void FileManagerView::showDeleteConfirmModal(const StorageFileInfo& file) {
    lv_obj_t* mask = lv_obj_create(lv_screen_active());
    lv_obj_add_flag(mask, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_size(mask, 320, 480);
    lv_obj_set_style_bg_color(mask, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(mask, LV_OPA_80, 0);
    lv_obj_set_style_border_width(mask, 0, 0);

    lv_obj_t* modal = lv_obj_create(mask);
    lv_obj_set_width(modal, 280);
    lv_obj_set_height(modal, LV_SIZE_CONTENT);
    DefaultTheme::applyRaisedCard(modal, 16);
    lv_obj_center(modal);
    lv_obj_set_flex_flow(modal, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(modal, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(modal, 16, 0);
    lv_obj_set_style_pad_row(modal, 12, 0);

    lv_obj_t* title = lv_label_create(modal);
    lv_label_set_text(title, LV_SYMBOL_WARNING " Eliminar");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFF453A), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);

    char msgBuf[128];
    snprintf(msgBuf, sizeof(msgBuf), "¿Deseas eliminar permanentemente \"%s\"?", file.name.c_str());
    lv_obj_t* msg = lv_label_create(modal);
    lv_label_set_text(msg, msgBuf);
    lv_label_set_long_mode(msg, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(msg, 240);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(msg, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(msg, &lv_font_montserrat_12, 0);

    // Botones de acción
    lv_obj_t* btnCont = lv_obj_create(modal);
    lv_obj_set_width(btnCont, 250);
    lv_obj_set_height(btnCont, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(btnCont, 0, 0);
    lv_obj_set_style_border_width(btnCont, 0, 0);
    lv_obj_set_flex_flow(btnCont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btnCont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(btnCont, 0, 0);

    DeleteModalContext* ctx = new DeleteModalContext{file, mask};

    // Botón Cancelar
    lv_obj_t* btnCancel = lv_button_create(btnCont);
    lv_obj_set_size(btnCancel, 110, 36);
    DefaultTheme::applyButton(btnCancel, 8);
    lv_obj_t* lblC = lv_label_create(btnCancel);
    lv_label_set_text(lblC, "Cancelar");
    lv_obj_set_style_text_color(lblC, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(lblC, &lv_font_montserrat_12, 0);
    lv_obj_center(lblC);
    lv_obj_add_event_cb(btnCancel, modal_cancel_cb, LV_EVENT_CLICKED, ctx);

    // Botón Eliminar
    lv_obj_t* btnDel = lv_button_create(btnCont);
    lv_obj_set_size(btnDel, 110, 36);
    DefaultTheme::applyButton(btnDel, 8);
    lv_obj_set_style_bg_color(btnDel, lv_color_hex(0xFF453A), 0);
    lv_obj_t* lblD = lv_label_create(btnDel);
    lv_label_set_text(lblD, "Eliminar");
    lv_obj_set_style_text_color(lblD, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(lblD, &lv_font_montserrat_12, 0);
    lv_obj_center(lblD);
    lv_obj_add_event_cb(btnDel, modal_confirm_delete_cb, LV_EVENT_CLICKED, ctx);
}

void FileManagerView::modal_close_preview_cb(lv_event_t* e) {
    lv_obj_t* mask = (lv_obj_t*)lv_event_get_user_data(e);
    if (mask && lv_obj_is_valid(mask)) {
        lv_obj_delete_async(mask);
    }
}

void FileManagerView::showTextPreviewModal(const std::string& fileName, const std::string& content) {
    lv_obj_t* mask = lv_obj_create(lv_screen_active());
    lv_obj_add_flag(mask, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_size(mask, 320, 480);
    lv_obj_set_style_bg_color(mask, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(mask, LV_OPA_80, 0);
    lv_obj_set_style_border_width(mask, 0, 0);

    lv_obj_t* modal = lv_obj_create(mask);
    lv_obj_set_width(modal, 290);
    lv_obj_set_height(modal, 380);
    DefaultTheme::applyRaisedCard(modal, 16);
    lv_obj_center(modal);
    lv_obj_set_flex_flow(modal, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(modal, 12, 0);
    lv_obj_set_style_pad_row(modal, 8, 0);

    // Barra superior del modal
    lv_obj_t* topBar = lv_obj_create(modal);
    lv_obj_set_width(topBar, lv_pct(100));
    lv_obj_set_height(topBar, 30);
    lv_obj_set_style_bg_opa(topBar, 0, 0);
    lv_obj_set_style_border_width(topBar, 0, 0);
    lv_obj_set_flex_flow(topBar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(topBar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(topBar, 0, 0);
    DefaultTheme::disableScroll(topBar);

    lv_obj_t* title = lv_label_create(topBar);
    lv_label_set_text(title, fileName.c_str());
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    lv_obj_set_width(title, 200);
    lv_obj_set_style_text_color(title, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);

    lv_obj_t* btnClose = lv_button_create(topBar);
    lv_obj_set_size(btnClose, 28, 28);
    DefaultTheme::applyButton(btnClose, 6);
    lv_obj_t* lblClose = lv_label_create(btnClose);
    lv_label_set_text(lblClose, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_font(lblClose, &lv_font_montserrat_12, 0);
    lv_obj_center(lblClose);
    lv_obj_add_event_cb(btnClose, modal_close_preview_cb, LV_EVENT_CLICKED, mask);

    // Contenedor scrollable con el texto
    lv_obj_t* textContainer = lv_obj_create(modal);
    lv_obj_set_width(textContainer, lv_pct(100));
    lv_obj_set_flex_grow(textContainer, 1);
    DefaultTheme::applySunkenCard(textContainer, 8);
    lv_obj_set_style_pad_all(textContainer, 8, 0);
    lv_obj_add_flag(textContainer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(textContainer, LV_DIR_VER);

    lv_obj_t* contentLbl = lv_label_create(textContainer);
    lv_label_set_text(contentLbl, content.empty() ? "(Archivo vacío)" : content.c_str());
    lv_label_set_long_mode(contentLbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(contentLbl, 245);
    lv_obj_set_style_text_color(contentLbl, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(contentLbl, &lv_font_montserrat_12, 0);
}
