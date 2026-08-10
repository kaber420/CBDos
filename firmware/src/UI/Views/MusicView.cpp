#include "MusicView.h"
#include "../Themes/DefaultTheme.h"
#include "../UIManager.h"
#include "../../Core/NativeAudioDriver.h"
#include "../../Core/LVFS_Driver.h"
#ifdef ARDUINO
#include <SD.h>
#endif

HeaderBar*              MusicView::headerBar         = nullptr;
std::vector<TrackItem>  MusicView::playlist;
int                     MusicView::currentTrackIndex = -1;
bool                    MusicView::isPlaying         = false;

lv_obj_t* MusicView::listContainer = nullptr;
lv_obj_t* MusicView::playerCard     = nullptr;
lv_obj_t* MusicView::playBtn        = nullptr;
lv_obj_t* MusicView::playBtnLabel   = nullptr;
lv_obj_t* MusicView::titleLabel     = nullptr;
lv_obj_t* MusicView::statusLabel    = nullptr;

void MusicView::scanAudioFilesSD() {
    playlist.clear();
#ifdef ARDUINO
    if (SD.cardType() != CARD_NONE) {
        lv_fs_spi_lock();
        File root = SD.open("/");
        if (root) {
            File entry = root.openNextFile();
            while (entry) {
                String fileName = entry.name();
                String lowerName = fileName;
                lowerName.toLowerCase();

                if (!entry.isDirectory()) {
                    if (lowerName.endsWith(".mp3") || lowerName.endsWith(".wav")) {
                        playlist.push_back({fileName.c_str(), "A:/" + std::string(fileName.c_str())});
                    }
                } else {
                    File subDir = SD.open("/" + fileName);
                    if (subDir) {
                        File subEntry = subDir.openNextFile();
                        while (subEntry) {
                            String subName = subEntry.name();
                            String subLower = subName;
                            subLower.toLowerCase();
                            if (!subEntry.isDirectory()) {
                                if (subLower.endsWith(".mp3") || subLower.endsWith(".wav")) {
                                    playlist.push_back({
                                        subName.c_str(),
                                        "A:/" + std::string(fileName.c_str()) + "/" + std::string(subName.c_str())
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
}

lv_obj_t* MusicView::create() {
    scanAudioFilesSD();
    isPlaying = false;
    currentTrackIndex = -1;

    lv_obj_t* scr = lv_obj_create(NULL);
    DefaultTheme::applyFlatBg(scr);

    headerBar = HeaderBar::create(scr, "Reproductor de Musica", true, true);
    HeaderBar::setActiveHeader(headerBar);

    // Contenedor principal de la lista (con scroll)
    listContainer = lv_obj_create(scr);
    lv_obj_set_size(listContainer, LV_PCT(92), LV_PCT(78));
    lv_obj_align(listContainer, LV_ALIGN_CENTER, 0, 15);
    DefaultTheme::applyRaisedCard(listContainer);
    lv_obj_set_flex_flow(listContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(listContainer, 10, 0);
    lv_obj_set_style_pad_row(listContainer, 8, 0);
    lv_obj_add_flag(listContainer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(listContainer, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(listContainer, LV_SCROLLBAR_MODE_ACTIVE);

    renderPlaylist(listContainer);

    // Contenedor del Reproductor (inicialmente oculto)
    playerCard = lv_obj_create(scr);
    lv_obj_set_size(playerCard, LV_PCT(92), LV_PCT(78));
    lv_obj_align(playerCard, LV_ALIGN_CENTER, 0, 15);
    DefaultTheme::applyRaisedCard(playerCard);
    lv_obj_set_flex_flow(playerCard, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(playerCard, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(playerCard, LV_OBJ_FLAG_HIDDEN);

    // ── Elementos del Reproductor ──
    lv_obj_t* topRow = lv_obj_create(playerCard);
    lv_obj_set_size(topRow, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(topRow, 0, 0);
    lv_obj_set_style_border_width(topRow, 0, 0);
    lv_obj_set_style_pad_all(topRow, 0, 0);

    lv_obj_t* backListBtn = lv_button_create(topRow);
    lv_obj_set_size(backListBtn, 90, 32);
    lv_obj_align(backListBtn, LV_ALIGN_LEFT_MID, 0, 0);
    DefaultTheme::applyButton(backListBtn, 10);
    lv_obj_add_event_cb(backListBtn, back_to_list_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* backLbl = lv_label_create(backListBtn);
    lv_label_set_text(backLbl, LV_SYMBOL_LIST " Lista");
    lv_obj_set_style_text_font(backLbl, &lv_font_montserrat_12, 0);
    lv_obj_center(backLbl);

    // Icono grande de música
    lv_obj_t* iconLbl = lv_label_create(playerCard);
    lv_label_set_text(iconLbl, LV_SYMBOL_AUDIO);
    lv_obj_set_style_text_font(iconLbl, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(iconLbl, DefaultTheme::getPrimaryAccent(), 0);

    // Título de la canción
    titleLabel = lv_label_create(playerCard);
    lv_label_set_text(titleLabel, "Selecciona una cancion");
    lv_obj_set_style_text_font(titleLabel, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(titleLabel, DefaultTheme::getTextColor(), 0);
    lv_label_set_long_mode(titleLabel, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(titleLabel, LV_PCT(90));

    statusLabel = lv_label_create(playerCard);
    lv_label_set_text(statusLabel, "Listo");
    lv_obj_set_style_text_font(statusLabel, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(statusLabel, DefaultTheme::getMutedTextColor(), 0);

    // Fila de Controles [ ◀◀ ] [ ▶ / ❚❚ ] [ ▶▶ ]
    lv_obj_t* ctrlRow = lv_obj_create(playerCard);
    lv_obj_set_size(ctrlRow, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(ctrlRow, 0, 0);
    lv_obj_set_style_border_width(ctrlRow, 0, 0);
    lv_obj_set_flex_flow(ctrlRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ctrlRow, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(ctrlRow, 16, 0);

    // Anterior
    lv_obj_t* prevBtn = lv_button_create(ctrlRow);
    lv_obj_set_size(prevBtn, 50, 50);
    DefaultTheme::applyButton(prevBtn, LV_RADIUS_CIRCLE);
    lv_obj_add_event_cb(prevBtn, prev_track_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* prevLbl = lv_label_create(prevBtn);
    lv_label_set_text(prevLbl, LV_SYMBOL_PREV);
    lv_obj_center(prevLbl);

    // Play/Pause
    playBtn = lv_button_create(ctrlRow);
    lv_obj_set_size(playBtn, 65, 65);
    DefaultTheme::applyButton(playBtn, LV_RADIUS_CIRCLE);
    lv_obj_add_event_cb(playBtn, play_pause_cb, LV_EVENT_CLICKED, nullptr);
    playBtnLabel = lv_label_create(playBtn);
    lv_label_set_text(playBtnLabel, LV_SYMBOL_PLAY);
    lv_obj_set_style_text_font(playBtnLabel, &lv_font_montserrat_24, 0);
    lv_obj_center(playBtnLabel);

    // Siguiente
    lv_obj_t* nextBtn = lv_button_create(ctrlRow);
    lv_obj_set_size(nextBtn, 50, 50);
    DefaultTheme::applyButton(nextBtn, LV_RADIUS_CIRCLE);
    lv_obj_add_event_cb(nextBtn, next_track_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* nextLbl = lv_label_create(nextBtn);
    lv_label_set_text(nextLbl, LV_SYMBOL_NEXT);
    lv_obj_center(nextLbl);

    return scr;
}

void MusicView::renderPlaylist(lv_obj_t* parent) {
    if (playlist.empty()) {
        lv_obj_t* emptyLbl = lv_label_create(parent);
        lv_label_set_text(emptyLbl, "No se encontraron canciones en la SD (.mp3 / .wav)");
        lv_obj_set_style_text_color(emptyLbl, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_center(emptyLbl);
        return;
    }

    for (size_t i = 0; i < playlist.size(); i++) {
        lv_obj_t* itemBtn = lv_button_create(parent);
        lv_obj_set_size(itemBtn, LV_PCT(100), 45);
        DefaultTheme::applyButton(itemBtn, 10);
        lv_obj_set_user_data(itemBtn, (void*)(intptr_t)i);
        lv_obj_add_flag(itemBtn, LV_OBJ_FLAG_EVENT_BUBBLE); // propaga el arrastre al padre para scroll
        lv_obj_add_event_cb(itemBtn, track_click_cb, LV_EVENT_CLICKED, nullptr);

        lv_obj_set_flex_flow(itemBtn, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(itemBtn, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t* icon = lv_label_create(itemBtn);
        lv_label_set_text(icon, LV_SYMBOL_AUDIO);
        lv_obj_set_style_text_color(icon, DefaultTheme::getPrimaryAccent(), 0);

        lv_obj_t* nameLbl = lv_label_create(itemBtn);
        lv_label_set_text(nameLbl, playlist[i].name.c_str());
        lv_obj_set_style_text_color(nameLbl, DefaultTheme::getTextColor(), 0);
        lv_obj_set_style_text_font(nameLbl, &lv_font_montserrat_14, 0);
        lv_label_set_long_mode(nameLbl, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_width(nameLbl, LV_PCT(80));
    }
}

void MusicView::showPlayerScreen(int index) {
    if (index < 0 || index >= (int)playlist.size()) return;
    currentTrackIndex = index;

    lv_label_set_text(titleLabel, playlist[index].name.c_str());
    lv_label_set_text(statusLabel, "Reproduciendo...");
    lv_label_set_text(playBtnLabel, LV_SYMBOL_PAUSE);

    NativeAudioDriver::getInstance().playMP3(playlist[index].path.c_str());
    isPlaying = true;

    lv_obj_add_flag(listContainer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(playerCard, LV_OBJ_FLAG_HIDDEN);
}

void MusicView::showListScreen() {
    lv_obj_add_flag(playerCard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(listContainer, LV_OBJ_FLAG_HIDDEN);
}

void MusicView::track_click_cb(lv_event_t* e) {
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
    int index = (int)(intptr_t)lv_obj_get_user_data(btn);
    showPlayerScreen(index);
}

void MusicView::play_pause_cb(lv_event_t* e) {
    if (currentTrackIndex < 0 || currentTrackIndex >= (int)playlist.size()) return;

    isPlaying = !isPlaying;
    if (isPlaying) {
        NativeAudioDriver::getInstance().playMP3(playlist[currentTrackIndex].path.c_str());
        lv_label_set_text(playBtnLabel, LV_SYMBOL_PAUSE);
        lv_label_set_text(statusLabel, "Reproduciendo...");
    } else {
        NativeAudioDriver::getInstance().stop();
        lv_label_set_text(playBtnLabel, LV_SYMBOL_PLAY);
        lv_label_set_text(statusLabel, "Pausado");
    }
}

void MusicView::prev_track_cb(lv_event_t* e) {
    if (playlist.empty()) return;
    int prevIdx = currentTrackIndex - 1;
    if (prevIdx < 0) prevIdx = playlist.size() - 1;
    showPlayerScreen(prevIdx);
}

void MusicView::next_track_cb(lv_event_t* e) {
    if (playlist.empty()) return;
    int nextIdx = (currentTrackIndex + 1) % playlist.size();
    showPlayerScreen(nextIdx);
}

void MusicView::back_to_list_cb(lv_event_t* e) {
    showListScreen();
}
