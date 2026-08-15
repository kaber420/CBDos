#include "GalleryView.h"
#include "MenuView.h"
#include "../Themes/DefaultTheme.h"
#include "../UIManager.h"

HeaderBar*  GalleryView::headerBar        = nullptr;
std::string GalleryView::currentImagePath = "";
std::string GalleryView::currentImageName = "";

lv_obj_t* GalleryView::create(const std::string& imagePath, const std::string& imageName) {
    currentImagePath = imagePath;
    currentImageName = imageName;

    std::string resolvedPath = MenuView::resolveSdidxPath(imagePath);
    if (resolvedPath.empty()) {
        resolvedPath = imagePath;
    }

    lv_obj_t* scr = lv_obj_create(NULL);
    DefaultTheme::applyFlatBg(scr);

    // Contenedor visor — ocupa toda la pantalla de fondo
    lv_obj_t* container = lv_obj_create(scr);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_align(container, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(container, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(container, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_pad_all(container, 0, 0);

    // Habilitar desplazamiento (scroll) para examinar la imagen completa si excede la pantalla
    lv_obj_add_flag(container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(container, LV_DIR_ALL);
    lv_obj_set_scrollbar_mode(container, LV_SCROLLBAR_MODE_AUTO);

    std::string lowerResolved = resolvedPath;
    for (auto &c : lowerResolved) c = tolower(c);
    bool isGif = (lowerResolved.length() >= 4 && lowerResolved.substr(lowerResolved.length() - 4) == ".gif");

    lv_obj_t* imgObj = nullptr;
    if (isGif) {
#if LV_USE_GIF
        imgObj = lv_gif_create(container);
        lv_gif_set_src(imgObj, resolvedPath.c_str());
#else
        imgObj = lv_image_create(container);
        lv_image_set_src(imgObj, resolvedPath.c_str());
#endif
    } else {
        imgObj = lv_image_create(container);
        lv_image_set_src(imgObj, resolvedPath.c_str());
    }

    lv_obj_set_size(imgObj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_center(imgObj);

    // Barra de cabecera flotante sobre la imagen con marquesina (después del botón Volver), sin wifi ni reloj
    HeaderBarConfig cfg;
    cfg.title = imageName.c_str();
    cfg.showBackButton = true;
    cfg.showTime = false;
    cfg.showWifi = false;
    cfg.showCartButton = false;
    cfg.titleMarquee = true;
    cfg.translucent = true;
    cfg.onBackClick = [](lv_event_t* e) {
        UIManager::getInstance().loadMediaGallery();
    };

    headerBar = HeaderBar::create(scr, cfg);
    HeaderBar::setActiveHeader(headerBar);

    return scr;
}
