#include "GalleryListView.h"
#include "../Themes/DefaultTheme.h"
#include "../../Core/LVFS_Driver.h"
#include "../UIManager.h"
#include <cstdio>
#ifdef ARDUINO
#include <SD.h>
#endif

// ────────────────────────────────────────────────────────────────
//  Definición de estáticos
// ────────────────────────────────────────────────────────────────
HeaderBar*  GalleryListView::headerBar           = nullptr;
lv_obj_t*   GalleryListView::categoriesContainer = nullptr;
lv_obj_t*   GalleryListView::mediaContainer      = nullptr;
int         GalleryListView::currentCategoryId   = 0;
int         GalleryListView::currentItemIndex    = 0;

std::vector<GalleryCategory>  GalleryListView::categories;
std::vector<GalleryMediaItem> GalleryListView::mediaItems;
std::vector<int>              GalleryListView::filteredIndices;

// Widgets tarjeta central
lv_obj_t* GalleryListView::card             = nullptr;
lv_obj_t* GalleryListView::imgArea          = nullptr;
lv_obj_t* GalleryListView::iconLbl          = nullptr;
lv_obj_t* GalleryListView::imgObj           = nullptr;
lv_obj_t* GalleryListView::gifObj           = nullptr;
lv_obj_t* GalleryListView::nameLbl          = nullptr;
lv_obj_t* GalleryListView::pathLbl          = nullptr;

// Navegación
lv_obj_t* GalleryListView::prevBtn          = nullptr;
lv_obj_t* GalleryListView::nextBtn          = nullptr;
lv_obj_t* GalleryListView::pageIndicatorLbl = nullptr;

// Estilos
lv_style_t GalleryListView::style_raised_card;
lv_style_t GalleryListView::style_sunken_area;
bool       GalleryListView::stylesInitialized = false;

// ────────────────────────────────────────────────────────────────
//  Carga de datos desde Tarjeta SD
// ────────────────────────────────────────────────────────────────
void GalleryListView::loadDataFromSD() {
    mediaItems.clear();
    categories.clear();

    categories = {
        {0, "Todas", LV_SYMBOL_LIST},
        {1, "Fotos", LV_SYMBOL_IMAGE},
        {2, "GIFs",  LV_SYMBOL_PLAY}
    };

#ifdef ARDUINO
    if (SD.cardType() != CARD_NONE) {
        lv_fs_spi_lock();
        File root = SD.open("/");
        if (root) {
            int fileId = 1;
            File entry = root.openNextFile();
            while (entry) {
                String fileName = entry.name();
                String lowerName = fileName;
                lowerName.toLowerCase();

                if (!entry.isDirectory()) {
                    std::string directPath = "A:/" + std::string(fileName.c_str());
                    bool isGif = lowerName.endsWith(".gif");
                    bool isImg = lowerName.endsWith(".jpg") || lowerName.endsWith(".jpeg") || 
                                 lowerName.endsWith(".png") || lowerName.endsWith(".bmp");

                    if (isGif || isImg) {
                        mediaItems.push_back({
                            std::to_string(fileId++),
                            fileName.c_str(),
                            "Raiz",
                            directPath,
                            isGif,
                            isGif ? LV_SYMBOL_PLAY : LV_SYMBOL_IMAGE
                        });
                    }
                } else {
                    // Escanear 1 subnivel de carpetas (ej. /fotos, /wallpapers, /memes)
                    File subDir = SD.open("/" + fileName);
                    if (subDir) {
                        File subEntry = subDir.openNextFile();
                        while (subEntry) {
                            String subName = subEntry.name();
                            String subLower = subName;
                            subLower.toLowerCase();
                            if (!subEntry.isDirectory()) {
                                std::string directPath = "A:/" + std::string(fileName.c_str()) + "/" + std::string(subName.c_str());
                                bool isGif = subLower.endsWith(".gif");
                                bool isImg = subLower.endsWith(".jpg") || subLower.endsWith(".jpeg") || 
                                             subLower.endsWith(".png") || subLower.endsWith(".bmp");

                                if (isGif || isImg) {
                                    mediaItems.push_back({
                                        std::to_string(fileId++),
                                        subName.c_str(),
                                        fileName.c_str(),
                                        directPath,
                                        isGif,
                                        isGif ? LV_SYMBOL_PLAY : LV_SYMBOL_IMAGE
                                    });
                                }
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

    if (mediaItems.empty()) {
        mediaItems.push_back({
            "0",
            "Sin imagenes",
            "SD",
            "",
            false,
            LV_SYMBOL_FILE
        });
    }
}

void GalleryListView::refreshGallery() {
    categories.clear();
    mediaItems.clear();
    loadDataFromSD();
    renderCategories();
    renderMedia();
}

// ────────────────────────────────────────────────────────────────
//  create()
// ────────────────────────────────────────────────────────────────
lv_obj_t* GalleryListView::create() {
    if (mediaItems.empty()) {
        loadDataFromSD();
    }

    lv_obj_t* screen = lv_obj_create(NULL);
    DefaultTheme::applyFlatBg(screen);
    DefaultTheme::disableScroll(screen);
    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(screen, 8, 0);
    lv_obj_set_style_pad_row(screen, 8, 0);

    // Cabecera "Galería"
    headerBar = HeaderBar::create(screen, "Galeria", true, true, false);
    HeaderBar::setActiveHeader(headerBar);

    // Contenedor de Categorías / Filtros
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

    // Contenedor principal de visualización
    mediaContainer = lv_obj_create(screen);
    lv_obj_set_width(mediaContainer, lv_pct(100));
    lv_obj_set_height(mediaContainer, 354);
    lv_obj_set_style_bg_opa(mediaContainer, 0, 0);
    lv_obj_set_style_border_width(mediaContainer, 0, 0);
    lv_obj_set_style_pad_all(mediaContainer, 0, 0);
    lv_obj_set_style_pad_row(mediaContainer, 6, 0);
    lv_obj_set_flex_flow(mediaContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(mediaContainer, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(mediaContainer, LV_DIR_NONE);
    lv_obj_set_scrollbar_mode(mediaContainer, LV_SCROLLBAR_MODE_OFF);

    renderCategories();
    renderMedia();

    return screen;
}

// ────────────────────────────────────────────────────────────────
//  renderCategories()
// ────────────────────────────────────────────────────────────────
void GalleryListView::renderCategories() {
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
void GalleryListView::buildFilteredIndices() {
    filteredIndices.clear();
    for (int i = 0; i < (int)mediaItems.size(); ++i) {
        if (currentCategoryId == 0) {
            filteredIndices.push_back(i);
        } else if (currentCategoryId == 1 && !mediaItems[i].isGif) {
            filteredIndices.push_back(i);
        } else if (currentCategoryId == 2 && mediaItems[i].isGif) {
            filteredIndices.push_back(i);
        }
    }
}

// ────────────────────────────────────────────────────────────────
//  renderMedia()  — tarjeta central + barra de navegación
// ────────────────────────────────────────────────────────────────
void GalleryListView::renderMedia() {
    lv_obj_clean(mediaContainer);
    card = imgArea = iconLbl = imgObj = gifObj = nameLbl = pathLbl = nullptr;
    prevBtn = nextBtn = pageIndicatorLbl = nullptr;
    currentItemIndex = 0;

    buildFilteredIndices();
    if (filteredIndices.empty()) {
        lv_obj_t* emptyLbl = lv_label_create(mediaContainer);
        lv_label_set_text(emptyLbl, "No hay archivos en esta categoria\nInserte tarjeta SD con imagenes o GIFs");
        lv_obj_set_style_text_color(emptyLbl, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_align(emptyLbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(emptyLbl, &lv_font_montserrat_14, 0);
        lv_obj_center(emptyLbl);
        return;
    }

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

    // Tarjeta central (300px altura)
    card = lv_obj_create(mediaContainer);
    lv_obj_set_width(card, lv_pct(100));
    lv_obj_set_height(card, 300);
    lv_obj_add_style(card, &style_raised_card, 0);
    lv_obj_set_style_pad_all(card, 10, 0);
    lv_obj_set_scroll_dir(card, LV_DIR_NONE);
    lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Área de imagen (200px altura)
    imgArea = lv_obj_create(card);
    lv_obj_set_width(imgArea, lv_pct(100));
    lv_obj_set_height(imgArea, 200);
    lv_obj_add_style(imgArea, &style_sunken_area, 0);
    lv_obj_set_scroll_dir(imgArea, LV_DIR_NONE);
    lv_obj_set_scrollbar_mode(imgArea, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(imgArea, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_add_flag(imgArea, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(imgArea, media_card_cb, LV_EVENT_CLICKED, NULL);

    // Icono placeholder
    iconLbl = lv_label_create(imgArea);
    lv_label_set_text(iconLbl, LV_SYMBOL_IMAGE);
    lv_obj_set_style_text_color(iconLbl, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_set_style_text_font(iconLbl, &lv_font_montserrat_32, 0);
    lv_obj_center(iconLbl);

    // Widget para imagen estática
    imgObj = lv_image_create(imgArea);
    lv_obj_add_flag(imgObj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(imgObj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(imgObj);

#if LV_USE_GIF
    // Widget para GIF animado
    gifObj = lv_gif_create(imgArea);
    lv_obj_add_flag(gifObj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(gifObj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(gifObj);
#endif

    // Nombre de la imagen/archivo
    nameLbl = lv_label_create(card);
    lv_label_set_text(nameLbl, "");
    lv_obj_set_style_text_color(nameLbl, DefaultTheme::getTextColor(), 0);
    lv_label_set_long_mode(nameLbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(nameLbl, lv_pct(100));
    lv_obj_set_height(nameLbl, 34);
    lv_obj_set_style_text_align(nameLbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_top(nameLbl, 6, 0);

    // Carpeta / Ruta relativa
    pathLbl = lv_label_create(card);
    lv_label_set_text(pathLbl, "");
    lv_obj_set_style_text_color(pathLbl, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(pathLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_pad_bottom(pathLbl, 6, 0);

    // Barra de navegación inferior
    lv_obj_t* navRow = lv_obj_create(mediaContainer);
    lv_obj_set_width(navRow, lv_pct(100));
    lv_obj_set_height(navRow, 48);
    lv_obj_set_style_bg_opa(navRow, 0, 0);
    lv_obj_set_style_border_width(navRow, 0, 0);
    lv_obj_set_style_pad_all(navRow, 0, 0);
    lv_obj_clear_flag(navRow, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(navRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(navRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Botón Anterior
    prevBtn = lv_button_create(navRow);
    lv_obj_set_size(prevBtn, 76, 40);
    DefaultTheme::applyButton(prevBtn, 12);
    lv_obj_add_event_cb(prevBtn, prev_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* prevLbl = lv_label_create(prevBtn);
    lv_label_set_text(prevLbl, LV_SYMBOL_LEFT "  Ant.");
    lv_obj_set_style_text_color(prevLbl, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_set_style_text_font(prevLbl, &lv_font_montserrat_12, 0);
    lv_obj_center(prevLbl);

    // Indicador "1 / Total"
    pageIndicatorLbl = lv_label_create(navRow);
    lv_label_set_text(pageIndicatorLbl, "");
    lv_obj_set_style_text_color(pageIndicatorLbl, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(pageIndicatorLbl, &lv_font_montserrat_12, 0);

    // Botón Siguiente
    nextBtn = lv_button_create(navRow);
    lv_obj_set_size(nextBtn, 76, 40);
    DefaultTheme::applyButton(nextBtn, 12);
    lv_obj_add_event_cb(nextBtn, next_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* nextLbl = lv_label_create(nextBtn);
    lv_label_set_text(nextLbl, "Sig.  " LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(nextLbl, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_set_style_text_font(nextLbl, &lv_font_montserrat_12, 0);
    lv_obj_center(nextLbl);

    navigateTo(0);
}

// ────────────────────────────────────────────────────────────────
//  navigateTo()
// ────────────────────────────────────────────────────────────────
void GalleryListView::navigateTo(int index) {
    int total = (int)filteredIndices.size();
    if (total == 0 || card == nullptr) return;

    if (index < 0)       index = 0;
    if (index >= total)  index = total - 1;
    currentItemIndex = index;

    const GalleryMediaItem& item = mediaItems[filteredIndices[index]];

    lv_obj_set_user_data(imgArea, (void*)(intptr_t)filteredIndices[index]);

    // Ocultar todos los widgets visuales primero
    if (iconLbl) lv_obj_add_flag(iconLbl, LV_OBJ_FLAG_HIDDEN);
    if (imgObj)  lv_obj_add_flag(imgObj,  LV_OBJ_FLAG_HIDDEN);
#if LV_USE_GIF
    if (gifObj)  lv_obj_add_flag(gifObj,  LV_OBJ_FLAG_HIDDEN);
#endif

    if (!item.path.empty()) {
#if LV_USE_GIF
        if (item.isGif && gifObj) {
            lv_gif_set_src(gifObj, item.path.c_str());
            lv_obj_center(gifObj);
            lv_obj_clear_flag(gifObj, LV_OBJ_FLAG_HIDDEN);
        } else
#endif
        if (imgObj) {
            lv_image_set_src(imgObj, item.path.c_str());
            lv_obj_center(imgObj);
            lv_obj_clear_flag(imgObj, LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        if (iconLbl) {
            lv_label_set_text(iconLbl, item.icon ? item.icon : LV_SYMBOL_FILE);
            lv_obj_clear_flag(iconLbl, LV_OBJ_FLAG_HIDDEN);
        }
    }

    lv_label_set_text(nameLbl, item.name.c_str());
    std::string folderStr = "Carpeta: " + item.folder;
    lv_label_set_text(pathLbl, folderStr.c_str());

    updateNavButtons();
}

// ────────────────────────────────────────────────────────────────
//  updateNavButtons()
// ────────────────────────────────────────────────────────────────
void GalleryListView::updateNavButtons() {
    int total = (int)filteredIndices.size();

    if (currentItemIndex <= 0) lv_obj_add_state(prevBtn,  LV_STATE_DISABLED);
    else                        lv_obj_clear_state(prevBtn, LV_STATE_DISABLED);

    if (currentItemIndex >= total - 1) lv_obj_add_state(nextBtn,  LV_STATE_DISABLED);
    else                               lv_obj_clear_state(nextBtn, LV_STATE_DISABLED);

    lv_label_set_text_fmt(pageIndicatorLbl, "%d / %d", currentItemIndex + 1, total);
}

// ────────────────────────────────────────────────────────────────
//  Callbacks de eventos
// ────────────────────────────────────────────────────────────────
void GalleryListView::prev_btn_cb(lv_event_t* e) {
    UIManager::getInstance().resetInactivityTimer();
    navigateTo(currentItemIndex - 1);
}

void GalleryListView::next_btn_cb(lv_event_t* e) {
    UIManager::getInstance().resetInactivityTimer();
    navigateTo(currentItemIndex + 1);
}

void GalleryListView::category_btn_cb(lv_event_t* e) {
    lv_indev_t* indev = lv_indev_active();
    if (indev && lv_indev_get_scroll_obj(indev) != NULL) return;

    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
    int catId = (int)(intptr_t)lv_obj_get_user_data(btn);

    if (currentCategoryId != catId) {
        currentCategoryId = catId;
        renderCategories();
        renderMedia();
    }
    UIManager::getInstance().resetInactivityTimer();
}

void GalleryListView::media_card_cb(lv_event_t* e) {
    lv_indev_t* indev = lv_indev_active();
    if (indev && lv_indev_get_scroll_obj(indev) != NULL) return;

    lv_obj_t* area = (lv_obj_t*)lv_event_get_target(e);
    int index = (int)(intptr_t)lv_obj_get_user_data(area);

    if (index >= 0 && index < (int)mediaItems.size()) {
        const auto& item = mediaItems[index];
        if (!item.path.empty()) {
            UIManager::getInstance().loadImageViewer(item.path, item.name);
        }
    }
    UIManager::getInstance().resetInactivityTimer();
}
