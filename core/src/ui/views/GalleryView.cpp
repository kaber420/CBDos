#include "GalleryView.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include <algorithm>
#include <cctype>

namespace cbdos {
namespace ui {

GalleryView::GalleryView(const std::string& imagePath, const std::string& imageName)
    : BaseView("Visor"), m_imagePath(imagePath), m_imageName(imageName) {
}

bool GalleryView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;

    // Resolver ruta para el driver virtual de LVGL (Unidad 'A:')
    std::string resolvedPath = m_imagePath;
    if (resolvedPath.rfind("A:", 0) != 0 && resolvedPath.rfind("a:", 0) != 0) {
        if (resolvedPath.rfind("/sdcard/", 0) == 0) {
            resolvedPath = "A:/" + resolvedPath.substr(8);
        } else if (resolvedPath.rfind("/", 0) == 0) {
            resolvedPath = "A:" + resolvedPath;
        } else {
            resolvedPath = "A:/" + resolvedPath;
        }
    }

    // 1. Configurar HeaderBar flotante para el visor
    UIManager::getInstance().getHeaderBar().showWifi(false);
    UIManager::getInstance().getHeaderBar().setTitle(m_imageName.c_str());
    UIManager::getInstance().getHeaderBar().showBackButton(true, []() {
        UIManager::getInstance().popView();
    });

    // 2. Contenedor visor a pantalla completa con fondo negro
    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_align(m_container, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(m_container, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(m_container, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(m_container, 0, 0);
    lv_obj_set_style_pad_all(m_container, 0, 0);

    // Habilitar desplazamiento multidireccional (scroll libre) si la imagen supera la pantalla
    lv_obj_add_flag(m_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(m_container, LV_DIR_ALL);
    lv_obj_set_scrollbar_mode(m_container, LV_SCROLLBAR_MODE_AUTO);

    // 3. Detectar si es un GIF animado
    std::string lowerPath = resolvedPath;
    std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);
    bool isGif = (lowerPath.length() >= 4 && lowerPath.substr(lowerPath.length() - 4) == ".gif");

    m_imgObj = nullptr;
    if (isGif) {
#if LV_USE_GIF
        m_imgObj = lv_gif_create(m_container);
        lv_gif_set_src(m_imgObj, resolvedPath.c_str());
#else
        m_imgObj = lv_image_create(m_container);
        lv_image_set_src(m_imgObj, resolvedPath.c_str());
#endif
    } else {
        m_imgObj = lv_image_create(m_container);
        lv_image_set_src(m_imgObj, resolvedPath.c_str());
    }

    if (m_imgObj) {
        lv_obj_set_size(m_imgObj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_center(m_imgObj);
    }

    return true;
}

void GalleryView::onDestroy() {
    m_imgObj = nullptr;
    UIManager::getInstance().getHeaderBar().showWifi(true);
    BaseView::onDestroy();
}

void GalleryView::onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) {
    (void)theme;
    (void)palette;
    if (m_container && lv_obj_is_valid(m_container)) {
        lv_obj_set_style_bg_color(m_container, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(m_container, LV_OPA_COVER, 0);
    }
}

} // namespace ui
} // namespace cbdos
