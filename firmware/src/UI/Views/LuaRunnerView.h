#pragma once

#include <lvgl.h>
#include <string>

class LuaRunnerView {
public:
    static lv_obj_t* create(const std::string& initialScript = "");
    static void setInitialScript(const std::string& path);

private:
    static void timer_cb(lv_timer_t* timer);
    static void screen_delete_cb(lv_event_t* e);
    static void btn_run_cb(lv_event_t* e);
    static void btn_stop_cb(lv_event_t* e);
    static void btn_edit_cb(lv_event_t* e);
    static void btn_sd_cb(lv_event_t* e);
    static void btn_clear_cb(lv_event_t* e);
    static void file_select_cb(lv_event_t* e);
    static void modal_close_cb(lv_event_t* e);

    static void showFilePickerModal();
    static void appendLogLine(const std::string& line);
    static void updateStatusBadge();

    static lv_obj_t* logContainer;
    static lv_obj_t* statusBadge;
    static lv_obj_t* scriptLabel;
    static lv_timer_t* refreshTimer;
    static std::string activeScript;
};
