#pragma once
#include <lvgl.h>
#include <cstddef>
#include <cstdint>
#include "../../Core/mesh_header.h"

class TlvBrowserView {
public:
    static lv_obj_t* create();
    
    // Renderiza directamente un buffer TLV binario plano
    static void render(const uint8_t* data, size_t length);
    
    // Procesa un paquete completo de la red mesh (MeshHeader + TLV)
    static void processNetworkPacket(const uint8_t* packet, size_t length);

    // Navega a una URL especificada codificando la trama de petición
    static void navigateToUrl(const char* url);

    // Renderiza la página de plantilla base (demo offline)
    static void renderDemo();

private:
    static lv_obj_t* contentArea;
    static lv_obj_t* urlInput;
    static lv_obj_t* keyboard;
    
    static void onUrlSubmit(lv_event_t* e);
    static void onUrlFocus(lv_event_t* e);
    static void onBookmarkClick(lv_event_t* e);
    static void onUplinkFrameGenerated(const uint8_t* frame, size_t len);
};