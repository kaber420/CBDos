// ==========================================================================
// DoomView.cpp — Lanzador OTA Multi-Cartucho para juegos y emuladores
// ==========================================================================

#include "DoomView.h"
#include "../UIManager.h"
#include "../Themes/DefaultTheme.h"
#include <cstdio>

#ifdef ARDUINO
#include <Arduino.h>
#include <esp_ota_ops.h>
#endif

// ─── Función genérica para arrancar una partición OTA ─────────────────────
#ifdef ARDUINO
static void bootToPartition(esp_partition_subtype_t subtype, const char* gameName) {
    const esp_partition_t* game_partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, subtype, NULL);

    if (game_partition != NULL) {
        Serial.printf("[OTA] Partición para %s encontrada. Configurando boot...\n", gameName);
        esp_err_t err = esp_ota_set_boot_partition(game_partition);
        if (err == ESP_OK) {
            Serial.printf("[OTA] Reiniciando en %s...\n", gameName);
            UIManager::showToast("Iniciando...");
            delay(800);
            esp_restart();
        } else {
            Serial.printf("[OTA] Error al configurar el boot: %s\n", esp_err_to_name(err));
            UIManager::showToast("Error critico al iniciar");
        }
    } else {
        Serial.printf("[OTA] Error: No se encontro la particion para %s\n", gameName);
        UIManager::showToast("Juego no instalado");
    }
}
#endif

// ─── Callbacks ────────────────────────────────────────────────────────────
static void exitBtnCb(lv_event_t* e) {
    (void)e;
    UIManager::getInstance().loadLauncher();
}

static void launchDoomCb(lv_event_t* e) {
    (void)e;
#ifdef ARDUINO
    bootToPartition(ESP_PARTITION_SUBTYPE_APP_OTA_1, "DOOM");
#else
    UIManager::showToast("OTA solo funciona en hardware real");
#endif
}

static void launchGBCCb(lv_event_t* e) {
    (void)e;
#ifdef ARDUINO
    bootToPartition(ESP_PARTITION_SUBTYPE_APP_OTA_2, "Game Boy Color");
#else
    UIManager::showToast("OTA solo funciona en hardware real");
#endif
}

// ─── DoomView::create() ───────────────────────────────────────────────────
lv_obj_t* DoomView::create() {
    // ── Screen principal ──
    lv_obj_t* screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x121212), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    DefaultTheme::disableScroll(screen);

    // ── Título ──
    lv_obj_t* title = lv_label_create(screen);
    lv_label_set_text(title, "Centro de Juegos OTA");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 25);

    // ── Subtítulo ──
    lv_obj_t* sub = lv_label_create(screen);
    lv_label_set_text(sub, "Selecciona un cartucho nativo para arrancar");
    lv_obj_set_style_text_color(sub, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(sub, &lv_font_montserrat_14, 0);
    lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 58);

    // ── Contenedor de Botones / Tarjetas ──
    lv_obj_t* container = lv_obj_create(screen);
    lv_obj_set_size(container, 440, 210);
    lv_obj_align(container, LV_ALIGN_CENTER, 0, 25);
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_pad_all(container, 0, 0);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // 1. Tarjeta DOOM (app1)
    lv_obj_t* doomCard = lv_button_create(container);
    lv_obj_set_size(doomCard, 195, 170);
    lv_obj_set_style_bg_color(doomCard, lv_color_hex(0x7A1A1A), 0);
    lv_obj_set_style_bg_grad_color(doomCard, lv_color_hex(0x3B0A0A), 0);
    lv_obj_set_style_bg_grad_dir(doomCard, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_radius(doomCard, 14, 0);
    lv_obj_set_style_border_width(doomCard, 2, 0);
    lv_obj_set_style_border_color(doomCard, lv_color_hex(0xFF4444), 0);
    lv_obj_add_event_cb(doomCard, launchDoomCb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* doomLabel = lv_label_create(doomCard);
    lv_label_set_text(doomLabel, "DOOM\n(Classic FPS)\n\n[ Particion OTA 1 ]");
    lv_obj_set_style_text_align(doomLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(doomLabel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(doomLabel);

    // 2. Tarjeta Game Boy Color (app2)
    lv_obj_t* gbcCard = lv_button_create(container);
    lv_obj_set_size(gbcCard, 195, 170);
    lv_obj_set_style_bg_color(gbcCard, lv_color_hex(0x1B4D7E), 0);
    lv_obj_set_style_bg_grad_color(gbcCard, lv_color_hex(0x0C233C), 0);
    lv_obj_set_style_bg_grad_dir(gbcCard, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_radius(gbcCard, 14, 0);
    lv_obj_set_style_border_width(gbcCard, 2, 0);
    lv_obj_set_style_border_color(gbcCard, lv_color_hex(0x40A0FF), 0);
    lv_obj_add_event_cb(gbcCard, launchGBCCb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* gbcLabel = lv_label_create(gbcCard);
    lv_label_set_text(gbcLabel, "GAME BOY\nCOLOR\n\n[ Particion OTA 2 ]");
    lv_obj_set_style_text_align(gbcLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(gbcLabel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(gbcLabel);

    // ── Botón de salir ──
    lv_obj_t* exitBtn = lv_button_create(screen);
    lv_obj_set_size(exitBtn, 40, 40);
    lv_obj_align(exitBtn, LV_ALIGN_TOP_RIGHT, -12, 12);
    lv_obj_set_style_bg_color(exitBtn, lv_color_hex(0x333333), 0);
    lv_obj_set_style_radius(exitBtn, 20, 0);
    
    lv_obj_t* exitIcon = lv_label_create(exitBtn);
    lv_label_set_text(exitIcon, LV_SYMBOL_CLOSE);
    lv_obj_center(exitIcon);
    lv_obj_add_event_cb(exitBtn, exitBtnCb, LV_EVENT_CLICKED, NULL);

    return screen;
}
