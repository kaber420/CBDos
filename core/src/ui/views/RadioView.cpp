#include "RadioView.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include "cbdos/audio.hpp"
#include "cbdos/display.hpp"
#include "cbdos/network.hpp"
#include "cbdos/system.hpp"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>
#include <cstdio>

namespace cbdos {
namespace ui {

static const char* TAG = "RadioView";

RadioView* RadioView::s_activeInstance = nullptr;
audio::RadioStation RadioView::s_currentStation;
bool RadioView::s_isPlaying = false;
size_t RadioView::s_currentSearchOffset = 0;
std::string RadioView::s_currentSearchQuery = "";
std::vector<audio::RadioStation> RadioView::s_currentExploreStations;
volatile bool RadioView::s_searchInProgress = false;
volatile bool RadioView::s_searchCompleted = false;
std::vector<audio::RadioStation> RadioView::s_asyncSearchResults;

RadioView::RadioView()
    : BaseView("Radio Online") {
    s_activeInstance = this;
}

bool RadioView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;

    // Inicializar subsistema de radio y favoritos
    audio::RadioManager::getInstance().init();

    // Mostrar icono de WiFi en la barra superior
    UIManager::getInstance().getHeaderBar().showWifi(true);

    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(m_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_container, 8, 0);
    lv_obj_set_style_pad_row(m_container, 8, 0);
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_container, 0, 0);
    DefaultTheme::disableScroll(m_container);

    // 1. Barra de Reproducción Superior
    buildPlayerBar(m_container);

    // 2. Selector Segmentado de Pestañas (Favoritas / Explorar / Agregar)
    buildSegmentedNav(m_container);

    // 3. Contenedor de Favoritas
    buildFavoritesView(m_container);

    // 4. Contenedor de Explorar (Online Search)
    buildExploreView(m_container);

    // 5. Contenedor de Agregar Manualmente
    buildAddManualView(m_container);

    // Iniciar en la pestaña Favoritas
    showTab(0);

    return true;
}

void RadioView::onDestroy() {
    if (m_searchPollTimer) {
        lv_timer_delete(m_searchPollTimer);
        m_searchPollTimer = nullptr;
    }
    s_activeInstance = nullptr;
    BaseView::onDestroy();
}

void RadioView::buildPlayerBar(lv_obj_t* parent) {
    m_playerBar = lv_obj_create(parent);
    lv_obj_set_width(m_playerBar, lv_pct(100));
    lv_obj_set_height(m_playerBar, 64);
    DefaultTheme::applyRaisedCard(m_playerBar, 12);
    lv_obj_set_flex_flow(m_playerBar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(m_playerBar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(m_playerBar, 8, 0);
    DefaultTheme::disableScroll(m_playerBar);

    // Columna izquierda: Icono + Textos (Nombre y Estado)
    lv_obj_t* leftCol = lv_obj_create(m_playerBar);
    lv_obj_set_size(leftCol, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(leftCol, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(leftCol, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(leftCol, 0, 0);
    lv_obj_set_style_pad_column(leftCol, 8, 0);
    lv_obj_set_style_bg_opa(leftCol, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(leftCol, 0, 0);
    DefaultTheme::disableScroll(leftCol);

    lv_obj_t* radioIcon = lv_label_create(leftCol);
    lv_label_set_text(radioIcon, LV_SYMBOL_AUDIO);
    lv_obj_set_style_text_color(radioIcon, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_set_style_text_font(radioIcon, &lv_font_montserrat_24, 0);

    lv_obj_t* textCol = lv_obj_create(leftCol);
    lv_obj_set_size(textCol, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(textCol, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(textCol, 0, 0);
    lv_obj_set_style_pad_row(textCol, 2, 0);
    lv_obj_set_style_bg_opa(textCol, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(textCol, 0, 0);
    DefaultTheme::disableScroll(textCol);

    m_currentNameLbl = lv_label_create(textCol);
    lv_label_set_text(m_currentNameLbl, s_currentStation.name.empty() ? "Selecciona una emisora" : s_currentStation.name.c_str());
    lv_obj_set_style_text_color(m_currentNameLbl, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(m_currentNameLbl, &lv_font_montserrat_14, 0);
    lv_label_set_long_mode(m_currentNameLbl, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(m_currentNameLbl, 250);

    m_currentStatusLbl = lv_label_create(textCol);
    lv_label_set_text(m_currentStatusLbl, s_isPlaying ? "Reproduciendo stream..." : "Detenido");
    lv_obj_set_style_text_color(m_currentStatusLbl, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(m_currentStatusLbl, &lv_font_montserrat_12, 0);

    // Botón Play / Pause circular
    m_playBtn = lv_button_create(m_playerBar);
    lv_obj_set_size(m_playBtn, 44, 44);
    DefaultTheme::applyButton(m_playBtn, 22);
    lv_obj_set_style_bg_color(m_playBtn, s_isPlaying ? lv_color_hex(0xEF4444) : DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_add_event_cb(m_playBtn, playPauseCb, LV_EVENT_CLICKED, this);

    m_playBtnLbl = lv_label_create(m_playBtn);
    lv_label_set_text(m_playBtnLbl, s_isPlaying ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
    lv_obj_set_style_text_color(m_playBtnLbl, s_isPlaying ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x000000), 0);
    lv_obj_center(m_playBtnLbl);
}

void RadioView::buildSegmentedNav(lv_obj_t* parent) {
    lv_obj_t* navRow = lv_obj_create(parent);
    lv_obj_set_width(navRow, lv_pct(100));
    lv_obj_set_height(navRow, 40);
    DefaultTheme::applySunkenCard(navRow, 10);
    lv_obj_set_flex_flow(navRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(navRow, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(navRow, 2, 0);
    DefaultTheme::disableScroll(navRow);

    auto createNavBtn = [&](const char* title, int id) -> lv_obj_t* {
        lv_obj_t* btn = lv_button_create(navRow);
        lv_obj_set_size(btn, lv_pct(32), 34);
        DefaultTheme::applyButton(btn, 8);
        lv_obj_set_user_data(btn, (void*)(intptr_t)id);
        lv_obj_add_event_cb(btn, tabNavCb, LV_EVENT_CLICKED, this);

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, title);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_center(lbl);
        return btn;
    };

    m_tabBtnFav = createNavBtn("Favoritas", 0);
    m_tabBtnExplore = createNavBtn("Explorar", 1);
    m_tabBtnAdd = createNavBtn("Agregar", 2);
}

void RadioView::buildFavoritesView(lv_obj_t* parent) {
    m_favContainer = lv_obj_create(parent);
    lv_obj_set_width(m_favContainer, lv_pct(100));
    lv_obj_set_flex_grow(m_favContainer, 1);
    lv_obj_set_flex_flow(m_favContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_favContainer, 4, 0);
    lv_obj_set_style_pad_row(m_favContainer, 6, 0);
    lv_obj_set_style_bg_opa(m_favContainer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_favContainer, 0, 0);

    refreshFavoritesUI();
}

void RadioView::refreshFavoritesUI() {
    if (!m_favContainer) return;
    lv_obj_clean(m_favContainer);

    const auto& favorites = audio::RadioManager::getInstance().getFavorites();

    if (favorites.empty()) {
        lv_obj_t* emptyLbl = lv_label_create(m_favContainer);
        lv_label_set_text(emptyLbl, "No tienes emisoras favoritas aun.\nExplora o agrega tus preferidas.");
        lv_obj_set_style_text_color(emptyLbl, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_align(emptyLbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_margin_top(emptyLbl, 40, 0);
        lv_obj_center(emptyLbl);
        return;
    }

    for (size_t i = 0; i < favorites.size(); ++i) {
        const auto& st = favorites[i];

        lv_obj_t* row = lv_obj_create(m_favContainer);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, LV_SIZE_CONTENT);
        DefaultTheme::applyRaisedCard(row, 8);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(row, 6, 0);
        DefaultTheme::disableScroll(row);

        // Bloque de información (Texto unificado estilo espOS32)
        lv_obj_t* nameLbl = lv_label_create(row);
        char txt[160];
        snprintf(txt, sizeof(txt), "%s\n%s | %s", st.name.c_str(), st.genre.c_str(), st.country.c_str());
        lv_label_set_text(nameLbl, txt);
        lv_obj_set_style_text_color(nameLbl, DefaultTheme::getTextColor(), 0);
        lv_obj_set_style_text_font(nameLbl, &lv_font_montserrat_12, 0);
        lv_obj_set_width(nameLbl, 240);

        // Botones de acción derecha (Play & Delete)
        lv_obj_t* actionsRow = lv_obj_create(row);
        lv_obj_set_size(actionsRow, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(actionsRow, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_all(actionsRow, 0, 0);
        lv_obj_set_style_pad_column(actionsRow, 6, 0);
        lv_obj_set_style_bg_opa(actionsRow, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(actionsRow, 0, 0);
        DefaultTheme::disableScroll(actionsRow);

        // Botón Play
        lv_obj_t* btnPlay = lv_button_create(actionsRow);
        lv_obj_set_size(btnPlay, 36, 36);
        DefaultTheme::applyButton(btnPlay, 18);
        lv_obj_set_style_bg_color(btnPlay, DefaultTheme::getPrimaryAccent(), 0);
        lv_obj_set_user_data(btnPlay, (void*)(intptr_t)i);
        lv_obj_add_event_cb(btnPlay, favPlayCb, LV_EVENT_CLICKED, this);

        lv_obj_t* playIcon = lv_label_create(btnPlay);
        lv_label_set_text(playIcon, LV_SYMBOL_PLAY);
        lv_obj_set_style_text_color(playIcon, lv_color_hex(0x000000), 0);
        lv_obj_center(playIcon);

        // Botón Delete
        lv_obj_t* btnDel = lv_button_create(actionsRow);
        lv_obj_set_size(btnDel, 36, 36);
        DefaultTheme::applyButton(btnDel, 8);
        lv_obj_set_style_bg_color(btnDel, lv_color_hex(0x282C3C), 0);
        lv_obj_set_user_data(btnDel, (void*)(intptr_t)i);
        lv_obj_add_event_cb(btnDel, favDeleteCb, LV_EVENT_CLICKED, this);

        lv_obj_t* delIcon = lv_label_create(btnDel);
        lv_label_set_text(delIcon, LV_SYMBOL_TRASH);
        lv_obj_set_style_text_color(delIcon, lv_color_hex(0xEF4444), 0);
        lv_obj_center(delIcon);
    }
}

void RadioView::buildExploreView(lv_obj_t* parent) {
    m_exploreContainer = lv_obj_create(parent);
    lv_obj_set_width(m_exploreContainer, lv_pct(100));
    lv_obj_set_flex_grow(m_exploreContainer, 1);
    lv_obj_set_flex_flow(m_exploreContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_exploreContainer, 4, 0);
    lv_obj_set_style_pad_row(m_exploreContainer, 6, 0);
    lv_obj_set_style_bg_opa(m_exploreContainer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_exploreContainer, 0, 0);

    // Barra de Búsqueda
    lv_obj_t* searchRow = lv_obj_create(m_exploreContainer);
    lv_obj_set_width(searchRow, lv_pct(100));
    lv_obj_set_height(searchRow, 42);
    lv_obj_set_flex_flow(searchRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(searchRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(searchRow, 0, 0);
    lv_obj_set_style_bg_opa(searchRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(searchRow, 0, 0);
    DefaultTheme::disableScroll(searchRow);

    m_taSearch = lv_textarea_create(searchRow);
    lv_obj_set_width(m_taSearch, lv_pct(76));
    lv_obj_set_height(m_taSearch, 40);
    DefaultTheme::applySunkenCard(m_taSearch, 8);
    lv_textarea_set_placeholder_text(m_taSearch, "Buscar emisora o genero...");
    lv_textarea_set_one_line(m_taSearch, true);
    lv_obj_set_style_text_color(m_taSearch, DefaultTheme::getTextColor(), 0);
    UIManager::attachKeyboard(m_taSearch);

    m_btnSearch = lv_button_create(searchRow);
    lv_obj_set_width(m_btnSearch, lv_pct(22));
    lv_obj_set_height(m_btnSearch, 40);
    DefaultTheme::applyButton(m_btnSearch, 8);
    lv_obj_set_style_bg_color(m_btnSearch, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_add_event_cb(m_btnSearch, searchBtnCb, LV_EVENT_CLICKED, this);

    lv_obj_t* searchLbl = lv_label_create(m_btnSearch);
    lv_label_set_text(searchLbl, "Buscar");
    lv_obj_set_style_text_color(searchLbl, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(searchLbl, &lv_font_montserrat_12, 0);
    lv_obj_center(searchLbl);

    // Contenedor de Resultados con Scroll
    m_exploreList = lv_obj_create(m_exploreContainer);
    lv_obj_set_width(m_exploreList, lv_pct(100));
    lv_obj_set_flex_grow(m_exploreList, 1);
    lv_obj_set_flex_flow(m_exploreList, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_exploreList, 0, 0);
    lv_obj_set_style_pad_row(m_exploreList, 6, 0);
    lv_obj_set_style_bg_opa(m_exploreList, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_exploreList, 0, 0);

    // Barra de Paginación Inferior
    lv_obj_t* pageRow = lv_obj_create(m_exploreContainer);
    lv_obj_set_width(pageRow, lv_pct(100));
    lv_obj_set_height(pageRow, 38);
    lv_obj_set_flex_flow(pageRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(pageRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(pageRow, 0, 0);
    lv_obj_set_style_bg_opa(pageRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(pageRow, 0, 0);
    DefaultTheme::disableScroll(pageRow);

    m_btnPrevPage = lv_button_create(pageRow);
    lv_obj_set_size(m_btnPrevPage, 90, 36);
    DefaultTheme::applyButton(m_btnPrevPage, 8);
    lv_obj_add_event_cb(m_btnPrevPage, navPrevCb, LV_EVENT_CLICKED, this);
    lv_obj_t* prevLbl = lv_label_create(m_btnPrevPage);
    lv_label_set_text(prevLbl, LV_SYMBOL_LEFT " Ant");
    lv_obj_center(prevLbl);

    m_pageLbl = lv_label_create(pageRow);
    lv_label_set_text(m_pageLbl, "Pagina 1");
    lv_obj_set_style_text_color(m_pageLbl, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(m_pageLbl, &lv_font_montserrat_12, 0);

    m_btnNextPage = lv_button_create(pageRow);
    lv_obj_set_size(m_btnNextPage, 90, 36);
    DefaultTheme::applyButton(m_btnNextPage, 8);
    lv_obj_add_event_cb(m_btnNextPage, navNextCb, LV_EVENT_CLICKED, this);
    lv_obj_t* nextLbl = lv_label_create(m_btnNextPage);
    lv_label_set_text(nextLbl, "Sig " LV_SYMBOL_RIGHT);
    lv_obj_center(nextLbl);
}

void RadioView::refreshExploreUI(const std::vector<audio::RadioStation>& stations) {
    if (!m_exploreList) return;
    lv_obj_clean(m_exploreList);

    if (stations.empty()) {
        lv_obj_t* emptyLbl = lv_label_create(m_exploreList);
        lv_label_set_text(emptyLbl, "Escribe un genero o nombre y toca 'Buscar'.");
        lv_obj_set_style_text_color(emptyLbl, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_margin_top(emptyLbl, 30, 0);
        lv_obj_center(emptyLbl);
        return;
    }

    for (size_t i = 0; i < stations.size(); ++i) {
        const auto& st = stations[i];

        lv_obj_t* row = lv_obj_create(m_exploreList);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, LV_SIZE_CONTENT);
        DefaultTheme::applyRaisedCard(row, 8);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(row, 6, 0);
        DefaultTheme::disableScroll(row);

        // Info unificada estilo espOS32
        lv_obj_t* nameLbl = lv_label_create(row);
        char txt[160];
        snprintf(txt, sizeof(txt), "%s\n%s | %s (%dk)", st.name.c_str(), st.genre.c_str(), st.country.c_str(), st.bitrate);
        lv_label_set_text(nameLbl, txt);
        lv_obj_set_style_text_color(nameLbl, DefaultTheme::getTextColor(), 0);
        lv_obj_set_style_text_font(nameLbl, &lv_font_montserrat_12, 0);
        lv_obj_set_width(nameLbl, 240);

        // Actions Row
        lv_obj_t* actionsRow = lv_obj_create(row);
        lv_obj_set_size(actionsRow, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(actionsRow, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_all(actionsRow, 0, 0);
        lv_obj_set_style_pad_column(actionsRow, 6, 0);
        lv_obj_set_style_bg_opa(actionsRow, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(actionsRow, 0, 0);
        DefaultTheme::disableScroll(actionsRow);

        // Play Button
        lv_obj_t* btnPlay = lv_button_create(actionsRow);
        lv_obj_set_size(btnPlay, 36, 36);
        DefaultTheme::applyButton(btnPlay, 18);
        lv_obj_set_style_bg_color(btnPlay, DefaultTheme::getPrimaryAccent(), 0);
        lv_obj_set_user_data(btnPlay, (void*)(intptr_t)i);
        lv_obj_add_event_cb(btnPlay, explorePlayCb, LV_EVENT_CLICKED, this);

        lv_obj_t* playIcon = lv_label_create(btnPlay);
        lv_label_set_text(playIcon, LV_SYMBOL_PLAY);
        lv_obj_set_style_text_color(playIcon, lv_color_hex(0x000000), 0);
        lv_obj_center(playIcon);

        // Favorite Button (+)
        lv_obj_t* btnFav = lv_button_create(actionsRow);
        lv_obj_set_size(btnFav, 36, 36);
        DefaultTheme::applyButton(btnFav, 8);
        lv_obj_set_style_bg_color(btnFav, st.isFavorite ? DefaultTheme::getSecondaryAccent() : lv_color_hex(0x282C3C), 0);
        lv_obj_set_user_data(btnFav, (void*)(intptr_t)i);
        lv_obj_add_event_cb(btnFav, exploreFavCb, LV_EVENT_CLICKED, this);

        lv_obj_t* starIcon = lv_label_create(btnFav);
        lv_label_set_text(starIcon, LV_SYMBOL_PLUS);
        lv_obj_set_style_text_color(starIcon, lv_color_hex(0xFFB800), 0);
        lv_obj_center(starIcon);
    }
}

void RadioView::buildAddManualView(lv_obj_t* parent) {
    m_addContainer = lv_obj_create(parent);
    lv_obj_set_width(m_addContainer, lv_pct(100));
    lv_obj_set_flex_grow(m_addContainer, 1);
    DefaultTheme::applyRaisedCard(m_addContainer, 12);
    lv_obj_set_flex_flow(m_addContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_addContainer, 10, 0);
    lv_obj_set_style_pad_row(m_addContainer, 8, 0);

    lv_obj_t* nameHeader = lv_label_create(m_addContainer);
    lv_label_set_text(nameHeader, "Nombre de la Emisora:");
    lv_obj_set_style_text_color(nameHeader, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(nameHeader, &lv_font_montserrat_12, 0);

    m_taName = lv_textarea_create(m_addContainer);
    lv_obj_set_width(m_taName, lv_pct(100));
    lv_obj_set_height(m_taName, 40);
    DefaultTheme::applySunkenCard(m_taName, 8);
    lv_textarea_set_placeholder_text(m_taName, "Ej. Mi Radio Chill");
    lv_textarea_set_one_line(m_taName, true);
    lv_obj_set_style_text_color(m_taName, DefaultTheme::getTextColor(), 0);
    UIManager::attachKeyboard(m_taName);

    lv_obj_t* urlHeader = lv_label_create(m_addContainer);
    lv_label_set_text(urlHeader, "URL Stream MP3 (http://...):");
    lv_obj_set_style_text_color(urlHeader, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(urlHeader, &lv_font_montserrat_12, 0);

    m_taUrl = lv_textarea_create(m_addContainer);
    lv_obj_set_width(m_taUrl, lv_pct(100));
    lv_obj_set_height(m_taUrl, 40);
    DefaultTheme::applySunkenCard(m_taUrl, 8);
    lv_textarea_set_text(m_taUrl, "http://");
    lv_textarea_set_one_line(m_taUrl, true);
    lv_obj_set_style_text_color(m_taUrl, DefaultTheme::getTextColor(), 0);
    UIManager::attachKeyboard(m_taUrl);

    lv_obj_t* genreHeader = lv_label_create(m_addContainer);
    lv_label_set_text(genreHeader, "Genero / Categoria:");
    lv_obj_set_style_text_color(genreHeader, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(genreHeader, &lv_font_montserrat_12, 0);

    m_taGenre = lv_textarea_create(m_addContainer);
    lv_obj_set_width(m_taGenre, lv_pct(100));
    lv_obj_set_height(m_taGenre, 40);
    DefaultTheme::applySunkenCard(m_taGenre, 8);
    lv_textarea_set_placeholder_text(m_taGenre, "Ej. Electronic / Jazz");
    lv_textarea_set_one_line(m_taGenre, true);
    lv_obj_set_style_text_color(m_taGenre, DefaultTheme::getTextColor(), 0);
    UIManager::attachKeyboard(m_taGenre);

    lv_obj_t* saveBtn = lv_button_create(m_addContainer);
    lv_obj_set_width(saveBtn, lv_pct(100));
    lv_obj_set_height(saveBtn, 44);
    DefaultTheme::applyButton(saveBtn, 10);
    lv_obj_set_style_bg_color(saveBtn, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_add_event_cb(saveBtn, addSaveCb, LV_EVENT_CLICKED, this);

    lv_obj_t* saveLbl = lv_label_create(saveBtn);
    lv_label_set_text(saveLbl, "Guardar en Favoritas");
    lv_obj_set_style_text_color(saveLbl, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(saveLbl, &lv_font_montserrat_14, 0);
    lv_obj_center(saveLbl);
}

void RadioView::showTab(int tabIndex) {
    if (m_favContainer) lv_obj_add_flag(m_favContainer, LV_OBJ_FLAG_HIDDEN);
    if (m_exploreContainer) lv_obj_add_flag(m_exploreContainer, LV_OBJ_FLAG_HIDDEN);
    if (m_addContainer) lv_obj_add_flag(m_addContainer, LV_OBJ_FLAG_HIDDEN);

    auto updateBtnStyle = [](lv_obj_t* btn, bool active) {
        if (!btn) return;
        lv_obj_set_style_bg_color(btn, active ? DefaultTheme::getPrimaryAccent() : lv_color_hex(0x1F2430), 0);
        lv_obj_t* lbl = lv_obj_get_child(btn, 0);
        if (lbl) {
            lv_obj_set_style_text_color(lbl, active ? lv_color_hex(0x000000) : DefaultTheme::getMutedTextColor(), 0);
        }
    };

    updateBtnStyle(m_tabBtnFav, tabIndex == 0);
    updateBtnStyle(m_tabBtnExplore, tabIndex == 1);
    updateBtnStyle(m_tabBtnAdd, tabIndex == 2);

    if (tabIndex == 0) {
        if (m_favContainer) {
            lv_obj_remove_flag(m_favContainer, LV_OBJ_FLAG_HIDDEN);
            refreshFavoritesUI();
        }
    } else if (tabIndex == 1) {
        if (m_favContainer) lv_obj_add_flag(m_favContainer, LV_OBJ_FLAG_HIDDEN);
        if (m_exploreContainer) lv_obj_remove_flag(m_exploreContainer, LV_OBJ_FLAG_HIDDEN);
        if (m_addContainer) lv_obj_add_flag(m_addContainer, LV_OBJ_FLAG_HIDDEN);
        if (s_currentExploreStations.empty()) {
            refreshExploreUI(s_currentExploreStations);
        }
    } else if (tabIndex == 2) {
        if (m_favContainer) lv_obj_add_flag(m_favContainer, LV_OBJ_FLAG_HIDDEN);
        if (m_exploreContainer) lv_obj_add_flag(m_exploreContainer, LV_OBJ_FLAG_HIDDEN);
        if (m_addContainer) lv_obj_remove_flag(m_addContainer, LV_OBJ_FLAG_HIDDEN);
    }
}

void RadioView::playStation(const audio::RadioStation& station) {
    // Verificar que la red esté lista antes de intentar streaming
    if (!cbdos::network::isConnected()) {
        UIManager::showToast("Sin conexion WiFi. Conecta primero.");
        return;
    }

    s_currentStation = station;
    s_isPlaying = true;

    if (s_activeInstance) {
        if (s_activeInstance->m_currentNameLbl && lv_obj_is_valid(s_activeInstance->m_currentNameLbl)) {
            lv_label_set_text(s_activeInstance->m_currentNameLbl, station.name.c_str());
        }
        if (s_activeInstance->m_currentStatusLbl && lv_obj_is_valid(s_activeInstance->m_currentStatusLbl)) {
            char stBuf[64];
            snprintf(stBuf, sizeof(stBuf), "Conectando stream (%s)...", station.genre.c_str());
            lv_label_set_text(s_activeInstance->m_currentStatusLbl, stBuf);
        }
        if (s_activeInstance->m_playBtn && lv_obj_is_valid(s_activeInstance->m_playBtn)) {
            lv_obj_set_style_bg_color(s_activeInstance->m_playBtn, lv_color_hex(0xEF4444), 0);
        }
        if (s_activeInstance->m_playBtnLbl && lv_obj_is_valid(s_activeInstance->m_playBtnLbl)) {
            lv_label_set_text(s_activeInstance->m_playBtnLbl, LV_SYMBOL_PAUSE);
            lv_obj_set_style_text_color(s_activeInstance->m_playBtnLbl, lv_color_hex(0xFFFFFF), 0);
        }
    }

    cbdos::audio::playStream(station.url.c_str());
}

void RadioView::stopStream() {
    s_isPlaying = false;
    cbdos::audio::stop();

    if (s_activeInstance) {
        if (s_activeInstance->m_currentStatusLbl && lv_obj_is_valid(s_activeInstance->m_currentStatusLbl)) {
            lv_label_set_text(s_activeInstance->m_currentStatusLbl, "Detenido");
        }
        if (s_activeInstance->m_playBtn && lv_obj_is_valid(s_activeInstance->m_playBtn)) {
            lv_obj_set_style_bg_color(s_activeInstance->m_playBtn, DefaultTheme::getPrimaryAccent(), 0);
        }
        if (s_activeInstance->m_playBtnLbl && lv_obj_is_valid(s_activeInstance->m_playBtnLbl)) {
            lv_label_set_text(s_activeInstance->m_playBtnLbl, LV_SYMBOL_PLAY);
            lv_obj_set_style_text_color(s_activeInstance->m_playBtnLbl, lv_color_hex(0x000000), 0);
        }
    }
}

void RadioView::performSearch() {
    if (s_searchInProgress) return;

    if (!cbdos::network::isConnected()) {
        UIManager::showToast("Sin conexion WiFi. Conecta primero.");
        return;
    }

    if (s_currentSearchQuery.empty()) {
        UIManager::showToast("Escribe algo para buscar");
        return;
    }

    s_searchInProgress = true;
    s_searchCompleted = false;

    // Mostrar estado de carga en la lista (exacto a espOS32)
    if (m_exploreList && lv_obj_is_valid(m_exploreList)) {
        lv_obj_clean(m_exploreList);
        lv_obj_t* loadingLbl = lv_label_create(m_exploreList);
        lv_label_set_text(loadingLbl, "Buscando emisoras en linea...");
        lv_obj_set_style_text_color(loadingLbl, DefaultTheme::getPrimaryAccent(), 0);
        lv_obj_center(loadingLbl);
    }

    if (m_currentStatusLbl && lv_obj_is_valid(m_currentStatusLbl)) {
        lv_label_set_text(m_currentStatusLbl, "Buscando emisoras...");
    }

    // Iniciar temporizador de sondeo de LVGL (60ms exacto a espOS32)
    m_searchPollTimer = lv_timer_create(searchPollTimerCb, 60, this);

    // Lanzar tarea de red en Core 0 con prioridad 1
    BaseType_t ret = xTaskCreatePinnedToCore(
        asyncSearchTask,
        "RadioSearch",
        8192,
        NULL,
        1,
        NULL,
        0
    );

    if (ret != pdPASS) {
        s_searchInProgress = false;
        if (m_searchPollTimer) {
            lv_timer_delete(m_searchPollTimer);
            m_searchPollTimer = nullptr;
        }
        if (m_exploreList && lv_obj_is_valid(m_exploreList)) {
            lv_obj_clean(m_exploreList);
            lv_obj_t* errLbl = lv_label_create(m_exploreList);
            lv_label_set_text(errLbl, "Error al iniciar busqueda");
            lv_obj_set_style_text_color(errLbl, lv_color_hex(0xFF5555), 0);
            lv_obj_center(errLbl);
        }
    }
}

void RadioView::asyncSearchTask(void* param) {
    s_asyncSearchResults = audio::RadioManager::getInstance().searchStations(
        s_currentSearchQuery, s_currentSearchOffset, 10);
    s_searchCompleted = true;
    vTaskDelete(NULL);
}

void RadioView::searchPollTimerCb(lv_timer_t* timer) {
    if (s_searchCompleted) {
        s_searchCompleted = false;
        lv_timer_delete(timer);
        s_searchInProgress = false;

        RadioView* self = static_cast<RadioView*>(lv_timer_get_user_data(timer));
        if (self) {
            self->m_searchPollTimer = nullptr;
            s_currentExploreStations = std::move(s_asyncSearchResults);
            s_asyncSearchResults.clear();

            if (self->m_pageLbl && lv_obj_is_valid(self->m_pageLbl)) {
                char pBuf[32];
                snprintf(pBuf, sizeof(pBuf), "Pagina %d", (int)(s_currentSearchOffset / 10) + 1);
                lv_label_set_text(self->m_pageLbl, pBuf);
            }
            if (self->m_currentStatusLbl && lv_obj_is_valid(self->m_currentStatusLbl)) {
                lv_label_set_text(self->m_currentStatusLbl, s_isPlaying ? "Reproduciendo" : "Listo");
            }

            self->refreshExploreUI(s_currentExploreStations);
        }
    }
}

void RadioView::tabNavCb(lv_event_t* e) {
    RadioView* self = static_cast<RadioView*>(lv_event_get_user_data(e));
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_current_target(e);
    int tabId = (int)(intptr_t)lv_obj_get_user_data(btn);
    if (self) self->showTab(tabId);
}

void RadioView::playPauseCb(lv_event_t* e) {
    if (s_isPlaying) {
        stopStream();
    } else {
        if (!s_currentStation.url.empty()) {
            playStation(s_currentStation);
        } else {
            const auto& favs = audio::RadioManager::getInstance().getFavorites();
            if (!favs.empty()) {
                playStation(favs[0]);
            } else {
                UIManager::showToast("Selecciona una emisora primero");
            }
        }
    }
}

void RadioView::favPlayCb(lv_event_t* e) {
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_current_target(e);
    size_t idx = (size_t)(intptr_t)lv_obj_get_user_data(btn);
    const auto& favs = audio::RadioManager::getInstance().getFavorites();
    if (idx < favs.size()) {
        playStation(favs[idx]);
    }
}

void RadioView::favDeleteCb(lv_event_t* e) {
    RadioView* self = static_cast<RadioView*>(lv_event_get_user_data(e));
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_current_target(e);
    size_t idx = (size_t)(intptr_t)lv_obj_get_user_data(btn);
    audio::RadioManager::getInstance().removeFavorite(idx);
    UIManager::showToast("Emisora eliminada");
    // Diferir la reconstrucción del UI para evitar use-after-free
    // (el botón que disparó el evento sigue vivo durante el callback)
    if (self) {
        lv_async_call([](void* userData) {
            RadioView* view = static_cast<RadioView*>(userData);
            if (view) view->refreshFavoritesUI();
        }, self);
    }
}

void RadioView::searchBtnCb(lv_event_t* e) {
    RadioView* self = static_cast<RadioView*>(lv_event_get_user_data(e));
    if (!self || !self->m_taSearch) return;

    const char* txt = lv_textarea_get_text(self->m_taSearch);
    if (!txt || strlen(txt) == 0) {
        UIManager::showToast("Escribe algo para buscar");
        return;
    }
    s_currentSearchQuery = txt;
    s_currentSearchOffset = 0;
    self->performSearch();
}

void RadioView::navPrevCb(lv_event_t* e) {
    RadioView* self = static_cast<RadioView*>(lv_event_get_user_data(e));
    if (s_currentSearchOffset >= 10) {
        s_currentSearchOffset -= 10;
        if (self) self->performSearch();
    }
}

void RadioView::navNextCb(lv_event_t* e) {
    RadioView* self = static_cast<RadioView*>(lv_event_get_user_data(e));
    s_currentSearchOffset += 10;
    if (self) self->performSearch();
}

void RadioView::explorePlayCb(lv_event_t* e) {
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_current_target(e);
    size_t idx = (size_t)(intptr_t)lv_obj_get_user_data(btn);
    if (idx < s_currentExploreStations.size()) {
        playStation(s_currentExploreStations[idx]);
    }
}

void RadioView::exploreFavCb(lv_event_t* e) {
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_current_target(e);
    size_t idx = (size_t)(intptr_t)lv_obj_get_user_data(btn);
    if (idx < s_currentExploreStations.size()) {
        audio::RadioManager::getInstance().addFavorite(s_currentExploreStations[idx]);
        s_currentExploreStations[idx].isFavorite = true;
        lv_obj_set_style_bg_color(btn, DefaultTheme::getSecondaryAccent(), 0);
        UIManager::showToast("Guardada en Favoritas");
    }
}

void RadioView::addSaveCb(lv_event_t* e) {
    RadioView* self = static_cast<RadioView*>(lv_event_get_user_data(e));
    if (!self || !self->m_taName || !self->m_taUrl) return;

    const char* name = lv_textarea_get_text(self->m_taName);
    const char* url = lv_textarea_get_text(self->m_taUrl);
    const char* genre = self->m_taGenre ? lv_textarea_get_text(self->m_taGenre) : "Web";

    if (name && strlen(name) > 0 && url && strlen(url) > 5) {
        audio::RadioStation st;
        st.name = name;
        st.url = url;
        st.genre = (genre && strlen(genre) > 0) ? genre : "Personalizada";
        st.country = "Manual";
        st.bitrate = 128;
        st.isFavorite = true;

        audio::RadioManager::getInstance().addFavorite(st);
        UIManager::showToast("Emisora agregada a Favoritas");

        lv_textarea_set_text(self->m_taName, "");
        lv_textarea_set_text(self->m_taUrl, "http://");
        if (self->m_taGenre) lv_textarea_set_text(self->m_taGenre, "");

        self->showTab(0);
    } else {
        UIManager::showToast("Completa los datos requeridos");
    }
}

void RadioView::onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) {
    if (!m_container || !lv_obj_is_valid(m_container)) return;
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
}

} // namespace ui
} // namespace cbdos
