#pragma once
#include <lvgl.h>
#include <string>
#include <vector>
#include "../Components/HeaderBar.h"

struct TrackItem {
    std::string name;
    std::string path;
};

class MusicView {
public:
    static lv_obj_t* create();

private:
    static HeaderBar* headerBar;
    static std::vector<TrackItem> playlist;
    static int currentTrackIndex;
    static bool isPlaying;

    static lv_obj_t* listContainer;
    static lv_obj_t* playerCard;

    static lv_obj_t* playBtn;
    static lv_obj_t* playBtnLabel;
    static lv_obj_t* titleLabel;
    static lv_obj_t* statusLabel;

    static lv_obj_t* navHeaderBtn;
    static lv_obj_t* navHeaderLbl;

    static void scanAudioFilesSD();
    static void renderPlaylist(lv_obj_t* parent);
    static void startTrack(int index);
    static void showPlayerScreen();
    static void showListScreen();
    static void updateNavHeaderBtn();

    static void track_click_cb(lv_event_t* e);
    static void play_pause_cb(lv_event_t* e);
    static void prev_track_cb(lv_event_t* e);
    static void next_track_cb(lv_event_t* e);
    static void back_to_list_cb(lv_event_t* e);
    static void nav_header_cb(lv_event_t* e);
};
