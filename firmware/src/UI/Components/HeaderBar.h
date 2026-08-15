#pragma once
#include <lvgl.h>

struct HeaderBarConfig {
    const char* title = nullptr;
    bool showBackButton = true;
    bool showTime = true;
    bool showWifi = true;
    bool showCartButton = false;
    bool titleMarquee = false;
    bool translucent = false;
    lv_event_cb_t onBackClick = nullptr;
};

class HeaderBar {
public:
    // Crea una barra de cabecera en el contenedor 'parent'
    static HeaderBar* create(lv_obj_t* parent, const HeaderBarConfig& config);
    static HeaderBar* create(lv_obj_t* parent, const char* title, bool showBackButton, bool showStatus, bool showCartButton = false);
    ~HeaderBar();

    // Establece el header activo para dirigir las actualizaciones globales
    static void setActiveHeader(HeaderBar* header);
    
    // Métodos estáticos para actualizar el header actualmente activo (llamados desde main.cpp / UIManager)
    static void updateActiveTime(const char* timeStr);
    static void updateActiveBattery(int percentage);
    static void updateActiveSignal(int strength);
    static void updateActiveCart();

    // Métodos de instancia para actualización de widgets LVGL internos
    void updateTime(const char* timeStr);
    void updateBattery(int percentage);
    void updateSignal(int strength);
    void updateCart(int count);

private:
    HeaderBar() = default;

    lv_obj_t* container = nullptr;
    lv_obj_t* backButton = nullptr;
    lv_obj_t* titleLabel = nullptr;
    
    // Componentes del Carrito
    lv_obj_t* cartButton = nullptr;
    lv_obj_t* cartBadge = nullptr;
    lv_obj_t* cartBadgeLbl = nullptr;
    
    // Componentes del Status Bar
    lv_obj_t* timeLabel = nullptr;
    lv_obj_t* batteryShell = nullptr;
    lv_obj_t* batteryFill = nullptr;
    lv_obj_t* batteryTip = nullptr;
    lv_obj_t* signalContainer = nullptr;
    lv_obj_t* signalIcon = nullptr;
    lv_obj_t* signalBadge = nullptr;

    bool appliedDisconnected = true;
    uint32_t appliedColorHex = 0;

    // Estado global compartido
    static HeaderBar* activeHeader;
    static char lastTimeStr[16];
    static int lastBatteryPercentage;
    static int lastSignalStrength;
    static int lastCartCount;

    static void back_event_cb(lv_event_t * e);
    static void cart_btn_cb(lv_event_t * e);
    static void status_tap_cb(lv_event_t * e);
};
