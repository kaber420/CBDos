#include <lvgl.h>
#include "UI/UIManager.h"
#include "UI/Views/DashboardView.h"
#include "UI/Components/HeaderBar.h"
#include "UI/Themes/DefaultTheme.h"
#include "UI/WallpaperManager.h"
#include "Network/ConfigManager.h"
#include "Network/DiscoveryService.h"
#include "Network/MQTTService.h"
#include "Core/SystemDiagnostics.h"
#include "Core/LVFS_Driver.h"
#include "Core/StorageManager.h"
#include "Network/AssetManager.h"
#include "Network/TlvNetworkClient.h"

#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
#include <unistd.h>
#else
#include <Arduino.h>
#include <WiFi.h>
#include <SPI.h>
#include <SD.h>
#include <JC3248W535.h>

JC3248W535_Display displayDriver;
JC3248W535_Touch touchDriver;

void my_disp_flush(lv_display_t *display, const lv_area_t *area, uint8_t *px_map) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    
    // Corregir colores (endianness) y escribir DIRECTAMENTE a la pantalla de hardware
    // Eliminado lv_draw_sw_rgb565_swap para ahorrar CPU; LVGL ya lo hace internamente (LV_COLOR_16_SWAP 1)

    if (displayDriver.getCanvas()) {
        displayDriver.getCanvas()->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
        displayDriver.flush();
    }
    lv_display_flush_ready(display);
}

static bool was_pressed = false;
static TouchPoint last_point;

void my_touchpad_read(lv_indev_t * indev, lv_indev_data_t * data) {
    TouchPoint tp;
    if (touchDriver.read(tp) && tp.touched) {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = tp.x;
        data->point.y = tp.y;
        last_point = tp;
        was_pressed = true;
    } else {
        data->state = LV_INDEV_STATE_REL;
        if (was_pressed) {
            data->point.x = last_point.x;
            data->point.y = last_point.y;
            was_pressed = false;
        }
    }
}
#endif

static constexpr uint32_t LVGL_TASK_PERIOD_MS = 5;

static bool g_isConfigured = false;
static bool g_sdMounted = false;

void commandCallback(int cmdId) {
}

void ordersCallback(const std::vector<OrderItem>& orders) {
}

static WiFiConfig g_wifiConfig;
static GatewayConfig g_activeGateway;
static bool g_hasWifi = false;
static bool g_hasGateway = false;

static volatile int g_currentRssi = -999;

#include "Core/SystemStateAPI.h"

#ifdef ARDUINO
static void networkTask(void* param) {
    while (1) {
        if (WiFi.status() == WL_CONNECTED) {
            static uint32_t lastRssiCheck = 0;
            uint32_t now = millis();
            if (now - lastRssiCheck >= 5000) {
                lastRssiCheck = now;
                g_currentRssi = WiFi.RSSI();
                SystemStateAPI::updateWifi(true, g_currentRssi);
            }
            if (g_hasGateway) {
                MQTTService::getInstance().update();
            }
            AssetManager::getInstance().processQueue();
        } else {
            g_currentRssi = -999;
            SystemStateAPI::updateWifi(false, -999);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
#endif

void startNormalBoot() {
    Serial.println("[OK] Starting normal boot...");
    String mac;
#ifdef ARDUINO
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    mac = WiFi.macAddress();
    if (g_hasWifi && g_wifiConfig.ssid.length() > 0) {
        Serial.printf("[WiFi] Conectando a SSID: %s...\n", g_wifiConfig.ssid.c_str());
        if (g_wifiConfig.useStaticIp) {
            IPAddress ip, gw, sub, dns1;
            if (ip.fromString(g_wifiConfig.staticIp.c_str()) && gw.fromString(g_wifiConfig.gateway.c_str())) {
                sub.fromString(g_wifiConfig.subnet.length() > 0 ? g_wifiConfig.subnet.c_str() : "255.255.255.0");
                if (g_wifiConfig.dns1.length() > 0) dns1.fromString(g_wifiConfig.dns1.c_str());
                WiFi.config(ip, gw, sub, dns1);
            }
        }
        WiFi.begin(g_wifiConfig.ssid.c_str(), g_wifiConfig.password.c_str());
    } else {
        Serial.println("[WiFi] No hay SSID configurado.");
    }
    // Lanzar tarea de red aislada en Core 0 con 12KB de stack para evitar Stack Overflow
    xTaskCreatePinnedToCore(networkTask, "NetworkTask", 12288, NULL, 1, NULL, 0);
#else
    mac = "00:11:22:33:44:55";
#endif
    if (g_hasGateway) {
        MQTTService::getInstance().init(g_activeGateway, mac);
    }
    UIManager::getInstance().loadLauncher();
}

static void accept_btn_cb(lv_event_t * e) {
    lv_obj_t * mbox = (lv_obj_t*)lv_event_get_user_data(e);
    if(lv_obj_is_valid(mbox)) lv_obj_delete_async(mbox);
}

static void cancel_btn_cb(lv_event_t * e) {
    lv_obj_t * mbox = (lv_obj_t*)lv_event_get_user_data(e);
    if(lv_obj_is_valid(mbox)) lv_obj_delete_async(mbox);
    startNormalBoot();
}

static void showSystemModal(const char* titleText, const char* msgText, bool showButtons) {
    lv_obj_t* mask = lv_obj_create(lv_screen_active());
    lv_obj_add_flag(mask, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_size(mask, 320, 480);
    lv_obj_set_style_bg_color(mask, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(mask, LV_OPA_80, 0);
    lv_obj_set_style_border_width(mask, 0, 0);

    lv_obj_t* modal = lv_obj_create(mask);
    lv_obj_set_width(modal, 280);
    lv_obj_set_height(modal, LV_SIZE_CONTENT);
    DefaultTheme::applyRaisedCard(modal, 20);
    lv_obj_center(modal);
    lv_obj_set_flex_flow(modal, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(modal, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(modal, 20, 0);

    lv_obj_t* title = lv_label_create(modal);
    lv_label_set_text(title, titleText);
    lv_obj_set_style_text_color(title, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);

    lv_obj_t* msg = lv_label_create(modal);
    lv_label_set_text(msg, msgText);
    lv_label_set_long_mode(msg, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(msg, 240);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(msg, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_margin_top(msg, 10, 0);

    if (showButtons) {
        lv_obj_t* btnCont = lv_obj_create(modal);
        lv_obj_set_width(btnCont, 260);
        lv_obj_set_height(btnCont, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(btnCont, 0, 0);
        lv_obj_set_style_border_width(btnCont, 0, 0);
        lv_obj_set_flex_flow(btnCont, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(btnCont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(btnCont, 0, 0);
        lv_obj_set_style_margin_top(btnCont, 20, 0);

        lv_obj_t* btn_accept = lv_button_create(btnCont);
        lv_obj_set_size(btn_accept, 110, 40);
        DefaultTheme::applyButton(btn_accept, 12);
        lv_obj_set_style_bg_color(btn_accept, DefaultTheme::getPrimaryAccent(), 0);
        lv_obj_t* lblA = lv_label_create(btn_accept);
        lv_label_set_text(lblA, "Aceptar");
        lv_obj_set_style_text_color(lblA, lv_color_hex(0x000000), 0);
        lv_obj_center(lblA);
        lv_obj_add_event_cb(btn_accept, accept_btn_cb, LV_EVENT_CLICKED, mask);

        lv_obj_t* btn_cancel = lv_button_create(btnCont);
        lv_obj_set_size(btn_cancel, 110, 40);
        DefaultTheme::applyButton(btn_cancel, 12);
        lv_obj_t* lblC = lv_label_create(btn_cancel);
        lv_label_set_text(lblC, "Cancelar");
        lv_obj_set_style_text_color(lblC, DefaultTheme::getTextColor(), 0);
        lv_obj_center(lblC);
        lv_obj_add_event_cb(btn_cancel, cancel_btn_cb, LV_EVENT_CLICKED, mask);
    }
}

void setup() {
#ifdef ARDUINO
    Serial.begin(115200);
    delay(500); // Dar tiempo al Serial para conectar
    Serial.println("\n=== CBDos Firmware Booting ===");
    Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());
    Serial.printf("PSRAM size: %u bytes\n", ESP.getPsramSize());
    Serial.printf("Free PSRAM: %u bytes\n", ESP.getFreePsram());
#endif
    lv_init();
    Serial.println("[OK] lv_init done");

#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
    lv_display_t * disp = lv_sdl_window_create(480, 320);
    lv_indev_t * mouse = lv_sdl_mouse_create();
#else
    // Hardware init para JC3248W535
    Serial.println("[..] Initializing display...");
    if (!displayDriver.begin()) {
        Serial.println("[FAIL] displayDriver.begin() FAILED! Check PSRAM and wiring.");
        // Parpadear el backlight como señal de error
        pinMode(1, OUTPUT);
        while(1) {
            digitalWrite(1, HIGH); delay(200);
            digitalWrite(1, LOW);  delay(200);
        }
    }
    Serial.println("[OK] Display initialized");
    displayDriver.backlightOn(); // Asegurar backlight encendido explícitamente
    // NO rotamos el hardware (output_display), lo dejamos en ROTATION_0 (320x480)
    // displayDriver.setRotation(ROTATION_90);

    Serial.println("[..] Initializing touch...");
    touchDriver.begin();
    displayDriver.setTouchRotation(&touchDriver);
    Serial.println("[OK] Touch initialized");

    // Creamos la pantalla en su resolución física real (Vertical 320x480)
    lv_display_t * disp = lv_display_create(320, 480);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(disp, my_disp_flush);
    
    // El usuario quiere el sistema en VERTICAL, por lo tanto la rotación es 0 (Portrait)
    lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_0);
    
    // Asignar memoria para el buffer de pantalla completa en PSRAM externa (307 KB)
    uint32_t buf_size = 320 * 480 * 2;
    uint8_t *buf = (uint8_t *)ps_malloc(buf_size);
    if (!buf) {
        Serial.println("[WARN] ps_malloc failed, falling back to malloc");
        buf_size = 480 * 40 * 2;
        buf = (uint8_t *)malloc(buf_size);
        if (!buf) {
            Serial.println("[FAIL] malloc also failed! Out of memory.");
            while(1) delay(1000);
        }
    } else {
        Serial.printf("[OK] LVGL buffer allocated in PSRAM: %u bytes\n", buf_size);
    }
    lv_display_set_buffers(disp, buf, NULL, buf_size, LV_DISPLAY_RENDER_MODE_FULL);

    lv_indev_t * indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);
    lv_timer_t * indev_timer = lv_indev_get_read_timer(indev);
    if (indev_timer) {
        lv_timer_set_period(indev_timer, 10); // Polling táctil a 100Hz (10ms)
    }
    Serial.println("[OK] LVGL display and input configured");
#endif

    UIManager::getInstance().init();
    Serial.println("[OK] UIManager initialized");
    
    DashboardView::setCommandCallback(commandCallback);
    MQTTService::getInstance().setOrdersCallback(ordersCallback);

#ifdef ARDUINO
    SPIClass* sdSPI = new SPIClass(HSPI);
    sdSPI->begin(12, 13, 11, 10); // SCK, MISO, MOSI, SS (HSPI / SPI3_HOST para no colisionar con FSPI de la pantalla)
    bool sdMounted = SD.begin(10, *sdSPI, 10000000); // Intento 1: 10MHz
    if (!sdMounted) {
        Serial.println("[WARN] SD 10MHz fallo, reintentando a 4MHz (modo compatibilidad SDHC 16GB)...");
        sdMounted = SD.begin(10, *sdSPI, 4000000); // Intento 2: 4MHz (Compatibilidad universal SDHC)
    }
    g_sdMounted = sdMounted;
    if (!sdMounted) {
        Serial.println("[WARN] SD.begin() failed");
    } else {
        // Crear el Mutex SPI global para proteger accesos concurrentes a la SD
        SemaphoreHandle_t spiMutex = xSemaphoreCreateMutex();
        lv_fs_set_spi_mutex(spiMutex);
        Serial.println("[OK] SPI Mutex configurado");
        
        lv_fs_if_init();
        AssetManager::getInstance().init();
    }
    StorageManager::init();
#endif

    Serial.println("[..] Loading config...");
    ConfigManager::getInstance().init();
    WallpaperManager::getInstance().init();
    g_hasWifi = ConfigManager::getInstance().loadWiFi(g_wifiConfig);
    g_hasGateway = ConfigManager::getInstance().loadActiveGateway(g_activeGateway);
    g_isConfigured = g_hasWifi && g_hasGateway;

    startNormalBoot();
    lv_refr_now(NULL);
    Serial.println("=== Boot complete ===");
}

void loop() {
#ifdef ARDUINO
    static uint32_t last_tick = millis();
    uint32_t current_tick = millis();
    if (current_tick > last_tick) {
        lv_tick_inc(current_tick - last_tick);
    }
    last_tick = current_tick;
#else
    lv_tick_inc(5);
#endif
    uint32_t time_till_next = lv_timer_handler();
    UIManager::getInstance().update();
    TlvNetworkClient::loop();

    static uint32_t lastBadgeCheck = 0;
    uint32_t now = millis();
    if (now - lastBadgeCheck >= 5000) {
        lastBadgeCheck = now;
        int rssi = SystemStateAPI::isWifiConnected() ? SystemStateAPI::getWifiRSSI() : -999;
        HeaderBar::updateActiveSignal(rssi);
    }
    if (g_isConfigured) {
        if (AssetManager::getInstance().checkAndClearRefreshFlag()) {
            lv_obj_send_event(lv_screen_active(), LV_EVENT_REFRESH, NULL);
        }
    }

#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
    if (time_till_next == 0 || time_till_next > 5) time_till_next = 5;
    usleep(time_till_next * 1000);
#else
    if (time_till_next == 0 || time_till_next > 5) time_till_next = 5;
    delay(time_till_next);
#endif
}

#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
int main(int argc, char **argv) {
    setup();
    while (1) {
        loop();
    }
    return 0;
}
#endif

SystemDiagnostics getSystemDiagnostics() {
    SystemDiagnostics diag;
    memset(&diag, 0, sizeof(diag));
    diag.sdMounted = g_sdMounted;
#ifdef ARDUINO
    diag.sdHasConfigFile = g_sdMounted && SD.exists("/tablehub.enc");
    diag.wifiConnected = (WiFi.status() == WL_CONNECTED);
    if (diag.wifiConnected) {
        strncpy(diag.wifiSsid, WiFi.SSID().c_str(), sizeof(diag.wifiSsid) - 1);
        strncpy(diag.ipAddress, WiFi.localIP().toString().c_str(), sizeof(diag.ipAddress) - 1);
        diag.rssi = WiFi.RSSI();
    }
    strncpy(diag.macAddress, WiFi.macAddress().c_str(), sizeof(diag.macAddress) - 1);
    diag.freeHeap = ESP.getFreeHeap();
    diag.freePsram = ESP.getFreePsram();
    diag.uptimeSeconds = millis() / 1000;
#else
    diag.sdHasConfigFile = false;
    diag.wifiConnected = true;
    strncpy(diag.wifiSsid, "MockWiFi", sizeof(diag.wifiSsid) - 1);
    strncpy(diag.ipAddress, "192.168.1.100", sizeof(diag.ipAddress) - 1);
    strncpy(diag.macAddress, "00:11:22:33:44:55", sizeof(diag.macAddress) - 1);
    diag.rssi = -50;
    diag.freeHeap = 250000;
    diag.freePsram = 4000000;
    diag.uptimeSeconds = 120;
#endif
    strncpy(diag.hubIp, DiscoveryService::getInstance().getDiscoveredHubIp().c_str(), sizeof(diag.hubIp) - 1);
    diag.mqttConnected = MQTTService::getInstance().isConnected();
    return diag;
}