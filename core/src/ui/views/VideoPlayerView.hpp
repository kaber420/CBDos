#pragma once

#include "BaseView.hpp"
#include "cbdos/media/AviParser.hpp"
#include "cbdos/media/Mp4Parser.hpp"
#include <vector>
#include <string>

namespace cbdos {
namespace ui {

struct VideoItem {
    std::string name;
    std::string path;
    uint32_t width{0};
    uint32_t height{0};
    float fps{0.0f};
    uint32_t totalFrames{0};
    bool hasAudio{false};
    bool isMp4{false};
};

class VideoPlayerView : public BaseView {
public:
    VideoPlayerView();
    virtual ~VideoPlayerView();

    bool onCreate(lv_obj_t* parent) override;
    void onDestroy() override;
    void onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) override;

private:
    void scanVideoFilesSD();
    void renderPlaylist(lv_obj_t* parent);
    void startVideo(int index);
    void stopVideo();
    void showPlayerScreen();
    void showListScreen();
    void updateNavHeaderBtn();
    void toggleControlsVisibility();

    static void videoTimerCb(lv_timer_t* timer);
    static void itemClickCb(lv_event_t* e);
    static void playPauseCb(lv_event_t* e);
    static void stopClickCb(lv_event_t* e);
    static void screenTouchCb(lv_event_t* e);
    static void sliderSeekCb(lv_event_t* e);

    lv_obj_t* m_listContainer{nullptr};
    lv_obj_t* m_playerContainer{nullptr};
    lv_obj_t* m_canvas{nullptr};
    lv_obj_t* m_overlayControls{nullptr};
    lv_obj_t* m_titleLabel{nullptr};
    lv_obj_t* m_timeLabel{nullptr};
    lv_obj_t* m_playBtn{nullptr};
    lv_obj_t* m_playBtnLabel{nullptr};
    lv_obj_t* m_seekSlider{nullptr};

    lv_timer_t* m_videoTimer{nullptr};
    lv_timer_t* m_autoHideTimer{nullptr};

    std::vector<VideoItem> m_playlist;
    int m_currentVideoIndex{-1};
    bool m_isPlaying{false};
    bool m_controlsVisible{true};
    bool m_isCurrentMp4{false};

    cbdos::media::AviParser m_aviParser;
    cbdos::media::Mp4Parser m_mp4Parser;
    uint8_t* m_frameBuffer{nullptr};
    size_t m_frameBufferSize{0};
    uint32_t m_canvasWidth{480};
    uint32_t m_canvasHeight{320};
};

} // namespace ui
} // namespace cbdos
