#pragma once

#include "BaseView.hpp"
#include <vector>
#include <string>
#include <lvgl.h>

namespace cbdos {
namespace ui {

struct GalleryMediaItem {
    std::string id;
    std::string name;
    std::string folder;
    std::string path;
    bool isGif;
    const char* icon;

    GalleryMediaItem(const std::string& i, const std::string& n, const std::string& f, const std::string& p, bool gif, const char* ic)
        : id(i), name(n), folder(f), path(p), isGif(gif), icon(ic) {}
};

struct GalleryCategory {
    int id;
    std::string name;
    const char* icon;

    GalleryCategory(int i, const std::string& n, const char* ic)
        : id(i), name(n), icon(ic) {}
};

class GalleryListView : public BaseView {
public:
    GalleryListView();
    virtual ~GalleryListView() = default;

    bool onCreate(lv_obj_t* parent) override;
    void onDestroy() override;
    void onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) override;

    void refreshGallery();

private:
    void loadDataFromSD();
    void buildFilteredIndices();
    void renderCategories();
    void renderMedia();
    void navigateTo(int index);
    void updateNavButtons();

    static void categoryBtnCb(lv_event_t* e);
    static void mediaCardCb(lv_event_t* e);
    static void prevBtnCb(lv_event_t* e);
    static void nextBtnCb(lv_event_t* e);

    lv_obj_t* m_categoriesContainer = nullptr;
    lv_obj_t* m_mediaContainer = nullptr;

    // Tarjeta central
    lv_obj_t* m_card = nullptr;
    lv_obj_t* m_imgArea = nullptr;
    lv_obj_t* m_iconLbl = nullptr;
    lv_obj_t* m_imgObj = nullptr;
    lv_obj_t* m_gifObj = nullptr;
    lv_obj_t* m_nameLbl = nullptr;
    lv_obj_t* m_pathLbl = nullptr;

    // Navegación
    lv_obj_t* m_prevBtn = nullptr;
    lv_obj_t* m_nextBtn = nullptr;
    lv_obj_t* m_pageIndicatorLbl = nullptr;

    int m_currentCategoryId = 0;
    int m_currentItemIndex = 0;

    std::vector<GalleryCategory> m_categories;
    std::vector<GalleryMediaItem> m_mediaItems;
    std::vector<int> m_filteredIndices;
};

} // namespace ui
} // namespace cbdos
