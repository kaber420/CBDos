#include "UIManager.h"
#include "Views/DashboardView.h"
#include "Views/MenuView.h"
#include "Themes/DefaultTheme.h"
#include <cstdio>

lv_obj_t* UIManager::toastObj = nullptr;
lv_timer_t* UIManager::toastTimer = nullptr;

void UIManager::init() {
    loadLauncher();
}

void UIManager::update() {
}

void UIManager::destroyTransient() {
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
