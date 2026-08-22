#ifndef TIME_CONFIG_VIEW_HPP
#define TIME_CONFIG_VIEW_HPP

#include "BaseView.hpp"
#include <lvgl.h>
#include <string>
#include <vector>

namespace cbdos {
namespace ui {

class TimeConfigView : public BaseView {
public:
    struct TzPreset {
        const char* name;
        int32_t offsetSec;
    };

    TimeConfigView();
    virtual ~TimeConfigView() = default;

    bool onCreate(lv_obj_t* parent) override;
    void onDestroy() override;

private:
    static const std::vector<TzPreset>& getTzPresets();

    static void save_btn_cb(lv_event_t* e);
    static void sync_btn_cb(lv_event_t* e);
    static void timer_cb(lv_timer_t* timer);

    lv_obj_t* m_timeLabel;
    lv_obj_t* m_dateLabel;
    lv_obj_t* m_statusLabel;
    lv_obj_t* m_tzDropdown;
    lv_obj_t* m_dstSwitch;
    lv_obj_t* m_ntpDropdown;
    lv_timer_t* m_clockTimer;

    void updateClockDisplay();
    void saveAndApply();
};

} // namespace ui
} // namespace cbdos

#endif // TIME_CONFIG_VIEW_HPP
