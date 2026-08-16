#include "RadioView.h"
#include "../Themes/DefaultTheme.h"
#include "../UIManager.h"
#include "../../Core/NativeAudioDriver.h"

HeaderBar*              RadioView::headerBar            = nullptr;
RadioStation            RadioView::currentStation;
bool                    RadioView::isPlaying            = false;

lv_obj_t*               RadioView::playerBar            = nullptr;
lv_obj_t*               RadioView::currentNameLbl       = nullptr;
lv_obj_t*               RadioView::currentStatusLbl     = nullptr;
lv_obj_t*               RadioView::playBtn              = nullptr;
lv_obj_t*               RadioView::playBtnLbl           = nullptr;

lv_obj_t*               RadioView::tabBtnFav            = nullptr;
lv_obj_t*               RadioView::tabBtnExplore        = nullptr;
lv_obj_t*               RadioView::tabBtnAdd            = nullptr;

lv_obj_t*               RadioView::favContainer         = nullptr;
lv_obj_t*               RadioView::exploreContainer      = nullptr;
lv_obj_t*               RadioView::addContainer         = nullptr;

size_t                  RadioView::currentSearchOffset    = 0;
std::string             RadioView::currentSearchQuery     = "";

lv_obj_t*               RadioView::taSearch               = nullptr;
lv_obj_t*               RadioView::btnSearch              = nullptr;
lv_obj_t*               RadioView::btnPrevPage            = nullptr;
lv_obj_t*               RadioView::btnNextPage            = nullptr;
lv_obj_t*               RadioView::pageLbl                = nullptr;
lv_obj_t*               RadioView::exploreList          = nullptr;

lv_obj_t*               RadioView::taName               = nullptr;
lv_obj_t*               RadioView::taUrl                = nullptr;
lv_obj_t*               RadioView::taGenre              = nullptr;

static std::vector<RadioStation> s_currentExploreStations;

lv_obj_t* RadioView::create() {
    RadioManager::getInstance().init();

    lv_obj_t* scr = lv_obj_create(NULL);
    DefaultTheme::applyFlatBg(scr);

    HeaderBarConfig cfg;
    cfg.title = "Radio Online";
    cfg.showBackButton = true;
    cfg.showTime = true;
    cfg.showWifi = true;
    cfg.showCartButton = false;
    cfg.titleMarquee = false;
    cfg.translucent = false;

    headerBar = HeaderBar::create(scr, cfg);
    HeaderBar::setActiveHeader(headerBar);

    // Contenedor principal
    lv_obj_t* mainCont = lv_obj_create(scr);
    lv_obj_set_size(mainCont, 320, 435);
    lv_obj_align(mainCont, LV_ALIGN_TOP_MID, 0, 45);
    lv_obj_set_style_bg_opa(mainCont, 0, 0);
    lv_obj_set_style_border_width(mainCont, 0, 0);
    lv_obj_set_style_pad_all(mainCont, 4, 0);
    lv_obj_set_flex_flow(mainCont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(mainCont, 4, 0);
    DefaultTheme::disableScroll(mainCont);

    // 1. Barra de Reproductor Superior (Fija)
    buildPlayerBar(mainCont);

    // 2. Barra de Segmentos (3 Botones Fijos: 94px c/u)
    buildSegmentedNav(mainCont);

    // 3. Los 3 Contenedores de Vistas
    buildFavoritesView(mainCont);
    buildExploreView(mainCont);
    buildAddManualView(mainCont);

    // Iniciar mostrando Favoritas
    showTab(0);

    return scr;
}

void RadioView::buildPlayerBar(lv_obj_t* parent) {
    playerBar = lv_obj_create(parent);
    lv_obj_set_size(playerBar, lv_pct(100), 58);
    DefaultTheme::applyRaisedCard(playerBar, 10);
    DefaultTheme::disableScroll(playerBar);
    lv_obj_set_style_pad_all(playerBar, 6, 0);
    lv_obj_set_flex_flow(playerBar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(playerBar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Icono + Textos
    lv_obj_t* infoRow = lv_obj_create(playerBar);
    lv_obj_set_size(infoRow, 230, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(infoRow, 0, 0);
    lv_obj_set_style_border_width(infoRow, 0, 0);
    lv_obj_set_style_pad_all(infoRow, 0, 0);
    lv_obj_set_flex_flow(infoRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(infoRow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(infoRow, 6, 0);
    DefaultTheme::disableScroll(infoRow);

    lv_obj_t* icon = lv_label_create(infoRow);
    lv_label_set_text(icon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(icon, DefaultTheme::getPrimaryAccent(), 0);

    lv_obj_t* txtCol = lv_obj_create(infoRow);
    lv_obj_set_size(txtCol, 190, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(txtCol, 0, 0);
    lv_obj_set_style_border_width(txtCol, 0, 0);
    lv_obj_set_style_pad_all(txtCol, 0, 0);
    lv_obj_set_flex_flow(txtCol, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(txtCol, 2, 0);
    DefaultTheme::disableScroll(txtCol);

    currentNameLbl = lv_label_create(txtCol);
    lv_label_set_text(currentNameLbl, currentStation.name[0] ? currentStation.name : "Selecciona una emisora");
    lv_obj_set_style_text_font(currentNameLbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(currentNameLbl, DefaultTheme::getTextColor(), 0);
    lv_label_set_long_mode(currentNameLbl, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(currentNameLbl, 185);

    currentStatusLbl = lv_label_create(txtCol);
    lv_label_set_text(currentStatusLbl, isPlaying ? "Reproduciendo Stream" : "Detenido");
    lv_obj_set_style_text_font(currentStatusLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(currentStatusLbl, DefaultTheme::getMutedTextColor(), 0);

    // Botón Play / Stop
    playBtn = lv_button_create(playerBar);
    lv_obj_set_size(playBtn, 44, 44);
    DefaultTheme::applyButton(playBtn, LV_RADIUS_CIRCLE);
    lv_obj_set_style_bg_color(playBtn, isPlaying ? lv_color_hex(0x9E2B2B) : DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_add_event_cb(playBtn, play_pause_cb, LV_EVENT_CLICKED, nullptr);

    playBtnLbl = lv_label_create(playBtn);
    lv_label_set_text(playBtnLbl, isPlaying ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
    lv_obj_set_style_text_font(playBtnLbl, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(playBtnLbl, isPlaying ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x000000), 0);
    lv_obj_center(playBtnLbl);
}

void RadioView::buildSegmentedNav(lv_obj_t* parent) {
    lv_obj_t* navRow = lv_obj_create(parent);
    lv_obj_set_size(navRow, lv_pct(100), 34);
    lv_obj_set_style_bg_opa(navRow, 0, 0);
    lv_obj_set_style_border_width(navRow, 0, 0);
    lv_obj_set_style_pad_all(navRow, 0, 0);
    lv_obj_set_flex_flow(navRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(navRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    DefaultTheme::disableScroll(navRow);

    // Tab 0: Favoritas
    tabBtnFav = lv_button_create(navRow);
    lv_obj_set_size(tabBtnFav, 94, 30);
    DefaultTheme::applyButton(tabBtnFav, 8);
    lv_obj_set_user_data(tabBtnFav, (void*)(intptr_t)0);
    lv_obj_add_event_cb(tabBtnFav, tab_nav_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* l0 = lv_label_create(tabBtnFav);
    lv_label_set_text(l0, "Favoritas");
    lv_obj_set_style_text_font(l0, &lv_font_montserrat_12, 0);
    lv_obj_center(l0);

    // Tab 1: Explorar
    tabBtnExplore = lv_button_create(navRow);
    lv_obj_set_size(tabBtnExplore, 94, 30);
    DefaultTheme::applyButton(tabBtnExplore, 8);
    lv_obj_set_user_data(tabBtnExplore, (void*)(intptr_t)1);
    lv_obj_add_event_cb(tabBtnExplore, tab_nav_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* l1 = lv_label_create(tabBtnExplore);
    lv_label_set_text(l1, "Explorar");
    lv_obj_set_style_text_font(l1, &lv_font_montserrat_12, 0);
    lv_obj_center(l1);

    // Tab 2: Añadir
    tabBtnAdd = lv_button_create(navRow);
    lv_obj_set_size(tabBtnAdd, 94, 30);
    DefaultTheme::applyButton(tabBtnAdd, 8);
    lv_obj_set_user_data(tabBtnAdd, (void*)(intptr_t)2);
    lv_obj_add_event_cb(tabBtnAdd, tab_nav_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* l2 = lv_label_create(tabBtnAdd);
    lv_label_set_text(l2, "Anadir");
    lv_obj_set_style_text_font(l2, &lv_font_montserrat_12, 0);
    lv_obj_center(l2);
}

void RadioView::showTab(int tabIndex) {
    if (!favContainer || !exploreContainer || !addContainer) return;

    // Actualizar estilos de los botones de pestañas
    lv_obj_set_style_bg_color(tabBtnFav,     tabIndex == 0 ? DefaultTheme::getPrimaryAccent() : lv_color_hex(0x282C3C), 0);
    lv_obj_set_style_text_color(tabBtnFav,   tabIndex == 0 ? lv_color_hex(0x000000) : DefaultTheme::getTextColor(), 0);

    lv_obj_set_style_bg_color(tabBtnExplore, tabIndex == 1 ? DefaultTheme::getPrimaryAccent() : lv_color_hex(0x282C3C), 0);
    lv_obj_set_style_text_color(tabBtnExplore, tabIndex == 1 ? lv_color_hex(0x000000) : DefaultTheme::getTextColor(), 0);

    lv_obj_set_style_bg_color(tabBtnAdd,     tabIndex == 2 ? DefaultTheme::getPrimaryAccent() : lv_color_hex(0x282C3C), 0);
    lv_obj_set_style_text_color(tabBtnAdd,   tabIndex == 2 ? lv_color_hex(0x000000) : DefaultTheme::getTextColor(), 0);

    // Conmutar visibilidad de los contenedores
    if (tabIndex == 0) {
        lv_obj_clear_flag(favContainer, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(exploreContainer, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(addContainer, LV_OBJ_FLAG_HIDDEN);
        refreshFavoritesUI();
    } else if (tabIndex == 1) {
        lv_obj_add_flag(favContainer, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(exploreContainer, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(addContainer, LV_OBJ_FLAG_HIDDEN);
        // Si no hay emisoras, mostramos algo vacío
        if (s_currentExploreStations.empty()) {
            refreshExploreUI(s_currentExploreStations);
        }
    } else {
        lv_obj_add_flag(favContainer, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(exploreContainer, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(addContainer, LV_OBJ_FLAG_HIDDEN);
    }
}

void RadioView::tab_nav_cb(lv_event_t* e) {
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
    int idx = (int)(intptr_t)lv_obj_get_user_data(btn);
    showTab(idx);
}

void RadioView::buildFavoritesView(lv_obj_t* parent) {
    favContainer = lv_obj_create(parent);
    lv_obj_set_size(favContainer, lv_pct(100), 330);
    DefaultTheme::applyRaisedCard(favContainer, 10);
    lv_obj_set_flex_flow(favContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(favContainer, 4, 0);
    lv_obj_set_style_pad_row(favContainer, 4, 0);
    lv_obj_add_flag(favContainer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(favContainer, LV_DIR_VER);
}

void RadioView::refreshFavoritesUI() {
    if (!favContainer || !lv_obj_is_valid(favContainer)) return;
    lv_obj_clean(favContainer);

    const auto& favs = RadioManager::getInstance().getFavorites();
    if (favs.empty()) {
        lv_obj_t* emptyLbl = lv_label_create(favContainer);
        lv_label_set_text(emptyLbl, "No hay emisoras favoritas guardadas.\nExplora la lista o anade una URL.");
        lv_obj_set_style_text_color(emptyLbl, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_align(emptyLbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(emptyLbl);
        return;
    }

    for (size_t i = 0; i < favs.size(); i++) {
        const auto& st = favs[i];

        lv_obj_t* row = lv_obj_create(favContainer);
        if (!row) continue;
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, LV_SIZE_CONTENT);
        DefaultTheme::applyRaisedCard(row, 6);
        lv_obj_set_style_pad_all(row, 4, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t* nameLbl = lv_label_create(row);
        if (nameLbl) {
            char txt[160];
            snprintf(txt, sizeof(txt), "%s\n%s | %s", st.name, st.genre, st.country);
            lv_label_set_text(nameLbl, txt);
            lv_obj_set_style_text_color(nameLbl, DefaultTheme::getTextColor(), 0);
            lv_obj_set_style_text_font(nameLbl, &lv_font_montserrat_12, 0);
            lv_obj_set_width(nameLbl, 185);
        }

        lv_obj_t* playItemBtn = lv_button_create(row);
        if (playItemBtn) {
            lv_obj_set_size(playItemBtn, 34, 34);
            DefaultTheme::applyButton(playItemBtn, 6);
            lv_obj_set_style_bg_color(playItemBtn, DefaultTheme::getPrimaryAccent(), 0);
            lv_obj_set_user_data(playItemBtn, (void*)(intptr_t)i);
            lv_obj_add_event_cb(playItemBtn, fav_play_cb, LV_EVENT_CLICKED, NULL);

            lv_obj_t* playIcon = lv_label_create(playItemBtn);
            lv_label_set_text(playIcon, LV_SYMBOL_PLAY);
            lv_obj_set_style_text_color(playIcon, lv_color_hex(0x000000), 0);
            lv_obj_center(playIcon);
        }

        lv_obj_t* delBtn = lv_button_create(row);
        if (delBtn) {
            lv_obj_set_size(delBtn, 34, 34);
            DefaultTheme::applyButton(delBtn, 6);
            lv_obj_set_style_bg_color(delBtn, lv_color_hex(0x2B1A24), 0);
            lv_obj_set_user_data(delBtn, (void*)(intptr_t)i);
            lv_obj_add_event_cb(delBtn, fav_delete_cb, LV_EVENT_CLICKED, NULL);

            lv_obj_t* trashIcon = lv_label_create(delBtn);
            lv_label_set_text(trashIcon, LV_SYMBOL_TRASH);
            lv_obj_set_style_text_color(trashIcon, lv_color_hex(0xFF4B6E), 0);
            lv_obj_center(trashIcon);
        }
    }
}

void RadioView::buildExploreView(lv_obj_t* parent) {
    exploreContainer = lv_obj_create(parent);
    lv_obj_set_size(exploreContainer, lv_pct(100), 330);
    DefaultTheme::applyRaisedCard(exploreContainer, 10);
    lv_obj_set_flex_flow(exploreContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(exploreContainer, 4, 0);
    lv_obj_set_style_pad_row(exploreContainer, 4, 0);

    // Fila superior: Input de búsqueda y Botón Buscar
    lv_obj_t* searchRow = lv_obj_create(exploreContainer);
    lv_obj_set_size(searchRow, lv_pct(100), 40);
    lv_obj_set_style_bg_opa(searchRow, 0, 0);
    lv_obj_set_style_border_width(searchRow, 0, 0);
    lv_obj_set_style_pad_all(searchRow, 0, 0);
    lv_obj_set_flex_flow(searchRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(searchRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    taSearch = lv_textarea_create(searchRow);
    lv_obj_set_size(taSearch, 220, 36);
    DefaultTheme::applySunkenCard(taSearch, 8);
    lv_textarea_set_placeholder_text(taSearch, "Ej. Rock, Jazz, 90s...");
    lv_textarea_set_one_line(taSearch, true);
    UIManager::attachKeyboard(taSearch);

    btnSearch = lv_button_create(searchRow);
    lv_obj_set_size(btnSearch, 70, 36);
    DefaultTheme::applyButton(btnSearch, 8);
    lv_obj_set_style_bg_color(btnSearch, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_add_event_cb(btnSearch, search_btn_cb, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t* lblSearch = lv_label_create(btnSearch);
    lv_label_set_text(lblSearch, "Buscar");
    lv_obj_set_style_text_color(lblSearch, lv_color_hex(0x000000), 0);
    lv_obj_center(lblSearch);

    // Fila de paginación
    lv_obj_t* pageRow = lv_obj_create(exploreContainer);
    lv_obj_set_size(pageRow, lv_pct(100), 34);
    lv_obj_set_style_bg_opa(pageRow, 0, 0);
    lv_obj_set_style_border_width(pageRow, 0, 0);
    lv_obj_set_style_pad_all(pageRow, 0, 0);
    lv_obj_set_flex_flow(pageRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(pageRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    btnPrevPage = lv_button_create(pageRow);
    lv_obj_set_size(btnPrevPage, 60, 30);
    DefaultTheme::applyButton(btnPrevPage, 6);
    lv_obj_set_style_bg_color(btnPrevPage, lv_color_hex(0x282C3C), 0);
    lv_obj_add_event_cb(btnPrevPage, nav_prev_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* pLbl = lv_label_create(btnPrevPage);
    lv_label_set_text(pLbl, LV_SYMBOL_LEFT);
    lv_obj_center(pLbl);

    pageLbl = lv_label_create(pageRow);
    lv_label_set_text(pageLbl, "Pagina 1");
    lv_obj_set_style_text_color(pageLbl, DefaultTheme::getMutedTextColor(), 0);

    btnNextPage = lv_button_create(pageRow);
    lv_obj_set_size(btnNextPage, 60, 30);
    DefaultTheme::applyButton(btnNextPage, 6);
    lv_obj_set_style_bg_color(btnNextPage, lv_color_hex(0x282C3C), 0);
    lv_obj_add_event_cb(btnNextPage, nav_next_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* nLbl = lv_label_create(btnNextPage);
    lv_label_set_text(nLbl, LV_SYMBOL_RIGHT);
    lv_obj_center(nLbl);

    // Lista vertical con scroll para las emisoras de la categoría
    exploreList = lv_obj_create(exploreContainer);
    lv_obj_set_size(exploreList, lv_pct(100), 225);
    lv_obj_set_flex_flow(exploreList, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(exploreList, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(exploreList, LV_DIR_VER);
    lv_obj_set_style_pad_all(exploreList, 2, 0);
    lv_obj_set_style_pad_row(exploreList, 4, 0);
    lv_obj_set_style_bg_opa(exploreList, 0, 0);
    lv_obj_set_style_border_width(exploreList, 0, 0);
}

static volatile bool s_isSearching = false;
static volatile bool s_searchFinished = false;
static std::string s_asyncQuery = "";
static size_t s_asyncOffset = 0;
static std::vector<RadioStation> s_asyncResults;

void RadioView::asyncSearchTask(void* param) {
    s_asyncResults = RadioManager::getInstance().searchStations(s_asyncQuery, s_asyncOffset, 10);
    s_searchFinished = true;
    vTaskDelete(NULL);
}

void RadioView::searchPollTimerCb(lv_timer_t* timer) {
    if (s_searchFinished) {
        s_searchFinished = false;
        lv_timer_delete(timer);
        s_isSearching = false;
        s_currentExploreStations = std::move(s_asyncResults);
        s_asyncResults.clear();

        if (pageLbl && lv_obj_is_valid(pageLbl)) {
            char buf[32];
            snprintf(buf, sizeof(buf), "Pagina %zu", (currentSearchOffset / 10) + 1);
            lv_label_set_text(pageLbl, buf);
        }

        refreshExploreUI(s_currentExploreStations);
    }
}

void RadioView::search_btn_cb(lv_event_t* e) {
    if (s_isSearching) return;
    if (taSearch) {
        currentSearchQuery = lv_textarea_get_text(taSearch);
        currentSearchOffset = 0;
        performSearch();
    }
}

void RadioView::nav_prev_cb(lv_event_t* e) {
    if (s_isSearching) return;
    if (currentSearchOffset >= 10) {
        currentSearchOffset -= 10;
        performSearch();
    }
}

void RadioView::nav_next_cb(lv_event_t* e) {
    if (s_isSearching) return;
    currentSearchOffset += 10;
    performSearch();
}

void RadioView::performSearch() {
    if (currentSearchQuery.empty()) {
        UIManager::showToast("Escribe algo para buscar");
        return;
    }

    if (s_isSearching) return;
    s_isSearching = true;
    s_searchFinished = false;
    s_asyncQuery = currentSearchQuery;
    s_asyncOffset = currentSearchOffset;

    // Mostrar estado de carga en la lista
    if (exploreList && lv_obj_is_valid(exploreList)) {
        lv_obj_clean(exploreList);
        lv_obj_t* loadingLbl = lv_label_create(exploreList);
        lv_label_set_text(loadingLbl, "Buscando emisoras en linea...");
        lv_obj_set_style_text_color(loadingLbl, DefaultTheme::getPrimaryAccent(), 0);
        lv_obj_center(loadingLbl);
    }

    // Iniciar temporizador de sondeo de LVGL
    lv_timer_create(searchPollTimerCb, 60, NULL);

    // Lanzar tarea de red en Core 0 con 6KB de stack dinámico
    BaseType_t ret = xTaskCreatePinnedToCore(
        asyncSearchTask,
        "RadioSearch",
        6144,
        NULL,
        1,
        NULL,
        0
    );

    if (ret != pdPASS) {
        s_isSearching = false;
        if (exploreList && lv_obj_is_valid(exploreList)) {
            lv_obj_clean(exploreList);
            lv_obj_t* errLbl = lv_label_create(exploreList);
            lv_label_set_text(errLbl, "Error al iniciar busqueda");
            lv_obj_set_style_text_color(errLbl, lv_color_hex(0xFF5555), 0);
            lv_obj_center(errLbl);
        }
    }
}

void RadioView::refreshExploreUI(const std::vector<RadioStation>& stations) {
    if (!exploreList || !lv_obj_is_valid(exploreList)) return;
    lv_obj_clean(exploreList);

    if (stations.empty()) {
        lv_obj_t* errLbl = lv_label_create(exploreList);
        lv_label_set_text(errLbl, "No hay emisoras en esta categoria.");
        lv_obj_set_style_text_color(errLbl, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_center(errLbl);
        return;
    }

    for (size_t i = 0; i < stations.size(); i++) {
        const auto& st = stations[i];

        lv_obj_t* row = lv_obj_create(exploreList);
        if (!row) continue;
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, LV_SIZE_CONTENT);
        DefaultTheme::applyRaisedCard(row, 6);
        lv_obj_set_style_pad_all(row, 4, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t* nameLbl = lv_label_create(row);
        if (nameLbl) {
            char txt[160];
            snprintf(txt, sizeof(txt), "%s\n%s | %s (%dk)", 
                     st.name[0] ? st.name : "Emisora", 
                     st.genre[0] ? st.genre : "General", 
                     st.country[0] ? st.country : "Online", 
                     st.bitrate);
            lv_label_set_text(nameLbl, txt);
            lv_obj_set_style_text_color(nameLbl, DefaultTheme::getTextColor(), 0);
            lv_obj_set_style_text_font(nameLbl, &lv_font_montserrat_12, 0);
            lv_obj_set_width(nameLbl, 185);
        }

        lv_obj_t* playItemBtn = lv_button_create(row);
        if (playItemBtn) {
            lv_obj_set_size(playItemBtn, 34, 34);
            DefaultTheme::applyButton(playItemBtn, 6);
            lv_obj_set_style_bg_color(playItemBtn, DefaultTheme::getPrimaryAccent(), 0);
            lv_obj_set_user_data(playItemBtn, (void*)(intptr_t)i);
            lv_obj_add_event_cb(playItemBtn, explore_play_cb, LV_EVENT_CLICKED, NULL);

            lv_obj_t* playIcon = lv_label_create(playItemBtn);
            lv_label_set_text(playIcon, LV_SYMBOL_PLAY);
            lv_obj_set_style_text_color(playIcon, lv_color_hex(0x000000), 0);
            lv_obj_center(playIcon);
        }

        lv_obj_t* favBtn = lv_button_create(row);
        if (favBtn) {
            lv_obj_set_size(favBtn, 34, 34);
            DefaultTheme::applyButton(favBtn, 6);
            lv_obj_set_style_bg_color(favBtn, st.isFavorite ? DefaultTheme::getSecondaryAccent() : lv_color_hex(0x282C3C), 0);
            lv_obj_set_user_data(favBtn, (void*)(intptr_t)i);
            lv_obj_add_event_cb(favBtn, explore_fav_cb, LV_EVENT_CLICKED, NULL);

            lv_obj_t* starIcon = lv_label_create(favBtn);
            lv_label_set_text(starIcon, LV_SYMBOL_PLUS);
            lv_obj_set_style_text_color(starIcon, lv_color_hex(0xFFB800), 0);
            lv_obj_center(starIcon);
        }
    }
}

void RadioView::buildAddManualView(lv_obj_t* parent) {
    addContainer = lv_obj_create(parent);
    lv_obj_set_size(addContainer, lv_pct(100), 330);
    DefaultTheme::applyRaisedCard(addContainer, 10);
    lv_obj_set_flex_flow(addContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(addContainer, 6, 0);
    lv_obj_set_style_pad_row(addContainer, 6, 0);

    lv_obj_t* nameHeader = lv_label_create(addContainer);
    lv_label_set_text(nameHeader, "Nombre de la Emisora:");
    lv_obj_set_style_text_color(nameHeader, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(nameHeader, &lv_font_montserrat_12, 0);

    taName = lv_textarea_create(addContainer);
    lv_obj_set_width(taName, lv_pct(100));
    lv_obj_set_height(taName, 36);
    DefaultTheme::applySunkenCard(taName, 8);
    lv_textarea_set_placeholder_text(taName, "Ej. Mi Radio");
    lv_textarea_set_one_line(taName, true);
    lv_obj_set_style_text_color(taName, DefaultTheme::getTextColor(), 0);
    UIManager::attachKeyboard(taName);

    lv_obj_t* urlHeader = lv_label_create(addContainer);
    lv_label_set_text(urlHeader, "URL Stream MP3 (http://...):");
    lv_obj_set_style_text_color(urlHeader, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(urlHeader, &lv_font_montserrat_12, 0);

    taUrl = lv_textarea_create(addContainer);
    lv_obj_set_width(taUrl, lv_pct(100));
    lv_obj_set_height(taUrl, 36);
    DefaultTheme::applySunkenCard(taUrl, 8);
    lv_textarea_set_text(taUrl, "http://");
    lv_textarea_set_one_line(taUrl, true);
    lv_obj_set_style_text_color(taUrl, DefaultTheme::getTextColor(), 0);
    UIManager::attachKeyboard(taUrl);

    lv_obj_t* genreHeader = lv_label_create(addContainer);
    lv_label_set_text(genreHeader, "Genero / Categoria:");
    lv_obj_set_style_text_color(genreHeader, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(genreHeader, &lv_font_montserrat_12, 0);

    taGenre = lv_textarea_create(addContainer);
    lv_obj_set_width(taGenre, lv_pct(100));
    lv_obj_set_height(taGenre, 36);
    DefaultTheme::applySunkenCard(taGenre, 8);
    lv_textarea_set_placeholder_text(taGenre, "Ej. Rock, Reggae");
    lv_textarea_set_one_line(taGenre, true);
    lv_obj_set_style_text_color(taGenre, DefaultTheme::getTextColor(), 0);
    UIManager::attachKeyboard(taGenre);

    lv_obj_t* saveBtn = lv_button_create(addContainer);
    lv_obj_set_width(saveBtn, lv_pct(100));
    lv_obj_set_height(saveBtn, 40);
    DefaultTheme::applyButton(saveBtn, 10);
    lv_obj_set_style_bg_color(saveBtn, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_add_event_cb(saveBtn, add_save_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* saveLbl = lv_label_create(saveBtn);
    lv_label_set_text(saveLbl, "Guardar en Favoritas");
    lv_obj_set_style_text_color(saveLbl, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(saveLbl, &lv_font_montserrat_14, 0);
    lv_obj_center(saveLbl);
}

void RadioView::playStation(const RadioStation& station) {
    currentStation = station;
    isPlaying = true;

    if (currentNameLbl && lv_obj_is_valid(currentNameLbl)) {
        lv_label_set_text(currentNameLbl, station.name);
    }
    if (currentStatusLbl && lv_obj_is_valid(currentStatusLbl)) {
        char stBuf[64];
        snprintf(stBuf, sizeof(stBuf), "Conectando stream (%s)...", station.genre);
        lv_label_set_text(currentStatusLbl, stBuf);
    }
    if (playBtn && lv_obj_is_valid(playBtn)) {
        lv_obj_set_style_bg_color(playBtn, lv_color_hex(0x9E2B2B), 0);
    }
    if (playBtnLbl && lv_obj_is_valid(playBtnLbl)) {
        lv_label_set_text(playBtnLbl, LV_SYMBOL_PAUSE);
        lv_obj_set_style_text_color(playBtnLbl, lv_color_hex(0xFFFFFF), 0);
    }

    NativeAudioDriver::getInstance().playStream(station.url);
}

void RadioView::stopStream() {
    isPlaying = false;
    NativeAudioDriver::getInstance().stop();

    if (currentStatusLbl && lv_obj_is_valid(currentStatusLbl)) {
        lv_label_set_text(currentStatusLbl, "Detenido");
    }
    if (playBtn && lv_obj_is_valid(playBtn)) {
        lv_obj_set_style_bg_color(playBtn, DefaultTheme::getPrimaryAccent(), 0);
    }
    if (playBtnLbl && lv_obj_is_valid(playBtnLbl)) {
        lv_label_set_text(playBtnLbl, LV_SYMBOL_PLAY);
        lv_obj_set_style_text_color(playBtnLbl, lv_color_hex(0x000000), 0);
    }
}

void RadioView::play_pause_cb(lv_event_t* e) {
    if (isPlaying) {
        stopStream();
    } else {
        if (currentStation.url[0] != '\0') {
            playStation(currentStation);
        } else {
            const auto& favs = RadioManager::getInstance().getFavorites();
            if (!favs.empty()) {
                playStation(favs[0]);
            } else {
                UIManager::showToast("Selecciona una emisora primero");
            }
        }
    }
}

void RadioView::fav_play_cb(lv_event_t* e) {
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
    size_t idx = (size_t)(intptr_t)lv_obj_get_user_data(btn);
    const auto& favs = RadioManager::getInstance().getFavorites();
    if (idx < favs.size()) {
        playStation(favs[idx]);
    }
}

void RadioView::fav_delete_cb(lv_event_t* e) {
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
    size_t idx = (size_t)(intptr_t)lv_obj_get_user_data(btn);
    RadioManager::getInstance().removeFavorite(idx);
    UIManager::showToast("Emisora eliminada");
    refreshFavoritesUI();
}

void RadioView::explore_play_cb(lv_event_t* e) {
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
    size_t idx = (size_t)(intptr_t)lv_obj_get_user_data(btn);
    if (idx < s_currentExploreStations.size()) {
        playStation(s_currentExploreStations[idx]);
    }
}

void RadioView::explore_fav_cb(lv_event_t* e) {
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
    size_t idx = (size_t)(intptr_t)lv_obj_get_user_data(btn);
    if (idx < s_currentExploreStations.size()) {
        RadioManager::getInstance().addFavorite(s_currentExploreStations[idx]);
        s_currentExploreStations[idx].isFavorite = true;
        lv_obj_set_style_bg_color(btn, DefaultTheme::getSecondaryAccent(), 0);
        UIManager::showToast("Guardada en Favoritas");
    }
}

void RadioView::add_save_cb(lv_event_t* e) {
    if (!taName || !taUrl) return;
    const char* name = lv_textarea_get_text(taName);
    const char* url = lv_textarea_get_text(taUrl);
    const char* genre = taGenre ? lv_textarea_get_text(taGenre) : "Web";

    if (name && strlen(name) > 0 && url && strlen(url) > 5) {
        RadioStation st;
        strncpy(st.name, name, sizeof(st.name) - 1);
        strncpy(st.url, url, sizeof(st.url) - 1);
        strncpy(st.genre, (genre && strlen(genre) > 0) ? genre : "Personalizada", sizeof(st.genre) - 1);
        strncpy(st.country, "Manual", sizeof(st.country) - 1);
        st.bitrate = 128;
        st.isFavorite = true;

        RadioManager::getInstance().addFavorite(st);
        UIManager::showToast("Emisora agregada a Favoritas");

        lv_textarea_set_text(taName, "");
        lv_textarea_set_text(taUrl, "http://");
        if (taGenre) lv_textarea_set_text(taGenre, "");

        showTab(0); // Volver a Favoritas
        refreshFavoritesUI();
    } else {
        UIManager::showToast("Completa los datos requeridos");
    }
}
