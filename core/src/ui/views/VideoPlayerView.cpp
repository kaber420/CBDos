#include "VideoPlayerView.hpp"
#include "cbdos/storage.hpp"
#include "cbdos/display.hpp"
#include "cbdos/audio.hpp"
#include "cbdos/system.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <esp_log.h>

#if defined(ESP_PLATFORM)
#include <esp_heap_caps.h>
#endif

namespace cbdos {
namespace ui {

static const char* TAG = "VideoPlayer";

VideoPlayerView::VideoPlayerView()
    : BaseView("Reproductor de Video") {
}

VideoPlayerView::~VideoPlayerView() {
    stopVideo();
    if (m_frameBuffer) {
        free(m_frameBuffer);
        m_frameBuffer = nullptr;
    }
}

bool VideoPlayerView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;

    scanVideoFilesSD();
    m_isPlaying = false;
    m_currentVideoIndex = -1;
    m_controlsVisible = true;
    m_isCurrentMp4 = false;

    UIManager::getInstance().getHeaderBar().showWifi(false);

    // 1. Contenedor principal de la vista
    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_container, 0, 0);
    lv_obj_set_style_pad_all(m_container, 0, 0);
    DefaultTheme::disableScroll(m_container);

    // 2. Contenedor de Lista de Videos (Playlist)
    m_listContainer = lv_obj_create(m_container);
    lv_obj_set_size(m_listContainer, LV_PCT(100), LV_PCT(100));
    lv_obj_align(m_listContainer, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(m_listContainer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_listContainer, 0, 0);
    lv_obj_set_flex_flow(m_listContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_listContainer, 12, 0);
    lv_obj_set_style_pad_row(m_listContainer, 10, 0);
    lv_obj_add_flag(m_listContainer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(m_listContainer, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(m_listContainer, LV_SCROLLBAR_MODE_ACTIVE);

    renderPlaylist(m_listContainer);

    // 3. Contenedor del Reproductor de Video
    m_playerContainer = lv_obj_create(m_container);
    lv_obj_set_size(m_playerContainer, LV_PCT(100), LV_PCT(100));
    lv_obj_align(m_playerContainer, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(m_playerContainer, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(m_playerContainer, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(m_playerContainer, 0, 0);
    lv_obj_set_style_pad_all(m_playerContainer, 0, 0);
    lv_obj_add_flag(m_playerContainer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(m_playerContainer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(m_playerContainer, screenTouchCb, LV_EVENT_CLICKED, this);
    DefaultTheme::disableScroll(m_playerContainer);

    // Canvas / Área de renderizado del frame de video
    auto caps = cbdos::display::getCapabilities();
    m_canvasWidth = caps.width > 0 ? caps.width : 480;
    m_canvasHeight = caps.height > 0 ? (caps.height - 40) : 760;

    m_canvas = lv_canvas_create(m_playerContainer);
    lv_obj_align(m_canvas, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_size(m_canvas, m_canvasWidth, m_canvasHeight);

    m_frameBufferSize = m_canvasWidth * m_canvasHeight * 2; // RGB565 (2 bytes/pixel)
#if defined(ESP_PLATFORM)
    m_frameBuffer = (uint8_t*)heap_caps_malloc(m_frameBufferSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    m_frameBuffer = (uint8_t*)malloc(m_frameBufferSize);
#endif
    if (!m_frameBuffer) {
        m_frameBuffer = (uint8_t*)malloc(m_frameBufferSize);
    }
    if (m_frameBuffer) {
        memset(m_frameBuffer, 0, m_frameBufferSize);
        lv_canvas_set_buffer(m_canvas, m_frameBuffer, m_canvasWidth, m_canvasHeight, LV_COLOR_FORMAT_RGB565);
    }

    // Overlay de Controles Flotante (Glassmorphism)
    m_overlayControls = lv_obj_create(m_playerContainer);
    lv_obj_set_size(m_overlayControls, LV_PCT(92), 110);
    lv_obj_align(m_overlayControls, LV_ALIGN_BOTTOM_MID, 0, -12);
    lv_obj_set_style_bg_color(m_overlayControls, lv_color_make(20, 20, 30), 0);
    lv_obj_set_style_bg_opa(m_overlayControls, LV_OPA_80, 0);
    lv_obj_set_style_border_color(m_overlayControls, lv_color_make(60, 60, 80), 0);
    lv_obj_set_style_border_width(m_overlayControls, 1, 0);
    lv_obj_set_style_radius(m_overlayControls, 16, 0);
    lv_obj_set_style_pad_all(m_overlayControls, 10, 0);
    DefaultTheme::disableScroll(m_overlayControls);

    // Título del video
    m_titleLabel = lv_label_create(m_overlayControls);
    lv_label_set_text(m_titleLabel, "Video");
    lv_obj_set_style_text_font(m_titleLabel, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(m_titleLabel, lv_color_white(), 0);
    lv_label_set_long_mode(m_titleLabel, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(m_titleLabel, LV_PCT(60));
    lv_obj_align(m_titleLabel, LV_ALIGN_TOP_LEFT, 4, 0);

    // Tiempo transcurrido / total
    m_timeLabel = lv_label_create(m_overlayControls);
    lv_label_set_text(m_timeLabel, "00:00 / 00:00");
    lv_obj_set_style_text_font(m_timeLabel, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_timeLabel, lv_color_make(180, 180, 200), 0);
    lv_obj_align(m_timeLabel, LV_ALIGN_TOP_RIGHT, -4, 0);

    // Slider de progreso
    m_seekSlider = lv_slider_create(m_overlayControls);
    lv_obj_set_size(m_seekSlider, LV_PCT(96), 10);
    lv_obj_align(m_seekSlider, LV_ALIGN_TOP_MID, 0, 26);
    lv_slider_set_range(m_seekSlider, 0, 100);
    lv_obj_set_style_bg_color(m_seekSlider, lv_color_make(50, 50, 70), LV_PART_MAIN);
    lv_obj_set_style_bg_color(m_seekSlider, DefaultTheme::getPrimaryAccent(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(m_seekSlider, lv_color_white(), LV_PART_KNOB);
    lv_obj_set_style_pad_all(m_seekSlider, 2, LV_PART_KNOB);
    lv_obj_add_event_cb(m_seekSlider, sliderSeekCb, LV_EVENT_VALUE_CHANGED, this);

    // Fila de botones de control inferior
    lv_obj_t* btnRow = lv_obj_create(m_overlayControls);
    lv_obj_set_size(btnRow, LV_PCT(100), 44);
    lv_obj_align(btnRow, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(btnRow, 0, 0);
    lv_obj_set_style_border_width(btnRow, 0, 0);
    lv_obj_set_flex_flow(btnRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btnRow, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(btnRow, 16, 0);
    DefaultTheme::disableScroll(btnRow);

    // Botón Volver / Lista
    lv_obj_t* backBtn = lv_button_create(btnRow);
    lv_obj_set_size(backBtn, 40, 40);
    DefaultTheme::applyButton(backBtn, 20);
    lv_obj_add_event_cb(backBtn, stopClickCb, LV_EVENT_CLICKED, this);
    lv_obj_t* backLbl = lv_label_create(backBtn);
    lv_label_set_text(backLbl, LV_SYMBOL_LEFT);
    lv_obj_center(backLbl);

    // Botón Play / Pausa
    m_playBtn = lv_button_create(btnRow);
    lv_obj_set_size(m_playBtn, 46, 46);
    DefaultTheme::applyButton(m_playBtn, 23);
    lv_obj_set_style_bg_color(m_playBtn, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_add_event_cb(m_playBtn, playPauseCb, LV_EVENT_CLICKED, this);
    m_playBtnLabel = lv_label_create(m_playBtn);
    lv_label_set_text(m_playBtnLabel, LV_SYMBOL_PAUSE);
    lv_obj_set_style_text_color(m_playBtnLabel, lv_color_black(), 0);
    lv_obj_center(m_playBtnLabel);

    updateNavHeaderBtn();
    return true;
}

void VideoPlayerView::onDestroy() {
    stopVideo();
    if (m_videoTimer) {
        lv_timer_delete(m_videoTimer);
        m_videoTimer = nullptr;
    }
    m_canvas = nullptr;
    m_playerContainer = nullptr;
    m_listContainer = nullptr;
    m_overlayControls = nullptr;
    m_playBtn = nullptr;
    m_playBtnLabel = nullptr;
    m_titleLabel = nullptr;
    m_timeLabel = nullptr;
    m_seekSlider = nullptr;

    BaseView::onDestroy();

    if (m_frameBuffer) {
        free(m_frameBuffer);
        m_frameBuffer = nullptr;
    }
}

void VideoPlayerView::onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) {
    if (m_listContainer && lv_obj_is_valid(m_listContainer)) {
        renderPlaylist(m_listContainer);
    }
}

void VideoPlayerView::scanVideoFilesSD() {
    m_playlist.clear();

    auto scanDir = [this](const std::string& folder) {
        auto entries = cbdos::storage::listDir(folder.c_str());
        for (const auto& entry : entries) {
            if (entry.isDirectory) continue;
            std::string nameLower = entry.name;
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

            bool isAvi = (nameLower.length() >= 4 && (nameLower.rfind(".avi") == nameLower.length() - 4 ||
                                                      nameLower.rfind(".mjpg") == nameLower.length() - 5));
            bool isMp4 = (nameLower.length() >= 4 && (nameLower.rfind(".mp4") == nameLower.length() - 4 ||
                                                      nameLower.rfind(".m4v") == nameLower.length() - 4));

            if (isAvi) {
                std::string fullPath = folder + "/" + entry.name;
                m_playlist.push_back({
                    entry.name,
                    fullPath,
                    0, 0, 30.0f, 0, false, false
                });
            } else if (isMp4) {
                std::string fullPath = folder + "/" + entry.name;
                m_playlist.push_back({
                    entry.name,
                    fullPath,
                    0, 0, 30.0f, 0, false, true
                });
            }
        }
    };

    scanDir("/sdcard/videos");
    scanDir("/sdcard");
}

void VideoPlayerView::renderPlaylist(lv_obj_t* parent) {
    if (!parent) return;
    lv_obj_clean(parent);

    // Cabecera de la lista
    lv_obj_t* headerCard = lv_obj_create(parent);
    lv_obj_set_size(headerCard, LV_PCT(100), LV_SIZE_CONTENT);
    DefaultTheme::applyRaisedCard(headerCard, 14);
    lv_obj_set_flex_flow(headerCard, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(headerCard, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(headerCard, 10, 0);

    lv_obj_t* titleLbl = lv_label_create(headerCard);
    lv_label_set_text(titleLbl, LV_SYMBOL_VIDEO " Videos en MicroSD");
    lv_obj_set_style_text_font(titleLbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(titleLbl, DefaultTheme::getTextColor(), 0);

    char countBuf[32];
    snprintf(countBuf, sizeof(countBuf), "%zu archivos", m_playlist.size());
    lv_obj_t* countLbl = lv_label_create(headerCard);
    lv_label_set_text(countLbl, countBuf);
    lv_obj_set_style_text_font(countLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(countLbl, DefaultTheme::getMutedTextColor(), 0);

    if (m_playlist.empty()) {
        lv_obj_t* emptyCard = lv_obj_create(parent);
        lv_obj_set_size(emptyCard, LV_PCT(100), 160);
        DefaultTheme::applyRaisedCard(emptyCard, 14);
        lv_obj_set_flex_flow(emptyCard, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(emptyCard, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(emptyCard, 16, 0);

        lv_obj_t* icon = lv_label_create(emptyCard);
        lv_label_set_text(icon, LV_SYMBOL_FILE);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(icon, DefaultTheme::getMutedTextColor(), 0);

        lv_obj_t* msg = lv_label_create(emptyCard);
        lv_label_set_text(msg, "No se encontraron videos (.mp4 / .avi) en /sdcard/videos");
        lv_obj_set_style_text_font(msg, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(msg, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
        return;
    }

    for (size_t i = 0; i < m_playlist.size(); ++i) {
        const auto& item = m_playlist[i];

        lv_obj_t* card = lv_obj_create(parent);
        lv_obj_set_size(card, LV_PCT(100), 72);
        DefaultTheme::applyRaisedCard(card, 12);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(card, 8, 0);
        lv_obj_set_style_pad_column(card, 12, 0);
        lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(card, itemClickCb, LV_EVENT_CLICKED, this);
        lv_obj_set_user_data(card, (void*)(uintptr_t)i);

        // Thumbnail / Icono de video
        lv_obj_t* iconBox = lv_obj_create(card);
        lv_obj_set_size(iconBox, 48, 48);
        lv_obj_set_style_bg_color(iconBox, item.isMp4 ? lv_color_make(30, 50, 80) : lv_color_make(40, 30, 60), 0);
        lv_obj_set_style_border_width(iconBox, 0, 0);
        lv_obj_set_style_radius(iconBox, 8, 0);
        DefaultTheme::disableScroll(iconBox);

        lv_obj_t* iconLbl = lv_label_create(iconBox);
        lv_label_set_text(iconLbl, LV_SYMBOL_PLAY);
        lv_obj_set_style_text_color(iconLbl, item.isMp4 ? lv_color_hex(0x38BDF8) : DefaultTheme::getPrimaryAccent(), 0);
        lv_obj_center(iconLbl);

        // Info (Nombre + Detalles)
        lv_obj_t* infoBox = lv_obj_create(card);
        lv_obj_set_size(infoBox, LV_PCT(76), LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(infoBox, 0, 0);
        lv_obj_set_style_border_width(infoBox, 0, 0);
        lv_obj_set_flex_flow(infoBox, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_all(infoBox, 0, 0);
        lv_obj_set_style_pad_row(infoBox, 4, 0);
        DefaultTheme::disableScroll(infoBox);

        lv_obj_t* nameLbl = lv_label_create(infoBox);
        lv_label_set_text(nameLbl, item.name.c_str());
        lv_obj_set_style_text_font(nameLbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(nameLbl, DefaultTheme::getTextColor(), 0);
        lv_label_set_long_mode(nameLbl, LV_LABEL_LONG_DOT);
        lv_obj_set_width(nameLbl, LV_PCT(100));

        char metaBuf[64];
        if (item.width > 0 && item.height > 0) {
            snprintf(metaBuf, sizeof(metaBuf), "%s %lux%lu  •  %.1f FPS %s",
                     item.isMp4 ? "[MP4]" : "[AVI]",
                     (unsigned long)item.width, (unsigned long)item.height, item.fps,
                     item.hasAudio ? " •  " LV_SYMBOL_AUDIO : "");
        } else {
            snprintf(metaBuf, sizeof(metaBuf), "%s Video", item.isMp4 ? "MP4 H.264" : "AVI MJPEG");
        }
        lv_obj_t* metaLbl = lv_label_create(infoBox);
        lv_label_set_text(metaLbl, metaBuf);
        lv_obj_set_style_text_font(metaLbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(metaLbl, DefaultTheme::getMutedTextColor(), 0);
    }
}

void VideoPlayerView::startVideo(int index) {
    if (index < 0 || index >= (int)m_playlist.size()) return;

    stopVideo();
    m_currentVideoIndex = index;
    const auto& item = m_playlist[index];
    m_isCurrentMp4 = item.isMp4;
    ESP_LOGI(TAG, "Abriendo archivo video: %s (MP4: %d)", item.path.c_str(), m_isCurrentMp4);

    float fps = 30.0f;

    if (m_isCurrentMp4) {
        if (!m_mp4Parser.open(item.path)) {
            ESP_LOGE(TAG, "Error abriendo MP4: %s", item.path.c_str());
            return;
        }
        const auto& info = m_mp4Parser.getInfo();
        if (info.fps > 0.0f) fps = info.fps;
        ESP_LOGI(TAG, "MP4 cargado: %lux%lu @ %.1f FPS, muestras: %lu",
                 (unsigned long)info.width, (unsigned long)info.height, info.fps, (unsigned long)info.totalVideoSamples);
    } else {
        if (!m_aviParser.open(item.path)) {
            ESP_LOGE(TAG, "Error abriendo AVI: %s", item.path.c_str());
            return;
        }
        const auto& info = m_aviParser.getInfo();
        if (info.fps > 0.0f) fps = info.fps;
        ESP_LOGI(TAG, "AVI cargado: %lux%lu @ %.1f FPS, frames: %lu",
                 (unsigned long)info.width, (unsigned long)info.height, info.fps, (unsigned long)info.totalFrames);
    }

    lv_label_set_text(m_titleLabel, item.name.c_str());
    lv_slider_set_value(m_seekSlider, 0, LV_ANIM_OFF);

    uint32_t intervalMs = (fps > 0.0f) ? (uint32_t)(1000.0f / fps) : 33;
    if (intervalMs < 10) intervalMs = 10;
    if (intervalMs > 100) intervalMs = 100;

    m_isPlaying = true;
    if (m_playBtnLabel && lv_obj_is_valid(m_playBtnLabel)) {
        lv_label_set_text(m_playBtnLabel, LV_SYMBOL_PAUSE);
    }

    if (!m_videoTimer) {
        m_videoTimer = lv_timer_create(videoTimerCb, intervalMs, this);
    } else {
        lv_timer_set_period(m_videoTimer, intervalMs);
        lv_timer_resume(m_videoTimer);
    }

    showPlayerScreen();
}

void VideoPlayerView::stopVideo() {
    if (m_videoTimer) {
        lv_timer_delete(m_videoTimer);
        m_videoTimer = nullptr;
    }
    m_mp4Parser.close();
    m_aviParser.close();
    m_isPlaying = false;
    if (m_playBtnLabel && lv_obj_is_valid(m_playBtnLabel)) {
        lv_label_set_text(m_playBtnLabel, LV_SYMBOL_PLAY);
    }
}

void VideoPlayerView::showPlayerScreen() {
    if (m_listContainer && lv_obj_is_valid(m_listContainer)) {
        lv_obj_add_flag(m_listContainer, LV_OBJ_FLAG_HIDDEN);
    }
    if (m_playerContainer && lv_obj_is_valid(m_playerContainer)) {
        lv_obj_remove_flag(m_playerContainer, LV_OBJ_FLAG_HIDDEN);
    }
    updateNavHeaderBtn();
}

void VideoPlayerView::showListScreen() {
    stopVideo();
    if (m_playerContainer && lv_obj_is_valid(m_playerContainer)) {
        lv_obj_add_flag(m_playerContainer, LV_OBJ_FLAG_HIDDEN);
    }
    if (m_listContainer && lv_obj_is_valid(m_listContainer)) {
        lv_obj_remove_flag(m_listContainer, LV_OBJ_FLAG_HIDDEN);
    }
    updateNavHeaderBtn();
}

void VideoPlayerView::updateNavHeaderBtn() {
    auto& hb = UIManager::getInstance().getHeaderBar();
    if (m_playerContainer && lv_obj_is_valid(m_playerContainer) && !lv_obj_has_flag(m_playerContainer, LV_OBJ_FLAG_HIDDEN)) {
        hb.showBackButton(true, [this]() {
            showListScreen();
        });
        hb.clearRightAction();
    } else {
        hb.showBackButton(true, []() {
            UIManager::getInstance().popView();
        });
        hb.clearRightAction();
    }
}

void VideoPlayerView::toggleControlsVisibility() {
    m_controlsVisible = !m_controlsVisible;
    if (m_overlayControls) {
        if (m_controlsVisible) {
            lv_obj_remove_flag(m_overlayControls, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(m_overlayControls, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void VideoPlayerView::videoTimerCb(lv_timer_t* timer) {
    auto* view = static_cast<VideoPlayerView*>(lv_timer_get_user_data(timer));
    if (!view || !view->m_isPlaying) return;

    if (view->m_isCurrentMp4) {
        if (view->m_mp4Parser.decodeNextVideoFrame(view->m_frameBuffer, view->m_canvasWidth, view->m_canvasHeight)) {
            lv_obj_invalidate(view->m_canvas);

            const auto& info = view->m_mp4Parser.getInfo();
            uint32_t curSample = view->m_mp4Parser.getCurrentVideoSample();
            if (info.totalVideoSamples > 0) {
                uint32_t pct = (curSample * 100) / info.totalVideoSamples;
                lv_slider_set_value(view->m_seekSlider, pct, LV_ANIM_OFF);

                uint32_t curSec = (info.fps > 0.0f) ? (uint32_t)(curSample / info.fps) : 0;
                uint32_t totalSec = info.durationSec;

                char timeBuf[32];
                snprintf(timeBuf, sizeof(timeBuf), "%02lu:%02lu / %02lu:%02lu",
                         (unsigned long)(curSec / 60), (unsigned long)(curSec % 60),
                         (unsigned long)(totalSec / 60), (unsigned long)(totalSec % 60));
                lv_label_set_text(view->m_timeLabel, timeBuf);
            }
        } else {
            const auto& info = view->m_mp4Parser.getInfo();
            if (view->m_mp4Parser.getCurrentVideoSample() >= info.totalVideoSamples) {
                view->stopVideo();
            }
        }
    } else {
        cbdos::media::AviChunk chunk;
        if (view->m_aviParser.readNextChunkHeader(chunk)) {
            if (chunk.isVideo) {
                const auto& info = view->m_aviParser.getInfo();
                uint32_t curFrame = view->m_aviParser.getCurrentFrameIndex();
                if (info.totalFrames > 0) {
                    uint32_t pct = (curFrame * 100) / info.totalFrames;
                    lv_slider_set_value(view->m_seekSlider, pct, LV_ANIM_OFF);

                    uint32_t curSec = (info.fps > 0.0f) ? (uint32_t)(curFrame / info.fps) : 0;
                    uint32_t totalSec = (info.fps > 0.0f) ? (uint32_t)(info.totalFrames / info.fps) : 0;

                    char timeBuf[32];
                    snprintf(timeBuf, sizeof(timeBuf), "%02lu:%02lu / %02lu:%02lu",
                             (unsigned long)(curSec / 60), (unsigned long)(curSec % 60),
                             (unsigned long)(totalSec / 60), (unsigned long)(totalSec % 60));
                    lv_label_set_text(view->m_timeLabel, timeBuf);
                }
                view->m_aviParser.skipChunk(chunk);
            } else if (chunk.isAudio) {
                view->m_aviParser.skipChunk(chunk);
            }
        } else {
            view->stopVideo();
        }
    }
}

void VideoPlayerView::itemClickCb(lv_event_t* e) {
    auto* view = static_cast<VideoPlayerView*>(lv_event_get_user_data(e));
    lv_obj_t* target = (lv_obj_t*)lv_event_get_current_target(e);
    if (!view || !target) return;

    int index = (int)(uintptr_t)lv_obj_get_user_data(target);
    ESP_LOGI(TAG, "Item clicked, index: %d", index);
    view->startVideo(index);
}

void VideoPlayerView::playPauseCb(lv_event_t* e) {
    auto* view = static_cast<VideoPlayerView*>(lv_event_get_user_data(e));
    if (!view) return;

    if (view->m_isPlaying) {
        view->m_isPlaying = false;
        if (view->m_videoTimer) lv_timer_pause(view->m_videoTimer);
        lv_label_set_text(view->m_playBtnLabel, LV_SYMBOL_PLAY);
    } else {
        view->m_isPlaying = true;
        if (view->m_videoTimer) lv_timer_resume(view->m_videoTimer);
        lv_label_set_text(view->m_playBtnLabel, LV_SYMBOL_PAUSE);
    }
}

void VideoPlayerView::stopClickCb(lv_event_t* e) {
    auto* view = static_cast<VideoPlayerView*>(lv_event_get_user_data(e));
    if (!view) return;
    view->showListScreen();
}

void VideoPlayerView::screenTouchCb(lv_event_t* e) {
    auto* view = static_cast<VideoPlayerView*>(lv_event_get_user_data(e));
    if (!view) return;
    view->toggleControlsVisibility();
}

void VideoPlayerView::sliderSeekCb(lv_event_t* e) {
    auto* view = static_cast<VideoPlayerView*>(lv_event_get_user_data(e));
    if (!view) return;

    int32_t val = lv_slider_get_value(view->m_seekSlider);
    if (view->m_isCurrentMp4) {
        const auto& info = view->m_mp4Parser.getInfo();
        if (info.totalVideoSamples > 0) {
            uint32_t targetSample = (val * info.totalVideoSamples) / 100;
            view->m_mp4Parser.seekToSample(targetSample);
        }
    } else {
        const auto& info = view->m_aviParser.getInfo();
        if (info.totalFrames > 0) {
            uint32_t targetFrame = (val * info.totalFrames) / 100;
            view->m_aviParser.seekToFrame(targetFrame);
        }
    }
}

} // namespace ui
} // namespace cbdos
