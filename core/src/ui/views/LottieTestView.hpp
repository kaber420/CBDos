#pragma once
#include "BaseView.hpp"
#include <lvgl.h>
#include <string>
#include <vector>

namespace cbdos {
namespace ui {

class LottieTestView : public BaseView {
public:
    LottieTestView();
    ~LottieTestView() override;

    bool onCreate(lv_obj_t* parent) override;
    void onDestroy() override;
    void onUpdate() override;
    void onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) override;

    void loadCurrentLottie();
    void nextLottie();
    void prevLottie();
    void rescanFiles();
    void toggleFullscreen();

private:
    void scanLottieFiles();

    lv_obj_t* m_lottieObj;
    lv_obj_t* m_fpsLabel;
    lv_obj_t* m_infoLabel;
    lv_obj_t* m_fileLabel;
    lv_obj_t* m_animCard;
    lv_obj_t* m_navRow;
    lv_obj_t* m_statsCard;
    lv_obj_t* m_btnBack;
    lv_draw_buf_t* m_drawBuf;

    std::vector<std::string> m_fileList;
    size_t m_currentFileIndex;
    std::string m_currentJsonData;

    bool m_isFullscreen;
    uint32_t m_lastTime;
    uint32_t m_frameCount;
};

} // namespace ui
} // namespace cbdos
