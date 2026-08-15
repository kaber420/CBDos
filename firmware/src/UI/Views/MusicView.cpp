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

lv_obj_t* MusicView::listContainer  = nullptr;
lv_obj_t* MusicView::playerCard      = nullptr;
lv_obj_t* MusicView::playBtn         = nullptr;
lv_obj_t* MusicView::playBtnLabel    = nullptr;
lv_obj_t* MusicView::titleLabel      = nullptr;
lv_obj_t* MusicView::statusLabel     = nullptr;
lv_obj_t* MusicView::mainIconLabel   = nullptr;
lv_obj_t* MusicView::navHeaderBtn    = nullptr;
lv_obj_t* MusicView::navHeaderLbl    = nullptr;

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

    HeaderBarConfig cfg;
    cfg.title = "Musica SD";
    cfg.showBackButton = true;
    cfg.showTime = true;
    cfg.showWifi = false;
    cfg.showCartButton = false;
    cfg.titleMarquee = false;
    cfg.translucent = false;

    headerBar = HeaderBar::create(scr, cfg);
    HeaderBar::setActiveHeader(headerBar);

    // Botón de navegación en la cabecera (Lista SD <-> Reproductor)
    navHeaderBtn = lv_button_create(scr);
    lv_obj_set_size(navHeaderBtn, 95, 30);
    lv_obj_align(navHeaderBtn, LV_ALIGN_TOP_RIGHT, -8, 7);
    DefaultTheme::applyButton(navHeaderBtn, 10);
    lv_obj_add_event_cb(navHeaderBtn, nav_header_cb, LV_EVENT_CLICKED, nullptr);

    navHeaderLbl = lv_label_create(navHeaderBtn);
    lv_obj_set_style_text_font(navHeaderLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(navHeaderLbl, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_center(navHeaderLbl);

    // Contenedor principal de la lista SD (con scroll)
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

    // Icono grande de música
    mainIconLabel = lv_label_create(playerCard);
    lv_label_set_text(mainIconLabel, LV_SYMBOL_AUDIO);
    lv_obj_set_style_text_font(mainIconLabel, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(mainIconLabel, DefaultTheme::getPrimaryAccent(), 0);

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

    updateNavHeaderBtn();

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
        lv_obj_add_flag(itemBtn, LV_OBJ_FLAG_EVENT_BUBBLE);
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

void MusicView::updateNavHeaderBtn() {
    if (!navHeaderLbl || !playerCard) return;
    if (lv_obj_has_flag(playerCard, LV_OBJ_FLAG_HIDDEN)) {
        lv_label_set_text(navHeaderLbl, LV_SYMBOL_AUDIO " Player");
    } else {
        lv_label_set_text(navHeaderLbl, LV_SYMBOL_LIST " Lista");
    }
}

void MusicView::startTrack(int index) {
    if (index < 0 || index >= (int)playlist.size()) return;
    currentTrackIndex = index;

    lv_label_set_text(titleLabel, playlist[index].name.c_str());
    lv_label_set_text(statusLabel, "Reproduciendo desde SD...");
    if (mainIconLabel) lv_label_set_text(mainIconLabel, LV_SYMBOL_AUDIO);
    lv_label_set_text(playBtnLabel, LV_SYMBOL_PAUSE);

    NativeAudioDriver::getInstance().playMP3(playlist[index].path.c_str());
    isPlaying = true;

    showPlayerScreen();
}

void MusicView::showPlayerScreen() {
    if (currentTrackIndex >= 0 && currentTrackIndex < (int)playlist.size()) {
        lv_label_set_text(titleLabel, playlist[currentTrackIndex].name.c_str());
    } else {
        lv_label_set_text(titleLabel, "Selecciona una cancion");
        lv_label_set_text(statusLabel, "Listo");
    }

    lv_obj_add_flag(listContainer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(playerCard, LV_OBJ_FLAG_HIDDEN);
    updateNavHeaderBtn();
}

void MusicView::showListScreen() {
    lv_obj_add_flag(playerCard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(listContainer, LV_OBJ_FLAG_HIDDEN);
    updateNavHeaderBtn();
}

void MusicView::track_click_cb(lv_event_t* e) {
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
    int index = (int)(intptr_t)lv_obj_get_user_data(btn);
    startTrack(index);
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
    startTrack(prevIdx);
}

void MusicView::next_track_cb(lv_event_t* e) {
    if (playlist.empty()) return;
    int nextIdx = (currentTrackIndex + 1) % playlist.size();
    startTrack(nextIdx);
}

void MusicView::nav_header_cb(lv_event_t* e) {
    if (lv_obj_has_flag(playerCard, LV_OBJ_FLAG_HIDDEN)) {
        if (currentTrackIndex < 0 && !playlist.empty()) {
            currentTrackIndex = 0;
        }
        showPlayerScreen();
    } else {
        showListScreen();
    }
}
