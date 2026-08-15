#pragma once

#include <lvgl.h>
#include <vector>
#include <cstdint>

class StopwatchApp {
public:
    static void build(lv_obj_t* parent);
    static void cleanup();

private:
    static void updateSwTimeLabel();
    static void updateTimerDisplayLabel();
    static void timerCallback(lv_timer_t* t);

    static void swToggleCb(lv_event_t* e);
    static void swResetCb(lv_event_t* e);
    static void swLapCb(lv_event_t* e);
    static void timerPresetCb(lv_event_t* e);
    static void timerStopCb(lv_event_t* e);
};
