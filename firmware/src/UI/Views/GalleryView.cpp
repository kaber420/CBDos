#include "GalleryView.h"
#include "../Themes/DefaultTheme.h"
#include "../UIManager.h"

HeaderBar*  GalleryView::headerBar        = nullptr;
std::string GalleryView::currentImagePath = "";
std::string GalleryView::currentImageName = "";

lv_obj_t* GalleryView::create(const std::string& imagePath, const std::string& imageName) {
    currentImagePath = imagePath;
    currentImageName = imageName;

    lv_obj_t* scr = lv_obj_create(NULL);
    DefaultTheme::applyFlatBg(scr);

    headerBar = HeaderBar::create(scr, ("Imagen: " + imageName).c_str(), true, true);
    HeaderBar::setActiveHeader(headerBar);

    // Contenedor visor — ocupa toda la pantalla debajo del header
    lv_obj_t* container = lv_obj_create(scr);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(82));
    lv_obj_align(container, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(container, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(container, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_pad_all(container, 0, 0);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);

    // Imagen — escalada a la pantalla para no agotar RAM
    lv_obj_t* imgObj = lv_image_create(container);
    lv_image_set_src(imgObj, imagePath.c_str());
    // Escalar para caber dentro de 320×390 px sin distorsionar
    lv_image_set_scale(imgObj, 256); // 256 = 100% (sin escala, LVGL ajusta si cabe)
    lv_obj_set_size(imgObj, LV_PCT(100), LV_PCT(100));
    lv_image_set_inner_align(imgObj, LV_IMAGE_ALIGN_CENTER);
    lv_obj_center(imgObj);

    return scr;
}
