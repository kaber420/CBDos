#include "HeaderBar.h"
#include "../Themes/DefaultTheme.h"
#include "../UIManager.h"
#include "../Modals/DiagnosticsModal.h"
#include "../../Core/SystemDiagnostics.h"
#include "../../Core/SystemStateAPI.h"
#include <cstring>
#include <cstdio>

// Inicialización de miembros estáticos
HeaderBar* HeaderBar::activeHeader = nullptr;
char HeaderBar::lastTimeStr[16] = "--:--";
int HeaderBar::lastBatteryPercentage = 85;
int HeaderBar::lastSignalStrength = -999;
int HeaderBar::lastCartCount = 0;

HeaderBar* HeaderBar::create(lv_obj_t* parent, const char* title, bool showBackButton, bool showStatus, bool showCartButton) {
    HeaderBar* hb = new HeaderBar();

    // Contenedor principal del Header
    hb->container = lv_obj_create(parent);
    lv_obj_set_width(hb->container, lv_pct(100));
    lv_obj_set_height(hb->container, 44);
    DefaultTheme::applyRaisedCard(hb->container, 14);
    DefaultTheme::disableScroll(hb->container);
    lv_obj_set_style_pad_all(hb->container, 0, 0);

    // Botón de Volver (si está habilitado)
    if (showBackButton) {
        hb->backButton = lv_button_create(hb->container);
        lv_obj_set_size(hb->backButton, 84, 30);
        lv_obj_align(hb->backButton, LV_ALIGN_LEFT_MID, 8, 0);
        DefaultTheme::applyButton(hb->backButton, 10);
        lv_obj_add_event_cb(hb->backButton, back_event_cb, LV_EVENT_CLICKED, nullptr);

        lv_obj_t * backLbl = lv_label_create(hb->backButton);
        lv_label_set_text(backLbl, LV_SYMBOL_LEFT " Volver");
        lv_obj_set_style_text_color(backLbl, DefaultTheme::getPrimaryAccent(), 0);
        lv_obj_set_style_text_font(backLbl, &lv_font_montserrat_14, 0);
        lv_obj_remove_flag(backLbl, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_center(backLbl);
    }

    // Título de la pantalla
    if (title && strlen(title) > 0) {
        hb->titleLabel = lv_label_create(hb->container);
        lv_label_set_text(hb->titleLabel, title);
        lv_obj_set_style_text_color(hb->titleLabel, DefaultTheme::getTextColor(), 0);
        lv_obj_set_style_text_font(hb->titleLabel, &lv_font_montserrat_16, 0);
        
        if (showBackButton) {
            // Alinear al centro si hay botón de volver
            lv_obj_align(hb->titleLabel, LV_ALIGN_CENTER, 0, 0);
        } else {
            // Alinear a la izquierda si no hay botón de volver
            lv_obj_align(hb->titleLabel, LV_ALIGN_LEFT_MID, 12, 0);
        }
    }

    // Componentes del Carrito
    if (showCartButton) {
        hb->cartButton = lv_button_create(hb->container);
        lv_obj_set_size(hb->cartButton, 44, 30);
        lv_obj_align(hb->cartButton, LV_ALIGN_RIGHT_MID, showStatus ? -150 : -8, 0);
        DefaultTheme::applyButton(hb->cartButton, 10);
        lv_obj_add_event_cb(hb->cartButton, cart_btn_cb, LV_EVENT_CLICKED, nullptr);

        lv_obj_t * cartIcon = lv_label_create(hb->cartButton);
        lv_label_set_text(cartIcon, LV_SYMBOL_LIST); 
        lv_obj_set_style_text_color(cartIcon, DefaultTheme::getPrimaryAccent(), 0);
        lv_obj_center(cartIcon);
        
        hb->cartBadge = lv_obj_create(hb->cartButton);
        lv_obj_set_size(hb->cartBadge, 18, 18);
        lv_obj_align(hb->cartBadge, LV_ALIGN_TOP_RIGHT, 8, -8);
        DefaultTheme::applySunkenCard(hb->cartBadge, 9);
        lv_obj_set_style_bg_color(hb->cartBadge, DefaultTheme::getPrimaryAccent(), 0);
        lv_obj_set_style_border_width(hb->cartBadge, 0, 0);
        lv_obj_set_style_pad_all(hb->cartBadge, 0, 0);
        DefaultTheme::disableScroll(hb->cartBadge);
        lv_obj_remove_flag(hb->cartBadge, LV_OBJ_FLAG_CLICKABLE);
        
        hb->cartBadgeLbl = lv_label_create(hb->cartBadge);
        lv_label_set_text(hb->cartBadgeLbl, "0");
        lv_obj_set_style_text_color(hb->cartBadgeLbl, lv_color_hex(0x0F172A), 0);
        lv_obj_set_style_text_font(hb->cartBadgeLbl, &lv_font_montserrat_12, 0);
        lv_obj_center(hb->cartBadgeLbl);
        
        hb->updateCart(0);
    }

    // Componentes de Estado (Wi-Fi, Batería, Hora)
    if (showStatus) {
        // Contenedor de señal Wi-Fi (Derecha al borde)
        hb->signalContainer = lv_obj_create(hb->container);
        lv_obj_set_size(hb->signalContainer, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_align(hb->signalContainer, LV_ALIGN_RIGHT_MID, -12, 0);
        lv_obj_set_style_bg_opa(hb->signalContainer, 0, 0);
        lv_obj_set_style_border_width(hb->signalContainer, 0, 0);
        lv_obj_set_style_pad_all(hb->signalContainer, 0, 0);
        lv_obj_set_flex_flow(hb->signalContainer, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(hb->signalContainer, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(hb->signalContainer, 3, 0);
        DefaultTheme::disableScroll(hb->signalContainer);

        hb->signalIcon = lv_label_create(hb->signalContainer);
        lv_label_set_text(hb->signalIcon, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_font(hb->signalIcon, &lv_font_montserrat_24, 0);

        hb->signalBadge = lv_label_create(hb->signalContainer);
        lv_label_set_text(hb->signalBadge, LV_SYMBOL_CLOSE);
        lv_obj_set_style_text_font(hb->signalBadge, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(hb->signalBadge, lv_color_hex(0xEF4444), 0);
        lv_obj_add_flag(hb->signalBadge, LV_OBJ_FLAG_HIDDEN);

        // Reloj (Centro) - Al tocar abre el Modal de Diagnóstico
        hb->timeLabel = lv_label_create(hb->container);
        lv_label_set_text(hb->timeLabel, "--:--");
        lv_obj_align(hb->timeLabel, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_text_color(hb->timeLabel, DefaultTheme::getTextColor(), 0);
        lv_obj_set_style_text_font(hb->timeLabel, &lv_font_montserrat_16, 0);
        lv_obj_add_flag(hb->timeLabel, LV_OBJ_FLAG_CLICKABLE);
        // Cargar los últimos valores guardados
        hb->updateTime(lastTimeStr);
        hb->updateSignal(lastSignalStrength);
        setActiveHeader(hb);
    }
    return hb;
}

HeaderBar::~HeaderBar() {
    if (activeHeader == this) {
        activeHeader = nullptr;
    }
}

void HeaderBar::setActiveHeader(HeaderBar* header) {
    activeHeader = header;
    if (activeHeader && activeHeader->container && lv_obj_is_valid(activeHeader->container)) {
        activeHeader->updateTime(lastTimeStr);
        activeHeader->updateBattery(lastBatteryPercentage);
        int currentRssi = SystemStateAPI::isWifiConnected() ? SystemStateAPI::getWifiRSSI() : -999;
        activeHeader->updateSignal(currentRssi);
        if (activeHeader->cartButton) {
            activeHeader->updateCart(0);
        }
    }
}

void HeaderBar::updateActiveTime(const char* timeStr) {
    if (timeStr) {
        std::strncpy(lastTimeStr, timeStr, sizeof(lastTimeStr) - 1);
        lastTimeStr[sizeof(lastTimeStr) - 1] = '\0';
    }
    if (activeHeader && activeHeader->container && lv_obj_is_valid(activeHeader->container)) {
        activeHeader->updateTime(timeStr);
    }
}

void HeaderBar::updateActiveBattery(int percentage) {
    if (lastBatteryPercentage == percentage) return;
    lastBatteryPercentage = percentage;
    if (activeHeader && activeHeader->container && lv_obj_is_valid(activeHeader->container)) {
        activeHeader->updateBattery(percentage);
    }
}

void HeaderBar::updateActiveSignal(int strength) {
    lastSignalStrength = strength;
    if (activeHeader && activeHeader->container && lv_obj_is_valid(activeHeader->container)) {
        activeHeader->updateSignal(strength);
    }
}

void HeaderBar::updateActiveCart() {
    int count = 0;
    if (lastCartCount == count) return;
    lastCartCount = count;
    if (activeHeader && activeHeader->container && lv_obj_is_valid(activeHeader->container)) {
        if (activeHeader->cartButton) {
            activeHeader->updateCart(count);
        }
    }
}

void HeaderBar::updateTime(const char* timeStr) {
    if (timeLabel && timeStr) {
        lv_label_set_text(timeLabel, timeStr);
    }
}

void HeaderBar::updateBattery(int percentage) {
}

void HeaderBar::updateSignal(int strength) {
    if (!signalIcon) return;

    bool disconnected = (strength == -999);
    uint32_t colorHex = 0x10B981; // Green

    if (!disconnected && strength <= 0) {
        if (strength < -80) {
            colorHex = 0xEF4444; // Red
        } else if (strength < -70) {
            colorHex = 0xF59E0B; // Yellow
        }
    } else if (!disconnected && strength > 0) {
        if (strength <= 30) {
            colorHex = 0xEF4444; // Red
        } else if (strength <= 65) {
            colorHex = 0xF59E0B; // Yellow
        }
    }

    if (disconnected == appliedDisconnected && colorHex == appliedColorHex) {
        return; // No visible color/state change on this HeaderBar instance
    }
    appliedDisconnected = disconnected;
    appliedColorHex = colorHex;

    lv_color_t color = lv_color_hex(colorHex);

    if (disconnected) {
        lv_label_set_text(signalIcon, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_color(signalIcon, lv_color_hex(0x666666), 0);
        if (signalBadge) {
            lv_label_set_text(signalBadge, LV_SYMBOL_CLOSE);
            lv_obj_set_style_text_color(signalBadge, lv_color_hex(0xEF4444), 0);
            lv_obj_clear_flag(signalBadge, LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        if (signalBadge) {
            lv_obj_add_flag(signalBadge, LV_OBJ_FLAG_HIDDEN);
        }
        lv_label_set_text(signalIcon, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_color(signalIcon, color, 0);
    }
}

void HeaderBar::back_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        UIManager::getInstance().loadLauncher();
    }
}

void HeaderBar::updateCart(int count) {
    if (!cartBadgeLbl || !lv_obj_is_valid(cartBadgeLbl)) return;
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", count);
    lv_label_set_text(cartBadgeLbl, buf);

    if (count > 0) {
        lv_obj_remove_flag(cartBadge, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_opa(cartButton, LV_OPA_COVER, 0);
    } else {
        lv_obj_add_flag(cartBadge, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_opa(cartButton, LV_OPA_70, 0);
    }
}

void HeaderBar::cart_btn_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        UIManager::getInstance().loadMediaGallery();
    }
}

#include "QuickSettingsPanel.h"

void HeaderBar::status_tap_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        QuickSettingsPanel::toggle(lv_screen_active());
    }
}
