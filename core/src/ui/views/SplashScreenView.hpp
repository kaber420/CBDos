#pragma once
#include "BaseView.hpp"
#include <lvgl.h>

namespace cbdos {
namespace ui {

class SplashScreenView : public BaseView {
public:
    SplashScreenView();
    ~SplashScreenView() override = default;

    bool onCreate(lv_obj_t* parent) override;
    void onDestroy() override;

private:
    static void splash_timer_cb(lv_timer_t* timer);
    lv_timer_t* m_timer;
};

} // namespace ui
} // namespace cbdos
