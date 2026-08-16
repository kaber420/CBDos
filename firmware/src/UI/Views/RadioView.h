#pragma once

#include <lvgl.h>
#include <string>
#include <vector>
#include "../Components/HeaderBar.h"
#include "Audio/RadioManager.h"

class RadioView {
public:
    static lv_obj_t* create();
    static void playStation(const RadioStation& station);
    static void stopStream();

private:
    static HeaderBar* headerBar;
    static RadioStation currentStation;
    static bool isPlaying;

    // Barra de Reproducción Superior
    static lv_obj_t* playerBar;
    static lv_obj_t* currentNameLbl;
    static lv_obj_t* currentStatusLbl;
    static lv_obj_t* playBtn;
    static lv_obj_t* playBtnLbl;

    // Botones de las 3 Pestañas Principales
    static lv_obj_t* tabBtnFav;
    static lv_obj_t* tabBtnExplore;
    static lv_obj_t* tabBtnAdd;

    // 3 Contenedores de Vistas
    static lv_obj_t* favContainer;
    static lv_obj_t* exploreContainer;
    static lv_obj_t* addContainer;

    // Elementos de la Pestaña Explorar (Tríos Paginados)
    static size_t                  currentSearchOffset;
    static std::string             currentSearchQuery;

    static lv_obj_t*               taSearch;
    static lv_obj_t*               btnSearch;
    static lv_obj_t*               btnPrevPage;
    static lv_obj_t*               btnNextPage;
    static lv_obj_t*               pageLbl;
    static lv_obj_t*               exploreList;

    static lv_obj_t*               taName;
    static lv_obj_t*               taUrl;
    static lv_obj_t*               taGenre;

    static void                    buildPlayerBar(lv_obj_t* parent);
    static void                    buildSegmentedNav(lv_obj_t* parent);
    static void                    buildFavoritesView(lv_obj_t* parent);
    static void                    buildExploreView(lv_obj_t* parent);
    static void                    buildAddManualView(lv_obj_t* parent);

    static void                    showTab(int tabIndex);
    static void                    refreshFavoritesUI();
    static void                    refreshExploreUI(const std::vector<RadioStation>& stations);
    static void                    performSearch();

    // Callbacks
    static void                    tab_nav_cb(lv_event_t* e);
    static void                    play_pause_cb(lv_event_t* e);
    
    static void                    fav_play_cb(lv_event_t* e);
    static void                    fav_delete_cb(lv_event_t* e);

    static void                    search_btn_cb(lv_event_t* e);
    static void                    nav_prev_cb(lv_event_t* e);
    static void                    nav_next_cb(lv_event_t* e);
    static void                    searchPollTimerCb(lv_timer_t* timer);
    static void                    asyncSearchTask(void* param);

    static void                    explore_play_cb(lv_event_t* e);
    static void                    explore_fav_cb(lv_event_t* e);

    static void                    add_save_cb(lv_event_t* e);
};
