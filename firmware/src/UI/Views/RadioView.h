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
    static size_t currentTrioIndex;
    static std::string activeExploreCategory;
    static lv_obj_t* trioBtn1;
    static lv_obj_t* trioLbl1;
    static lv_obj_t* trioBtn2;
    static lv_obj_t* trioLbl2;
    static lv_obj_t* trioBtn3;
    static lv_obj_t* trioLbl3;
    static lv_obj_t* exploreList;

    // Textareas para añadir manual
    static lv_obj_t* taName;
    static lv_obj_t* taUrl;
    static lv_obj_t* taGenre;

    static void buildPlayerBar(lv_obj_t* parent);
    static void buildSegmentedNav(lv_obj_t* parent);
    static void buildFavoritesView(lv_obj_t* parent);
    static void buildExploreView(lv_obj_t* parent);
    static void buildAddManualView(lv_obj_t* parent);

    static void showTab(int tabIndex); // 0 = Fav, 1 = Explore, 2 = Add
    static void refreshFavoritesUI();
    static void refreshExploreUI(const std::vector<RadioStation>& stations);
    static void updateTrioButtons();

    // Callbacks
    static void play_pause_cb(lv_event_t* e);
    static void tab_nav_cb(lv_event_t* e);
    static void trio_nav_prev_cb(lv_event_t* e);
    static void trio_nav_next_cb(lv_event_t* e);
    static void trio_chip_cb(lv_event_t* e);
    static void fav_play_cb(lv_event_t* e);
    static void fav_delete_cb(lv_event_t* e);
    static void explore_play_cb(lv_event_t* e);
    static void explore_fav_cb(lv_event_t* e);
    static void add_save_cb(lv_event_t* e);
};
