// ==========================================================================
// CartridgeView.cpp — Gestor y Lanzador Multi-Slot de Cartuchos con MicroSD
// ==========================================================================

#include "CartridgeView.h"
#include "../UIManager.h"
#include "../Themes/DefaultTheme.h"
#include "../../Core/CartridgeManager.h"
#include "../../Core/StorageManager.h"
#include <cstdio>
#include <vector>
#include <string>

#ifdef ARDUINO
#include <Arduino.h>
#endif

static lv_obj_t* s_screen = nullptr;
static lv_obj_t* s_modalBg = nullptr;

// Prototipos locales
static void showSDPickerDialog(esp_partition_subtype_t targetSlot);
static void closeCurrentModal();

// ─── Callbacks de Navegación y Boot ───────────────────────────────────────
static void exitBtnCb(lv_event_t* e) {
    (void)e;
    UIManager::getInstance().loadLauncher();
}

static void bootSlotCb(lv_event_t* e) {
    esp_partition_subtype_t subtype = (esp_partition_subtype_t)(intptr_t)lv_event_get_user_data(e);
    
    if (!CartridgeManager::isSlotInstalled(subtype)) {
        UIManager::showToast("Ranura vacia. Instala un .bin primero");
        return;
    }

    CartridgeSlotInfo info = CartridgeManager::getSlotInfo(subtype);
    char buf[64];
    snprintf(buf, sizeof(buf), "Iniciando %s...", info.projectName.c_str());
    UIManager::showToast(buf);

#ifdef ARDUINO
    delay(500);
    CartridgeManager::bootSlot(subtype);
#else
    UIManager::showToast("Boot solo en hardware real");
#endif
}

static void openInstallModalCb(lv_event_t* e) {
    esp_partition_subtype_t subtype = (esp_partition_subtype_t)(intptr_t)lv_event_get_user_data(e);
    showSDPickerDialog(subtype);
}

// ─── Diálogo de Selección de Archivo .BIN desde MicroSD ───────────────────
static void fileSelectedCb(lv_event_t* e) {
    struct InstallContext {
        std::string path;
        esp_partition_subtype_t slot;
    };
    
    InstallContext* ctx = (InstallContext*)lv_event_get_user_data(e);
    std::string binPath = ctx->path;
    esp_partition_subtype_t targetSlot = ctx->slot;
    delete ctx;

    closeCurrentModal();

    if (!StorageManager::isSdAvailable()) {
        UIManager::showToast("Error: Tarjeta MicroSD no detectada");
        return;
    }

    // Modal de Progreso de Flasheo
    lv_obj_t* progModal = lv_obj_create(s_screen);
    lv_obj_set_size(progModal, 280, 190);
    lv_obj_center(progModal);
    lv_obj_set_style_bg_color(progModal, lv_color_hex(0x1F1F1F), 0);
    lv_obj_set_style_border_color(progModal, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_border_width(progModal, 2, 0);
    lv_obj_set_style_radius(progModal, 14, 0);
    s_modalBg = progModal;

    lv_obj_t* pTitle = lv_label_create(progModal);
    lv_label_set_text(pTitle, "Flasheando Cartucho");
    lv_obj_set_style_text_font(pTitle, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(pTitle, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(pTitle, LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_t* pSub = lv_label_create(progModal);
    size_t lastSlash = binPath.rfind('/');
    std::string fileName = (lastSlash != std::string::npos) ? binPath.substr(lastSlash + 1) : binPath;
    lv_label_set_text(pSub, fileName.c_str());
    lv_obj_set_style_text_font(pSub, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(pSub, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(pSub, LV_ALIGN_TOP_MID, 0, 35);

    lv_obj_t* pBar = lv_bar_create(progModal);
    lv_obj_set_size(pBar, 230, 18);
    lv_obj_align(pBar, LV_ALIGN_CENTER, 0, 5);
    lv_bar_set_range(pBar, 0, 100);
    lv_bar_set_value(pBar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(pBar, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_color(pBar, lv_color_hex(0x00E5FF), LV_PART_INDICATOR);

    lv_obj_t* pPercent = lv_label_create(progModal);
    lv_label_set_text(pPercent, "0 %");
    lv_obj_set_style_text_font(pPercent, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(pPercent, lv_color_hex(0x00E5FF), 0);
    lv_obj_align(pPercent, LV_ALIGN_BOTTOM_MID, 0, -10);

    lv_timer_handler();

    // Ejecutar Flasheo
    bool ok = CartridgeManager::flashFromSD(binPath, targetSlot, [pBar, pPercent](size_t written, size_t total) {
        if (total > 0) {
            int pct = (int)((written * 100) / total);
            lv_bar_set_value(pBar, pct, LV_ANIM_OFF);
            char pctBuf[16];
            snprintf(pctBuf, sizeof(pctBuf), "%d %%", pct);
            lv_label_set_text(pPercent, pctBuf);
            lv_timer_handler();
        }
    });

    closeCurrentModal();

    if (ok) {
        UIManager::showToast("Instalacion exitosa!");
        // Re-crear la vista para reflejar el nuevo cartucho instalado
        UIManager::getInstance().loadCartridges();
    } else {
        UIManager::showToast("Fallo al escribir en Flash");
    }
}

static void closeModalCb(lv_event_t* e) {
    (void)e;
    closeCurrentModal();
}

static void closeCurrentModal() {
    if (s_modalBg && lv_obj_is_valid(s_modalBg)) {
        lv_obj_delete(s_modalBg);
        s_modalBg = nullptr;
    }
}

static void showSDPickerDialog(esp_partition_subtype_t targetSlot) {
    closeCurrentModal();

    s_modalBg = lv_obj_create(s_screen);
    lv_obj_set_size(s_modalBg, 300, 380);
    lv_obj_center(s_modalBg);
    lv_obj_set_style_bg_color(s_modalBg, lv_color_hex(0x181818), 0);
    lv_obj_set_style_border_color(s_modalBg, lv_color_hex(0x444444), 0);
    lv_obj_set_style_border_width(s_modalBg, 2, 0);
    lv_obj_set_style_radius(s_modalBg, 16, 0);
    lv_obj_set_style_pad_all(s_modalBg, 12, 0);

    // Encabezado del modal
    lv_obj_t* mTitle = lv_label_create(s_modalBg);
    char titleBuf[64];
    snprintf(titleBuf, sizeof(titleBuf), "Instalar en Ranura %d", 
             (targetSlot == ESP_PARTITION_SUBTYPE_APP_OTA_1) ? 1 : 2);
    lv_label_set_text(mTitle, titleBuf);
    lv_obj_set_style_text_font(mTitle, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(mTitle, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(mTitle, LV_ALIGN_TOP_LEFT, 5, 5);

    lv_obj_t* closeBtn = lv_button_create(s_modalBg);
    lv_obj_set_size(closeBtn, 30, 30);
    lv_obj_align(closeBtn, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(closeBtn, lv_color_hex(0x333333), 0);
    lv_obj_set_style_radius(closeBtn, 15, 0);
    lv_obj_t* closeLbl = lv_label_create(closeBtn);
    lv_label_set_text(closeLbl, LV_SYMBOL_CLOSE);
    lv_obj_center(closeLbl);
    lv_obj_add_event_cb(closeBtn, closeModalCb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* subTitle = lv_label_create(s_modalBg);
    lv_label_set_text(subTitle, "Buscando archivos en /sd/cartridges/...");
    lv_obj_set_style_text_font(subTitle, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(subTitle, lv_color_hex(0x888888), 0);
    lv_obj_align(subTitle, LV_ALIGN_TOP_LEFT, 5, 30);

    // Contenedor de lista
    lv_obj_t* list = lv_obj_create(s_modalBg);
    lv_obj_set_size(list, LV_PCT(100), 280);
    lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, 2, 0);
    lv_obj_set_style_pad_row(list, 8, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);

    std::vector<std::string> binFiles = CartridgeManager::listBinFilesOnSD("/cartridges");

    if (binFiles.empty()) {
        lv_obj_t* emptyLbl = lv_label_create(list);
        lv_label_set_text(emptyLbl, "No hay archivos .bin en\n/sd/cartridges/\n\nCopia tus cartuchos a la MicroSD.");
        lv_obj_set_style_text_align(emptyLbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(emptyLbl, lv_color_hex(0xFFA500), 0);
        lv_obj_center(emptyLbl);
        return;
    }

    struct InstallContext {
        std::string path;
        esp_partition_subtype_t slot;
    };

    for (const auto& file : binFiles) {
        size_t slashIdx = file.rfind('/');
        std::string fName = (slashIdx != std::string::npos) ? file.substr(slashIdx + 1) : file;

        lv_obj_t* itemBtn = lv_button_create(list);
        lv_obj_set_width(itemBtn, LV_PCT(100));
        lv_obj_set_height(itemBtn, 48);
        lv_obj_set_style_bg_color(itemBtn, lv_color_hex(0x282828), 0);
        lv_obj_set_style_radius(itemBtn, 8, 0);
        lv_obj_set_style_border_width(itemBtn, 1, 0);
        lv_obj_set_style_border_color(itemBtn, lv_color_hex(0x404040), 0);

        lv_obj_t* itemIcon = lv_label_create(itemBtn);
        lv_label_set_text(itemIcon, LV_SYMBOL_FILE);
        lv_obj_set_style_text_color(itemIcon, lv_color_hex(0x00E5FF), 0);
        lv_obj_align(itemIcon, LV_ALIGN_LEFT_MID, 5, 0);

        lv_obj_t* itemLbl = lv_label_create(itemBtn);
        lv_label_set_text(itemLbl, fName.c_str());
        lv_obj_set_style_text_font(itemLbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(itemLbl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(itemLbl, LV_ALIGN_LEFT_MID, 30, 0);

        InstallContext* ctx = new InstallContext{file, targetSlot};
        lv_obj_add_event_cb(itemBtn, fileSelectedCb, LV_EVENT_CLICKED, ctx);
    }
}

// ─── Renderizado de Tarjeta de Ranura ─────────────────────────────────────
static void createSlotCard(lv_obj_t* parent, esp_partition_subtype_t subtype, 
                           const char* slotTitle, const char* capacityStr, 
                           uint32_t bgHex, uint32_t borderHex) {
    CartridgeSlotInfo info = CartridgeManager::getSlotInfo(subtype);

    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_height(card, 165);
    lv_obj_set_style_bg_color(card, lv_color_hex(bgHex), 0);
    lv_obj_set_style_border_color(card, lv_color_hex(borderHex), 0);
    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_style_radius(card, 14, 0);
    lv_obj_set_style_pad_all(card, 12, 0);
    DefaultTheme::disableScroll(card);

    // Cabecera de la Ranura
    lv_obj_t* tag = lv_label_create(card);
    char headerBuf[64];
    snprintf(headerBuf, sizeof(headerBuf), "%s  [%s]", slotTitle, capacityStr);
    lv_label_set_text(tag, headerBuf);
    lv_obj_set_style_text_font(tag, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(tag, lv_color_hex(borderHex), 0);
    lv_obj_align(tag, LV_ALIGN_TOP_LEFT, 0, 0);

    // Nombre del Cartucho instalado
    lv_obj_t* nameLbl = lv_label_create(card);
    lv_label_set_text(nameLbl, info.projectName.c_str());
    lv_obj_set_style_text_font(nameLbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(nameLbl, info.isInstalled ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x888888), 0);
    lv_obj_align(nameLbl, LV_ALIGN_TOP_LEFT, 0, 22);

    // Detalle de versión / estado
    lv_obj_t* detailLbl = lv_label_create(card);
    if (info.isInstalled) {
        char detailBuf[64];
        snprintf(detailBuf, sizeof(detailBuf), "v%s • %s", 
                 info.version.empty() ? "1.0" : info.version.c_str(), 
                 info.compileDate.empty() ? "Instalado" : info.compileDate.c_str());
        lv_label_set_text(detailLbl, detailBuf);
        lv_obj_set_style_text_color(detailLbl, lv_color_hex(0x00FF88), 0);
    } else {
        lv_label_set_text(detailLbl, "Disponible para instalar .bin");
        lv_obj_set_style_text_color(detailLbl, lv_color_hex(0xAAAAAA), 0);
    }
    lv_obj_set_style_text_font(detailLbl, &lv_font_montserrat_12, 0);
    lv_obj_align(detailLbl, LV_ALIGN_TOP_LEFT, 0, 48);

    // Fila de Botones
    lv_obj_t* btnRow = lv_obj_create(card);
    lv_obj_set_size(btnRow, LV_PCT(100), 50);
    lv_obj_align(btnRow, LV_ALIGN_BOTTOM_MID, 0, 5);
    lv_obj_set_style_bg_opa(btnRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btnRow, 0, 0);
    lv_obj_set_style_pad_all(btnRow, 0, 0);
    lv_obj_set_flex_flow(btnRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btnRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Botón Iniciar
    lv_obj_t* runBtn = lv_button_create(btnRow);
    lv_obj_set_width(runBtn, LV_PCT(48));
    lv_obj_set_height(runBtn, 42);
    lv_obj_set_style_bg_color(runBtn, info.isInstalled ? lv_color_hex(borderHex) : lv_color_hex(0x2C2C2C), 0);
    lv_obj_set_style_radius(runBtn, 10, 0);
    lv_obj_add_event_cb(runBtn, bootSlotCb, LV_EVENT_CLICKED, (void*)(intptr_t)subtype);

    lv_obj_t* runLbl = lv_label_create(runBtn);
    lv_label_set_text(runLbl, LV_SYMBOL_PLAY " Iniciar");
    lv_obj_set_style_text_font(runLbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(runLbl, info.isInstalled ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x666666), 0);
    lv_obj_center(runLbl);

    // Botón Instalar SD
    lv_obj_t* instBtn = lv_button_create(btnRow);
    lv_obj_set_width(instBtn, LV_PCT(48));
    lv_obj_set_height(instBtn, 42);
    lv_obj_set_style_bg_color(instBtn, lv_color_hex(0x282828), 0);
    lv_obj_set_style_border_width(instBtn, 1, 0);
    lv_obj_set_style_border_color(instBtn, lv_color_hex(0x555555), 0);
    lv_obj_set_style_radius(instBtn, 10, 0);
    lv_obj_add_event_cb(instBtn, openInstallModalCb, LV_EVENT_CLICKED, (void*)(intptr_t)subtype);

    lv_obj_t* instLbl = lv_label_create(instBtn);
    lv_label_set_text(instLbl, LV_SYMBOL_SD_CARD " Instalar");
    lv_obj_set_style_text_font(instLbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(instLbl, lv_color_hex(0xDDDDDD), 0);
    lv_obj_center(instLbl);
}

// ─── CartridgeView::create() ──────────────────────────────────────────────
lv_obj_t* CartridgeView::create() {
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(0x121212), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    DefaultTheme::disableScroll(s_screen);

    // ── Título ──
    lv_obj_t* title = lv_label_create(s_screen);
    lv_label_set_text(title, "Gestor de Cartuchos");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 20, 20);

    // ── Subtítulo ──
    lv_obj_t* sub = lv_label_create(s_screen);
    lv_label_set_text(sub, "Ranuras OTA y Flasheo MicroSD");
    lv_obj_set_style_text_color(sub, lv_color_hex(0x777777), 0);
    lv_obj_set_style_text_font(sub, &lv_font_montserrat_12, 0);
    lv_obj_align(sub, LV_ALIGN_TOP_LEFT, 20, 46);

    // ── Botón de salir ──
    lv_obj_t* exitBtn = lv_button_create(s_screen);
    lv_obj_set_size(exitBtn, 36, 36);
    lv_obj_align(exitBtn, LV_ALIGN_TOP_RIGHT, -15, 18);
    lv_obj_set_style_bg_color(exitBtn, lv_color_hex(0x282828), 0);
    lv_obj_set_style_radius(exitBtn, 18, 0);
    
    lv_obj_t* exitIcon = lv_label_create(exitBtn);
    lv_label_set_text(exitIcon, LV_SYMBOL_CLOSE);
    lv_obj_center(exitIcon);
    lv_obj_add_event_cb(exitBtn, exitBtnCb, LV_EVENT_CLICKED, NULL);

    // ── Contenedor de Ranuras ──
    lv_obj_t* container = lv_obj_create(s_screen);
    lv_obj_set_width(container, LV_PCT(94));
    lv_obj_set_height(container, 385);
    lv_obj_align(container, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_pad_all(container, 2, 0);
    lv_obj_set_style_pad_row(container, 14, 0);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(container, LV_DIR_VER);

    // 1. Ranura Grande (app1 - 4MB)
    createSlotCard(container, ESP_PARTITION_SUBTYPE_APP_OTA_1, 
                   "RANURA 1", "4.0 MB", 0x1A1F36, 0x3F68D9);

    // 2. Ranura Pequeña (app2 - 2MB)
    createSlotCard(container, ESP_PARTITION_SUBTYPE_APP_OTA_2, 
                   "RANURA 2", "2.0 MB", 0x142826, 0x1DB89C);

    return s_screen;
}
