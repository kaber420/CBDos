#pragma once
#include <lvgl.h>

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

    void resetInactivityTimer() {}
    static void showToast(const char* message);

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
