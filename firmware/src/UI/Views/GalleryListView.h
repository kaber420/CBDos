#pragma once
#include <lvgl.h>
#include <vector>
#include <string>
#include "../Components/HeaderBar.h"

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

class GalleryListView {
public:
    static lv_obj_t* create();
    static HeaderBar* getHeaderBar() { return headerBar; }
    static void refreshGallery();

private:
    static HeaderBar*  headerBar;
    static lv_obj_t*   categoriesContainer;
    static lv_obj_t*   mediaContainer;
    static int         currentCategoryId;
    static int         currentItemIndex;

    static std::vector<GalleryCategory>  categories;
    static std::vector<GalleryMediaItem> mediaItems;
    static std::vector<int>              filteredIndices;

    // Widgets de la tarjeta central
    static lv_obj_t* card;
    static lv_obj_t* imgArea;
    static lv_obj_t* iconLbl;
    static lv_obj_t* imgObj;
    static lv_obj_t* gifObj;
    static lv_obj_t* nameLbl;
    static lv_obj_t* pathLbl;

    // Navegación
    static lv_obj_t* prevBtn;
    static lv_obj_t* nextBtn;
    static lv_obj_t* pageIndicatorLbl;

    // Estilos
    static lv_style_t style_raised_card;
    static lv_style_t style_sunken_area;
    static bool       stylesInitialized;

    static void loadDataFromSD();
    static void renderCategories();
    static void renderMedia();
    static void buildFilteredIndices();
    static void navigateTo(int index);
    static void updateNavButtons();

    static void category_btn_cb(lv_event_t* e);
    static void media_card_cb(lv_event_t* e);
    static void prev_btn_cb(lv_event_t* e);
    static void next_btn_cb(lv_event_t* e);
};
