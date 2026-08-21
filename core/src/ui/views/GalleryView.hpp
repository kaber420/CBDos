#pragma once

#include "BaseView.hpp"
#include <string>
#include <lvgl.h>

namespace cbdos {
namespace ui {

class GalleryView : public BaseView {
public:
    GalleryView(const std::string& imagePath, const std::string& imageName);
    virtual ~GalleryView() = default;

    bool onCreate(lv_obj_t* parent) override;
    void onDestroy() override;
    void onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) override;

    const std::string& getImagePath() const { return m_imagePath; }
    const std::string& getImageName() const { return m_imageName; }

private:
    std::string m_imagePath;
    std::string m_imageName;
    lv_obj_t* m_imgObj = nullptr;
};

} // namespace ui
} // namespace cbdos
