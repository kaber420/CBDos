#pragma once

#include <lvgl.h>
#include <cstdint>

enum class PomodoroMode {
    CLASSIC_25_5,
    DEEP_WORK_50_10,
    SPRINT_15_3,
    CUSTOM
};

enum class PomodoroPhase {
    WORK,
    SHORT_BREAK,
    LONG_BREAK
};

enum class PomodoroSound {
    ZEN = 1,
    CHIME = 2,
    URGENT = 3,
    MUTE = 0
};

class PomodoroApp {
public:
    static void build(lv_obj_t* parent);
    static void cleanup();

private:
    static void updateUI();
    static void timerCallback(lv_timer_t* t);
    static void toggleCb(lv_event_t* e);
    static void nextPhaseCb(lv_event_t* e);
    static void resetCb(lv_event_t* e);
    static void presetCb(lv_event_t* e);
    static void soundSelectCb(lv_event_t* e);
    static void customAdjustCb(lv_event_t* e);
    static void triggerAlert();
    static void applyPreset(PomodoroMode mode);
};
