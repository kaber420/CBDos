// ==========================================================================
// DoomView.cpp — Lanzador OTA para juegos / emuladores
// ==========================================================================

#include "DoomView.h"
#include "../UIManager.h"
#include "../Themes/DefaultTheme.h"
#include <cstdio>

#ifdef ARDUINO
#include <Arduino.h>
#include <esp_ota_ops.h>
#endif

// ─── Estado local del módulo ───────────────────────────────────────────
static lv_obj_t* s_exitBtn = nullptr;
static lv_obj_t* s_launchBtn = nullptr;

// ─── Función para saltar a la partición del juego ──────────────────────
#ifdef ARDUINO
static void bootToGamePartition() {
    const esp_partition_t* game_partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, NULL);

    if (game_partition != NULL) {
        Serial.println("[OTA] Partición de juegos encontrada. Configurando boot...");
        esp_err_t err = esp_ota_set_boot_partition(game_partition);
        if (err == ESP_OK) {
            Serial.println("[OTA] Reiniciando en modo Consola (Juegos)...");
            UIManager::showToast("Iniciando Juego...");
            delay(1000); // Dar tiempo a que se muestre el toast
            esp_restart();
        } else {
            Serial.printf("[OTA] Error al configurar el boot: %s\n", esp_err_to_name(err));
            UIManager::showToast("Error crítico al iniciar juego");
        }
    } else {
        Serial.println("[OTA] Error: No se encontró la partición app1 (Juegos)");
        UIManager::showToast("Partición de juegos no instalada");
    }
}
#endif

// ─── Callbacks ────────────────────────────────────────────────────────
static void exitBtnCb(lv_event_t* e) {
    (void)e;
    UIManager::getInstance().loadLauncher();
}

static void launchBtnCb(lv_event_t* e) {
    (void)e;
#ifdef ARDUINO
    bootToGamePartition();
#else
    UIManager::showToast("OTA solo funciona en hardware real");
#endif
}

// ─── DoomView::create() ────────────────────────────────────────────────
lv_obj_t* DoomView::create() {
    // ── Crear screen principal ──
    lv_obj_t* screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    DefaultTheme::disableScroll(screen);

    // ── Título ──
    lv_obj_t* title = lv_label_create(screen);
    lv_label_set_text(title, "Lanzador de Juegos");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 40);

    // ── Botón de lanzar ──
    s_launchBtn = lv_button_create(screen);
    lv_obj_set_size(s_launchBtn, 200, 60);
    lv_obj_center(s_launchBtn);
    lv_obj_set_style_bg_color(s_launchBtn, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_radius(s_launchBtn, 10, 0);
    lv_obj_add_event_cb(s_launchBtn, launchBtnCb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* launchLabel = lv_label_create(s_launchBtn);
    lv_label_set_text(launchLabel, "Entrar a DOOM\n(Modo Consola)");
    lv_obj_set_style_text_align(launchLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(launchLabel);

    // ── Botón de salir ──
    s_exitBtn = lv_button_create(screen);
    lv_obj_set_size(s_exitBtn, 40, 40);
    lv_obj_align(s_exitBtn, LV_ALIGN_TOP_RIGHT, -10, 10);
    lv_obj_set_style_bg_color(s_exitBtn, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_radius(s_exitBtn, 20, 0);
    
    lv_obj_t* exitIcon = lv_label_create(s_exitBtn);
    lv_label_set_text(exitIcon, LV_SYMBOL_CLOSE);
    lv_obj_center(exitIcon);
    lv_obj_add_event_cb(s_exitBtn, exitBtnCb, LV_EVENT_CLICKED, NULL);

    return screen;
}

