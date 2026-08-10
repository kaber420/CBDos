#include "UIManager.h"
#include "Views/DashboardView.h"
#include "Views/MenuView.h"
#include "Themes/DefaultTheme.h"
#include <cstdio>

lv_obj_t* UIManager::toastObj = nullptr;
lv_timer_t* UIManager::toastTimer = nullptr;
static lv_obj_t* activeKeyboard = nullptr;

static void kb_event_cb(lv_event_t* ev) {
    lv_event_code_t c = lv_event_get_code(ev);
    if (c == LV_EVENT_READY || c == LV_EVENT_CANCEL) {
        if (activeKeyboard && lv_obj_is_valid(activeKeyboard)) {
            lv_obj_delete_async(activeKeyboard);
            activeKeyboard = nullptr;
        }
    }
}

static void ta_focus_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* targetTa = (lv_obj_t*)lv_event_get_target(e);
    if (code == LV_EVENT_FOCUSED || code == LV_EVENT_CLICKED) {
        if (!activeKeyboard || !lv_obj_is_valid(activeKeyboard)) {
            activeKeyboard = lv_keyboard_create(lv_screen_active());
            lv_obj_set_style_bg_color(activeKeyboard, lv_color_hex(0x1B1E29), 0);
            lv_obj_set_style_border_color(activeKeyboard, lv_color_hex(0x2E3444), 0);
            lv_obj_set_style_border_width(activeKeyboard, 1, 0);
            lv_obj_add_event_cb(activeKeyboard, kb_event_cb, LV_EVENT_ALL, NULL);
        }
        lv_keyboard_set_textarea(activeKeyboard, targetTa);
    }
}

void UIManager::attachKeyboard(lv_obj_t* ta) {
    if (!ta) return;
    lv_obj_add_event_cb(ta, ta_focus_cb, LV_EVENT_ALL, NULL);
}

void UIManager::init() {
    loadLauncher();
}

void UIManager::update() {
}

void UIManager::destroyTransient() {
    if (activeKeyboard && lv_obj_is_valid(activeKeyboard)) {
        lv_obj_del(activeKeyboard);
        activeKeyboard = nullptr;
    }
    if (currentTransientScreen) {
        lv_obj_del(currentTransientScreen);
        currentTransientScreen = nullptr;
    }
}

void UIManager::loadLauncher() {
    destroyTransient();
    if (!launcherScreen) {
        launcherScreen = DashboardView::create();
    }
    lv_screen_load(launcherScreen);
}

void UIManager::loadMediaGallery() {
    destroyTransient();
    currentTransientScreen = MenuView::create();
    lv_screen_load(currentTransientScreen);
}

void UIManager::loadMeshChat() {
    showToast("Mesh Chat: Iniciando...");
}

void UIManager::loadWavBrowser() {
    showToast("WAV Browser: Iniciando...");
}

#include "Views/ConfigView.h"
#include "Views/WiFiConfigView.h"
#include "Views/LoRaConfigView.h"
#include "Views/FLRCConfigView.h"
#include "Views/GatewayConfigView.h"
#include "Views/MusicView.h"
#include "Views/GalleryView.h"

void UIManager::loadMusicPlayer() {
    destroyTransient();
    currentTransientScreen = MusicView::create();
    lv_screen_load(currentTransientScreen);
}

void UIManager::loadImageViewer(const std::string& imagePath, const std::string& imageName) {
    destroyTransient();
    currentTransientScreen = GalleryView::create(imagePath, imageName);
    lv_screen_load(currentTransientScreen);
}

void UIManager::loadConfigView() {
    destroyTransient();
    currentTransientScreen = ConfigView::create();
    lv_screen_load(currentTransientScreen);
}

void UIManager::loadWiFiConfig() {
    destroyTransient();
    currentTransientScreen = WiFiConfigView::create();
    lv_screen_load(currentTransientScreen);
}

void UIManager::loadLoRaConfig() {
    destroyTransient();
    currentTransientScreen = LoRaConfigView::create();
    lv_screen_load(currentTransientScreen);
}

void UIManager::loadFLRCConfig() {
    destroyTransient();
    currentTransientScreen = FLRCConfigView::create();
    lv_screen_load(currentTransientScreen);
}

void UIManager::loadGatewayConfig() {
    destroyTransient();
    currentTransientScreen = GatewayConfigView::create();
    lv_screen_load(currentTransientScreen);
}

void UIManager::showToast(const char* message) {
    if (toastObj) {
        lv_obj_del(toastObj);
        toastObj = nullptr;
    }
    if (toastTimer) {
        lv_timer_del(toastTimer);
        toastTimer = nullptr;
    }

    lv_obj_t* activeScr = lv_screen_active();
    if (!activeScr) return;

    toastObj = lv_obj_create(activeScr);
    DefaultTheme::applyRaisedCard(toastObj, 20);
    lv_obj_set_size(toastObj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(toastObj, LV_ALIGN_BOTTOM_MID, 0, -30);
    lv_obj_set_style_pad_left(toastObj, 16, 0);
    lv_obj_set_style_pad_right(toastObj, 16, 0);
    lv_obj_set_style_pad_top(toastObj, 10, 0);
    lv_obj_set_style_pad_bottom(toastObj, 10, 0);

    lv_obj_t* label = lv_label_create(toastObj);
    lv_label_set_text(label, message);
    lv_obj_set_style_text_color(label, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);

    toastTimer = lv_timer_create(toast_timer_cb, 2500, NULL);
    lv_timer_set_repeat_count(toastTimer, 1);
}

void UIManager::toast_timer_cb(lv_timer_t* timer) {
    if (toastObj) {
        lv_obj_del(toastObj);
        toastObj = nullptr;
    }
    toastTimer = nullptr;
}
