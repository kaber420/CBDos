#pragma once
#include <lvgl.h>
#include <string>
#include "../Core/StorageManager.h"

class UIManager {
public:
    static UIManager& getInstance() {
        static UIManager instance;
        return instance;
    }

    void init();
    void update();
    
    void loadLauncher();
    void loadMediaGallery();
    void loadMeshChat();
    void loadWavBrowser();
    void loadTlvBrowser();
    void loadConfigView();
    void loadWiFiConfig();
    void loadLoRaConfig();
    void loadFLRCConfig();
    void loadGatewayConfig();
    void loadMusicPlayer();
    void loadRadioPlayer();
    void loadImageViewer(const std::string& imagePath, const std::string& imageName);
    void loadWallpaperConfig();
    void loadUtilities();
    void loadFileManager();
    void loadDoom();
    void loadLuaRunner(const std::string& scriptPath = "");
    void loadTextEditor(const std::string& filePath = "", StorageType storage = StorageType::SD_CARD);

    void resetInactivityTimer() {}
    static void showToast(const char* message);
    static void attachKeyboard(lv_obj_t* ta);

private:
    UIManager() {}
    static void toast_timer_cb(lv_timer_t* timer);
    void destroyTransient();

    static lv_obj_t* toastObj;
    static lv_timer_t* toastTimer;
    
    // Pantalla permanente (Lanzador)
    lv_obj_t* launcherScreen = nullptr;

    // Pantalla transitoria activa
    lv_obj_t* currentTransientScreen = nullptr;
};
