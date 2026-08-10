#pragma once
#include <lvgl.h>
#include <cstddef>

class TlvBrowserView {
public:
    static lv_obj_t* create();
    // Renderiza bytes TLV compilados dentro del área de contenido
    static void render(const uint8_t* data, size_t length);
    // Renderiza la página de plantilla base (demo offline)
    static void renderDemo();

private:
    static lv_obj_t* contentArea;
};