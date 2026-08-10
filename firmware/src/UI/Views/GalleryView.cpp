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

    // Contenedor Visor
    lv_obj_t* container = lv_obj_create(scr);
    lv_obj_set_size(container, LV_PCT(95), LV_PCT(78));
    lv_obj_align(container, LV_ALIGN_CENTER, 0, 15);
    DefaultTheme::applyRaisedCard(container);

    // Objeto Imagen LVGL
    lv_obj_t* imgObj = lv_image_create(container);
    lv_image_set_src(imgObj, imagePath.c_str());
    lv_obj_center(imgObj);

    return scr;
}
