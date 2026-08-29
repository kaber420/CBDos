#pragma once

#include "BaseView.hpp"
#include <vector>
#include <string>

namespace cbdos {
namespace ui {

struct AudioRecordItem {
    std::string name;
    std::string path;
    std::string sizeStr;
};

class AudioRecorderView : public BaseView {
public:
    AudioRecorderView();
    virtual ~AudioRecorderView() = default;

    bool onCreate(lv_obj_t* parent) override;
    void onDestroy() override;
    void onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) override;

private:
    void scanRecordings();
    void renderHistoryList();
    void toggleRecord();
    void playRecording(const std::string& path);
    void deleteRecording(const std::string& path);

    static void timerCb(lv_timer_t* timer);
    static void recordBtnCb(lv_event_t* e);
    static void playBtnCb(lv_event_t* e);
    static void deleteBtnCb(lv_event_t* e);

    // Controles UI principales
    lv_obj_t* m_topCard = nullptr;
    lv_obj_t* m_statusLabel = nullptr;
    lv_obj_t* m_timerLabel = nullptr;
    lv_obj_t* m_vuBar = nullptr;
    lv_obj_t* m_recordBtn = nullptr;
    lv_obj_t* m_recordBtnLabel = nullptr;
    lv_obj_t* m_listContainer = nullptr;

    lv_timer_t* m_updateTimer = nullptr;
    std::vector<AudioRecordItem> m_recordings;
    std::string m_currentPlayingPath;
    bool m_isPlaying = false;
};

} // namespace ui
} // namespace cbdos
