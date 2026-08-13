#pragma once
#include <lvgl.h>
#include <vector>
#include <string>
#include "../Components/HeaderBar.h"

struct MenuItem {
    std::string id;
    std::string name;
    std::string description;
    float price;
    int categoryId;
    std::string imageHash;
    const char* icon;

    MenuItem(const std::string& i, const std::string& n, const std::string& d, float p, int c, const std::string& hash, const char* ic)
        : id(i), name(n), description(d), price(p), categoryId(c), imageHash(hash), icon(ic) {}
};

struct Category {
    int id;
    std::string name;
    std::string iconStr;
    const char* icon;

    Category(int i, const std::string& n, const std::string& is, const char* ic)
        : id(i), name(n), iconStr(is), icon(ic) {}
};

class MenuView {
public:
    static lv_obj_t* create();
    static HeaderBar* getHeaderBar() { return headerBar; }
    static std::string resolveSdidxPath(const std::string& path);
    
    // Versión activa del catálogo en RAM
    static std::string getCurrentCatalogVersion() { return currentCatalogVersion; }
    static void setCurrentCatalogVersion(const std::string& ver) { currentCatalogVersion = ver; }
    static void refreshCatalog() {
        categories.clear();
        menuItems.clear();
        loadDataFromSD();
        renderCategories();
        renderProducts();
    }

private:
    static HeaderBar*  headerBar;
    static lv_obj_t*   categoriesContainer;
    static lv_obj_t*   productsContainer;
    static int         currentCategoryId;
    static int         currentItemIndex;

    static std::vector<Category> categories;
    static std::vector<MenuItem> menuItems;
    static std::vector<int>      filteredIndices;
    static std::string           currentCatalogVersion;


    // ── Widgets de la tarjeta única ──────────────────────────────
    static lv_obj_t* card;
    static lv_obj_t* imgArea;
    static lv_obj_t* iconLbl;
    static lv_obj_t* imgObj;
    static lv_obj_t* nameLbl;
    static lv_obj_t* priceLbl;

    // ── Navegación ───────────────────────────────────────────────
    static lv_obj_t* prevBtn;
    static lv_obj_t* nextBtn;
    static lv_obj_t* pageIndicatorLbl;

    // ── Estilos estáticos ────────────────────────────────────────
    static lv_style_t style_raised_card;
    static lv_style_t style_sunken_area;
    static bool       stylesInitialized;

    static void loadDataFromSD();
    static void renderCategories();
    static void renderProducts();
    static void buildFilteredIndices();
    static void navigateTo(int index);
    static void updateNavButtons();

    static void category_btn_cb(lv_event_t* e);
    static void product_card_cb(lv_event_t* e);
    static void prev_btn_cb(lv_event_t* e);
    static void next_btn_cb(lv_event_t* e);
    static void showProductModal(const MenuItem& item);
    static void showFullScreenImage(const std::string& path);
};
