#include "MenuView.h"
#include "../Themes/DefaultTheme.h"
#include "../../Network/AssetManager.h"
#include "../../Core/LVFS_Driver.h"
#include "../../Core/NativeAudioDriver.h"
#include "../UIManager.h"
#include <cstdio>
#include <ArduinoJson.h>
#ifdef ARDUINO
#include <SD.h>
#endif

// ────────────────────────────────────────────────────────────────
//  Definición de estáticos
// ────────────────────────────────────────────────────────────────
HeaderBar*  MenuView::headerBar           = nullptr;
lv_obj_t*   MenuView::categoriesContainer = nullptr;
lv_obj_t*   MenuView::productsContainer   = nullptr;
int         MenuView::currentCategoryId   = 0;
int         MenuView::currentItemIndex    = 0;

std::vector<Category> MenuView::categories;
std::vector<MenuItem> MenuView::menuItems;
std::vector<int>      MenuView::filteredIndices;
std::string           MenuView::currentCatalogVersion = "";

// Widgets tarjeta única
lv_obj_t* MenuView::card             = nullptr;
lv_obj_t* MenuView::imgArea          = nullptr;
lv_obj_t* MenuView::iconLbl          = nullptr;
lv_obj_t* MenuView::imgObj           = nullptr;
lv_obj_t* MenuView::nameLbl          = nullptr;
lv_obj_t* MenuView::priceLbl         = nullptr;

// Navegación
lv_obj_t* MenuView::prevBtn          = nullptr;
lv_obj_t* MenuView::nextBtn          = nullptr;
lv_obj_t* MenuView::pageIndicatorLbl = nullptr;

// Estilos
lv_style_t MenuView::style_raised_card;
lv_style_t MenuView::style_sunken_area;
bool       MenuView::stylesInitialized = false;

// ────────────────────────────────────────────────────────────────
//  Carga de datos
// ────────────────────────────────────────────────────────────────

void MenuView::loadDataFromSD() {
    menuItems.clear();
    categories.clear();

    categories = {
        {0, "Todas", "LV_SYMBOL_LIST", LV_SYMBOL_LIST},
        {1, "Fotos", "LV_SYMBOL_IMAGE", LV_SYMBOL_IMAGE},
        {2, "Musica", "LV_SYMBOL_AUDIO", LV_SYMBOL_AUDIO}
    };

#ifdef ARDUINO
    if (SD.cardType() != CARD_NONE) {
        lv_fs_spi_lock();
        File root = SD.open("/");
        if (root) {
            int fileId = 1;
            int rootFileIdx = 0;
            File entry = root.openNextFile();
            while (entry) {
                String fileName = entry.name();
                String lowerName = fileName;
                lowerName.toLowerCase();

                if (!entry.isDirectory()) {
                    std::string directPath = "A:/" + std::string(fileName.c_str());
                    if (lowerName.endsWith(".jpg") || lowerName.endsWith(".png") || lowerName.endsWith(".bmp") || lowerName.endsWith(".gif")) {
                        menuItems.push_back({
                            std::to_string(fileId++),
                            fileName.c_str(),
                            "Imagen en SD",
                            0.0f,
                            1,
                            directPath,
                            LV_SYMBOL_IMAGE
                        });
                    }
                    // Archivos de audio (.mp3/.wav) se omiten — van al Reproductor de Música
                } else {
                    // Escanear 1 subnivel (ej. /fotos o /musica)
                    File subDir = SD.open("/" + fileName);
                    if (subDir) {
                        File subEntry = subDir.openNextFile();
                        while (subEntry) {
                            String subName = subEntry.name();
                            String subLower = subName;
                            subLower.toLowerCase();
                            if (!subEntry.isDirectory()) {
                                std::string directPath = "A:/" + std::string(fileName.c_str()) + "/" + std::string(subName.c_str());
                                if (subLower.endsWith(".jpg") || subLower.endsWith(".png") || subLower.endsWith(".bmp") || subLower.endsWith(".gif")) {
                                    menuItems.push_back({
                                        std::to_string(fileId++),
                                        subName.c_str(),
                                        ("Carpeta /" + fileName).c_str(),
                                        0.0f,
                                        1,
                                        directPath,
                                        LV_SYMBOL_IMAGE
                                    });
                                }
                                // Audio ignorado en la galería
                            }
                            subEntry.close();
                            subEntry = subDir.openNextFile();
                        }
                        subDir.close();
                    }
                }
                entry.close();
                entry = root.openNextFile();
            }
            root.close();
        }
        lv_fs_spi_unlock();
    }
#endif

    if (menuItems.empty()) {
        menuItems.push_back({"1", "Sin archivos", "Inserta SD con fotos (.jpg) o canciones (.mp3)", 0.0f, 0, "", LV_SYMBOL_FILE});
    }
}

// ────────────────────────────────────────────────────────────────
//  create()
// ────────────────────────────────────────────────────────────────
lv_obj_t* MenuView::create() {
    if (menuItems.empty()) {
        loadDataFromSD();
    }

    lv_obj_t* screen = lv_obj_create(NULL);
    DefaultTheme::applyFlatBg(screen);
    DefaultTheme::disableScroll(screen);
    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(screen, 8, 0);
    lv_obj_set_style_pad_row(screen, 8, 0);

    headerBar = HeaderBar::create(screen, "Carta", true, false, true);

    // Categorías (scroll horizontal, sin cambios)
    categoriesContainer = lv_obj_create(screen);
    lv_obj_set_width(categoriesContainer, lv_pct(100));
    lv_obj_set_height(categoriesContainer, 44);
    lv_obj_set_style_bg_opa(categoriesContainer, 0, 0);
    lv_obj_set_style_border_width(categoriesContainer, 0, 0);
    lv_obj_set_style_pad_all(categoriesContainer, 2, 0);
    lv_obj_set_style_pad_column(categoriesContainer, 8, 0);
    lv_obj_set_flex_flow(categoriesContainer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(categoriesContainer, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(categoriesContainer, LV_DIR_HOR);
    lv_obj_set_scrollbar_mode(categoriesContainer, LV_SCROLLBAR_MODE_OFF);

    // Contenedor de productos — COLUMNA con altura fija (354px)
    productsContainer = lv_obj_create(screen);
    lv_obj_set_width(productsContainer, lv_pct(100));
    lv_obj_set_height(productsContainer, 354);
    lv_obj_set_style_bg_opa(productsContainer, 0, 0);
    lv_obj_set_style_border_width(productsContainer, 0, 0);
    lv_obj_set_style_pad_all(productsContainer, 0, 0);
    lv_obj_set_style_pad_row(productsContainer, 6, 0);
    lv_obj_set_flex_flow(productsContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(productsContainer, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(productsContainer, LV_DIR_NONE);
    lv_obj_set_scrollbar_mode(productsContainer, LV_SCROLLBAR_MODE_OFF);

    renderCategories();
    renderProducts();

    return screen;
}

// ────────────────────────────────────────────────────────────────
//  renderCategories()
// ────────────────────────────────────────────────────────────────
void MenuView::renderCategories() {
    lv_obj_clean(categoriesContainer);

    for (const auto& cat : categories) {
        lv_obj_t* btn = lv_button_create(categoriesContainer);
        lv_obj_set_height(btn, 36);
        lv_obj_set_style_pad_hor(btn, 12, 0);

        bool isSelected = (cat.id == currentCategoryId);
        if (isSelected) {
            DefaultTheme::applyRaisedCard(btn, 18);
            lv_obj_set_style_border_width(btn, 1, 0);
            lv_obj_set_style_border_color(btn, DefaultTheme::getPrimaryAccent(), 0);
        } else {
            DefaultTheme::applyButton(btn, 18);
        }

        DefaultTheme::disableScroll(btn);
        lv_obj_set_user_data(btn, (void*)(intptr_t)cat.id);
        lv_obj_add_event_cb(btn, category_btn_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(btn, 6, 0);

        lv_obj_t* icon = lv_label_create(btn);
        lv_label_set_text(icon, cat.icon);
        lv_obj_set_style_text_color(icon, isSelected ? DefaultTheme::getPrimaryAccent() : DefaultTheme::getMutedTextColor(), 0);
        lv_obj_remove_flag(icon, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t* label = lv_label_create(btn);
        lv_label_set_text(label, cat.name.c_str());
        lv_obj_set_style_text_color(label, isSelected ? DefaultTheme::getPrimaryAccent() : DefaultTheme::getTextColor(), 0);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
        lv_obj_remove_flag(label, LV_OBJ_FLAG_CLICKABLE);
    }
}

// ────────────────────────────────────────────────────────────────
//  buildFilteredIndices()
// ────────────────────────────────────────────────────────────────
void MenuView::buildFilteredIndices() {
    filteredIndices.clear();
    for (int i = 0; i < (int)menuItems.size(); ++i) {
        if (currentCategoryId == 0 || menuItems[i].categoryId == currentCategoryId) {
            filteredIndices.push_back(i);
        }
    }
}

// ────────────────────────────────────────────────────────────────
//  renderProducts()  — crea la tarjeta única + barra de navegación
// ────────────────────────────────────────────────────────────────
void MenuView::renderProducts() {
    lv_obj_clean(productsContainer);
    card = imgArea = iconLbl = imgObj = nameLbl = priceLbl = nullptr;
    prevBtn = nextBtn = pageIndicatorLbl = nullptr;
    currentItemIndex = 0;

    buildFilteredIndices();
    if (filteredIndices.empty()) {
        lv_obj_t* emptyLbl = lv_label_create(productsContainer);
        lv_label_set_text(emptyLbl, "Sin catálogo disponible\nSincronice desde el Hub");
        lv_obj_set_style_text_color(emptyLbl, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_align(emptyLbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(emptyLbl, &lv_font_montserrat_14, 0);
        lv_obj_center(emptyLbl);
        return;
    }

    // Inicializar estilos estáticos una sola vez
    if (!stylesInitialized) {
        lv_style_init(&style_raised_card);
        lv_style_set_bg_color(&style_raised_card,     lv_color_hex(0x1B1E29));
        lv_style_set_bg_opa(&style_raised_card,       LV_OPA_COVER);
        lv_style_set_radius(&style_raised_card,       14);
        lv_style_set_border_color(&style_raised_card, lv_color_hex(0x2E3444));
        lv_style_set_border_width(&style_raised_card, 1);
        lv_style_set_border_opa(&style_raised_card,   LV_OPA_COVER);

        lv_style_init(&style_sunken_area);
        lv_style_set_bg_color(&style_sunken_area,     lv_color_hex(0x11131A));
        lv_style_set_bg_opa(&style_sunken_area,       LV_OPA_COVER);
        lv_style_set_radius(&style_sunken_area,       16);
        lv_style_set_border_color(&style_sunken_area, lv_color_hex(0x0B0C11));
        lv_style_set_border_width(&style_sunken_area, 1);
        lv_style_set_border_opa(&style_sunken_area,   LV_OPA_COVER);

        stylesInitialized = true;
    }

    // ── Tarjeta única (304x300px) ──────────────────────────────────
    card = lv_obj_create(productsContainer);
    lv_obj_set_width(card, lv_pct(100));
    lv_obj_set_height(card, 300);
    lv_obj_add_style(card, &style_raised_card, 0);
    lv_obj_set_style_pad_all(card, 10, 0);
    lv_obj_set_scroll_dir(card, LV_DIR_NONE);
    lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Área de imagen (altura fija 200px)
    imgArea = lv_obj_create(card);
    lv_obj_set_width(imgArea, lv_pct(100));
    lv_obj_set_height(imgArea, 200);
    lv_obj_add_style(imgArea, &style_sunken_area, 0);
    lv_obj_set_scroll_dir(imgArea, LV_DIR_NONE);
    lv_obj_set_scrollbar_mode(imgArea, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(imgArea, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_add_flag(imgArea, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(imgArea, product_card_cb, LV_EVENT_CLICKED, NULL);

    // Icono placeholder (hijo 0 de imgArea — persistente)
    iconLbl = lv_label_create(imgArea);
    lv_label_set_text(iconLbl, LV_SYMBOL_IMAGE);
    lv_obj_set_style_text_color(iconLbl, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_set_style_text_font(iconLbl, &lv_font_montserrat_32, 0);
    lv_obj_center(iconLbl);

    // Imagen (hijo 1 de imgArea — persistente, oculta por defecto)
    imgObj = lv_img_create(imgArea);
    lv_obj_add_flag(imgObj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(imgObj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(imgObj);

    // Nombre del producto
    nameLbl = lv_label_create(card);
    lv_label_set_text(nameLbl, "");
    lv_obj_set_style_text_color(nameLbl, DefaultTheme::getTextColor(), 0);
    lv_label_set_long_mode(nameLbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(nameLbl, lv_pct(100));
    lv_obj_set_height(nameLbl, 40);
    lv_obj_set_style_text_align(nameLbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_top(nameLbl, 8, 0);

    // Precio
    priceLbl = lv_label_create(card);
    lv_label_set_text(priceLbl, "");
    lv_obj_set_style_text_color(priceLbl, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_set_style_text_font(priceLbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_pad_bottom(priceLbl, 8, 0);

    // ── Barra de navegación ──────────────────────────────────────
    lv_obj_t* navRow = lv_obj_create(productsContainer);
    lv_obj_set_width(navRow, lv_pct(100));
    lv_obj_set_height(navRow, 48);
    lv_obj_set_style_bg_opa(navRow, 0, 0);
    lv_obj_set_style_border_width(navRow, 0, 0);
    lv_obj_set_style_pad_all(navRow, 0, 0);
    lv_obj_clear_flag(navRow, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(navRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(navRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Botón ◀
    prevBtn = lv_button_create(navRow);
    lv_obj_set_size(prevBtn, 72, 40);
    DefaultTheme::applyButton(prevBtn, 12);
    lv_obj_add_event_cb(prevBtn, prev_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* prevLbl = lv_label_create(prevBtn);
    lv_label_set_text(prevLbl, LV_SYMBOL_LEFT "  Ant.");
    lv_obj_set_style_text_color(prevLbl, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_set_style_text_font(prevLbl, &lv_font_montserrat_12, 0);
    lv_obj_center(prevLbl);

    // Indicador de página "1 / 6"
    pageIndicatorLbl = lv_label_create(navRow);
    lv_label_set_text(pageIndicatorLbl, "");
    lv_obj_set_style_text_color(pageIndicatorLbl, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(pageIndicatorLbl, &lv_font_montserrat_12, 0);

    // Botón ▶
    nextBtn = lv_button_create(navRow);
    lv_obj_set_size(nextBtn, 72, 40);
    DefaultTheme::applyButton(nextBtn, 12);
    lv_obj_add_event_cb(nextBtn, next_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* nextLbl = lv_label_create(nextBtn);
    lv_label_set_text(nextLbl, "Sig.  " LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(nextLbl, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_set_style_text_font(nextLbl, &lv_font_montserrat_12, 0);
    lv_obj_center(nextLbl);

    // Mostrar el primer ítem
    navigateTo(0);
}

// ────────────────────────────────────────────────────────────────
//  navigateTo()  — actualiza la tarjeta única con el ítem indicado
// ────────────────────────────────────────────────────────────────
void MenuView::navigateTo(int index) {
    int total = (int)filteredIndices.size();
    if (total == 0 || card == nullptr) return;

    if (index < 0)       index = 0;
    if (index >= total)  index = total - 1;
    currentItemIndex = index;

    const MenuItem& item = menuItems[filteredIndices[index]];

    // Actualizar user_data del área clickeable con el índice real en menuItems
    lv_obj_set_user_data(imgArea, (void*)(intptr_t)filteredIndices[index]);

    // Actualizar imagen/icono/audio
    if (item.categoryId != 2 && item.imageHash.length() > 0 && (item.icon == LV_SYMBOL_IMAGE || item.imageHash.find("sdidx:") != std::string::npos)) {
        std::string resolved = resolveSdidxPath(item.imageHash);
        if (resolved.length() > 0) {
            lv_img_set_src(imgObj, resolved.c_str());
            lv_obj_center(imgObj);
            lv_obj_clear_flag(imgObj, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(iconLbl,  LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_label_set_text(iconLbl, item.icon ? item.icon : LV_SYMBOL_FILE);
            lv_obj_clear_flag(iconLbl, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(imgObj,    LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        lv_label_set_text(iconLbl, item.icon ? item.icon : LV_SYMBOL_FILE);
        lv_obj_clear_flag(iconLbl, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(imgObj,    LV_OBJ_FLAG_HIDDEN);
    }

    lv_label_set_text(nameLbl, item.name.c_str());
    lv_label_set_text(priceLbl, item.description.c_str());

    updateNavButtons();
}

// ────────────────────────────────────────────────────────────────
//  updateNavButtons()  — deshabilita ◀/▶ en los extremos
// ────────────────────────────────────────────────────────────────
void MenuView::updateNavButtons() {
    int total = (int)filteredIndices.size();

    if (currentItemIndex <= 0) lv_obj_add_state(prevBtn,  LV_STATE_DISABLED);
    else                        lv_obj_clear_state(prevBtn, LV_STATE_DISABLED);

    if (currentItemIndex >= total - 1) lv_obj_add_state(nextBtn,  LV_STATE_DISABLED);
    else                               lv_obj_clear_state(nextBtn, LV_STATE_DISABLED);

    lv_label_set_text_fmt(pageIndicatorLbl, "%d / %d", currentItemIndex + 1, total);
}

// ────────────────────────────────────────────────────────────────
//  Callbacks de navegación
// ────────────────────────────────────────────────────────────────
void MenuView::prev_btn_cb(lv_event_t* e) {
    UIManager::getInstance().resetInactivityTimer();
    navigateTo(currentItemIndex - 1);
}

void MenuView::next_btn_cb(lv_event_t* e) {
    UIManager::getInstance().resetInactivityTimer();
    navigateTo(currentItemIndex + 1);
}

// ────────────────────────────────────────────────────────────────
//  Callbacks de categoría y producto
// ────────────────────────────────────────────────────────────────
void MenuView::category_btn_cb(lv_event_t* e) {
    lv_indev_t* indev = lv_indev_active();
    if (indev && lv_indev_get_scroll_obj(indev) != NULL) return;

    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
    int catId = (int)(intptr_t)lv_obj_get_user_data(btn);

    if (currentCategoryId != catId) {
        currentCategoryId = catId;
        renderCategories();
        renderProducts();
    }
    UIManager::getInstance().resetInactivityTimer();
}

void MenuView::product_card_cb(lv_event_t* e) {
    lv_indev_t* indev = lv_indev_active();
    if (indev && lv_indev_get_scroll_obj(indev) != NULL) return;

    lv_obj_t* area = (lv_obj_t*)lv_event_get_target(e);
    int index = (int)(intptr_t)lv_obj_get_user_data(area);

    if (index >= 0 && index < (int)menuItems.size()) {
        showProductModal(menuItems[index]);
    }
    UIManager::getInstance().resetInactivityTimer();
}

// ────────────────────────────────────────────────────────────────
//  showProductModal()  (sin cambios)
// ────────────────────────────────────────────────────────────────
void MenuView::showProductModal(const MenuItem& item) {
    UIManager::getInstance().resetInactivityTimer();

    lv_obj_t* mask = lv_obj_create(lv_layer_top());
    lv_obj_add_flag(mask, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_size(mask, 320, 480);
    lv_obj_set_pos(mask, 0, 0);
    lv_obj_set_style_pad_all(mask, 0, 0);
    lv_obj_set_style_border_width(mask, 0, 0);
    lv_obj_set_style_radius(mask, 0, 0);
    lv_obj_set_style_bg_color(mask, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(mask, LV_OPA_70, 0);
    DefaultTheme::disableScroll(mask);

    lv_obj_t* modal = lv_obj_create(mask);
    lv_obj_set_width(modal, 280);
    lv_obj_set_height(modal, LV_SIZE_CONTENT);
    DefaultTheme::applyRaisedCard(modal, 20);
    lv_obj_center(modal);
    lv_obj_set_style_pad_all(modal, 16, 0);
    lv_obj_set_flex_flow(modal, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(modal, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(modal, 12, 0);

    lv_obj_t* imgContainer = lv_obj_create(modal);
    lv_obj_set_size(imgContainer, 70, 70);
    DefaultTheme::applySunkenCard(imgContainer, 16);
    DefaultTheme::disableScroll(imgContainer);

    lv_obj_t* icon = lv_label_create(imgContainer);
    lv_label_set_text(icon, item.icon);
    lv_obj_set_style_text_color(icon, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_24, 0);
    lv_obj_center(icon);

    lv_obj_t* nameLblM = lv_label_create(modal);
    lv_label_set_text(nameLblM, item.name.c_str());
    lv_obj_set_style_text_color(nameLblM, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(nameLblM, &lv_font_montserrat_16, 0);

    lv_obj_t* descLbl = lv_label_create(modal);
    lv_label_set_text(descLbl, item.description.c_str());
    lv_obj_set_style_text_color(descLbl, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(descLbl, &lv_font_montserrat_12, 0);
    lv_label_set_long_mode(descLbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(descLbl, lv_pct(100));

    lv_obj_t* priceLblM = lv_label_create(modal);
    lv_label_set_text(priceLblM, item.imageHash.c_str());
    lv_obj_set_style_text_color(priceLblM, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_set_style_text_font(priceLblM, &lv_font_montserrat_12, 0);

    lv_obj_t* addBtn = lv_button_create(modal);
    lv_obj_set_size(addBtn, 220, 40);
    DefaultTheme::applyButton(addBtn, 12);

    lv_obj_t* addLbl = lv_label_create(addBtn);
    lv_label_set_text(addLbl, LV_SYMBOL_IMAGE " Ver a Pantalla Completa");
    // Los archivos de musica (categoryId == 2) no se abren desde la galeria
    if (item.categoryId == 2) lv_obj_add_flag(addBtn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_color(addLbl, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_set_style_text_font(addLbl, &lv_font_montserrat_14, 0);
    lv_obj_center(addLbl);

    lv_obj_t* closeBtn = lv_button_create(modal);
    lv_obj_set_size(closeBtn, 220, 36);
    DefaultTheme::applyButton(closeBtn, 12);

    lv_obj_t* closeLbl = lv_label_create(closeBtn);
    lv_label_set_text(closeLbl, "Cerrar");
    lv_obj_set_style_text_color(closeLbl, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(closeLbl, &lv_font_montserrat_14, 0);
    lv_obj_center(closeLbl);

    MenuItem* itemCopy = new MenuItem(item);

    lv_obj_add_event_cb(addBtn, [](lv_event_t* e) {
        UIManager::getInstance().resetInactivityTimer();
        MenuItem* itemPtr = (MenuItem*)lv_event_get_user_data(e);
        if (itemPtr && itemPtr->categoryId != 2) {
            UIManager::getInstance().loadImageViewer(itemPtr->imageHash, itemPtr->name);
        }
        lv_obj_t* btn       = (lv_obj_t*)lv_event_get_target(e);
        lv_obj_t* modalCard = lv_obj_get_parent(btn);
        lv_obj_t* maskObj   = lv_obj_get_parent(modalCard);
        if (lv_obj_is_valid(maskObj)) lv_obj_delete_async(maskObj);
    }, LV_EVENT_CLICKED, itemCopy);

    lv_obj_add_event_cb(addBtn, [](lv_event_t* e) {
        MenuItem* itemPtr = (MenuItem*)lv_event_get_user_data(e);
        if (itemPtr) delete itemPtr;
    }, LV_EVENT_DELETE, itemCopy);

    lv_obj_add_event_cb(closeBtn, [](lv_event_t* e) {
        UIManager::getInstance().resetInactivityTimer();
        lv_obj_t* btn       = (lv_obj_t*)lv_event_get_target(e);
        lv_obj_t* modalCard = lv_obj_get_parent(btn);
        lv_obj_t* maskObj   = lv_obj_get_parent(modalCard);
        if (lv_obj_is_valid(maskObj)) lv_obj_delete_async(maskObj);
    }, LV_EVENT_CLICKED, NULL);
}

// ────────────────────────────────────────────────────────────────
//  resolveSdidxPath()  — resuelve rutas sdidx: a rutas VFS A:/
// ────────────────────────────────────────────────────────────────
std::string MenuView::resolveSdidxPath(const std::string& path) {
    if (path.rfind("sdidx:", 0) != 0) {
        if (path.rfind("A:", 0) == 0) return path;
        if (path.find('/') == std::string::npos && path.find('.') == std::string::npos && !path.empty()) {
            std::string cached = AssetManager::getInstance().getAssetPath(path, 320, 480);
            if (cached.empty()) {
                cached = AssetManager::getInstance().getAssetPath(path, 200, 200);
            }
            return cached;
        }
        if (path.rfind("/", 0) == 0) return "A:" + path;
        return "A:/" + path;
    }
#ifdef ARDUINO
    std::string inner = path.substr(6);
    size_t sep = inner.find_last_of(':');
    if (sep == std::string::npos) return "";
    std::string dirPath = inner.substr(0, sep);
    int fileIdx = 0;
    try {
        fileIdx = std::stoi(inner.substr(sep + 1));
    } catch (...) {
        return "";
    }

    lv_fs_spi_lock();
    File dir = SD.open(dirPath.c_str());
    if (!dir) {
        lv_fs_spi_unlock();
        return "";
    }

    int current = 0;
    File candidate = dir.openNextFile();
    std::string result = "";
    while (candidate) {
        std::string cName = candidate.name();
        std::string cLow = cName;
        for (auto &c : cLow) c = tolower(c);
        bool isMedia = !candidate.isDirectory() &&
                       (cLow.find(".mp3") != std::string::npos || cLow.find(".wav") != std::string::npos ||
                        cLow.find(".jpg") != std::string::npos || cLow.find(".png") != std::string::npos ||
                        cLow.find(".bmp") != std::string::npos || cLow.find(".gif") != std::string::npos);
        if (isMedia) {
            if (current == fileIdx) {
                if (dirPath == "/") {
                    result = "A:/" + cName;
                } else {
                    result = "A:" + dirPath + "/" + cName;
                }
                candidate.close();
                break;
            }
            current++;
        }
        candidate.close();
        candidate = dir.openNextFile();
    }
    dir.close();
    lv_fs_spi_unlock();
    return result;
#else
    return "";
#endif
}

// ────────────────────────────────────────────────────────────────
//  showFullScreenImage()  — muestra la imagen/GIF a pantalla completa
// ────────────────────────────────────────────────────────────────
void MenuView::showFullScreenImage(const std::string& path) {
    std::string resolved = resolveSdidxPath(path);
    if (resolved.empty()) {
        UIManager::showToast("No se pudo cargar la imagen");
        return;
    }

    lv_obj_t* container = lv_obj_create(lv_layer_top());
    lv_obj_set_size(container, 320, 480);
    lv_obj_set_pos(container, 0, 0);
    lv_obj_set_style_bg_color(container, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(container, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_radius(container, 0, 0);
    lv_obj_set_style_pad_all(container, 0, 0);

    std::string lowerResolved = resolved;
    for (auto &c : lowerResolved) c = tolower(c);
    bool isGif = (lowerResolved.length() >= 4 && lowerResolved.substr(lowerResolved.length() - 4) == ".gif");

    lv_obj_t* img = nullptr;
    if (isGif) {
#if LV_USE_GIF
        img = lv_gif_create(container);
        lv_gif_set_src(img, resolved.c_str());
#else
        img = lv_img_create(container);
        lv_img_set_src(img, resolved.c_str());
#endif
    } else {
        img = lv_img_create(container);
        lv_img_set_src(img, resolved.c_str());
    }

    if (img) {
        lv_obj_center(img);
    }

    lv_obj_add_flag(container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(container, [](lv_event_t* e) {
        lv_obj_t* obj = (lv_obj_t*)lv_event_get_target(e);
        if (lv_obj_is_valid(obj)) {
            lv_obj_delete_async(obj);
        }
    }, LV_EVENT_CLICKED, nullptr);
}
