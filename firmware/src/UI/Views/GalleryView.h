#pragma once
#include <lvgl.h>
#include <string>
#include <vector>
#include "../Components/HeaderBar.h"

struct GalleryItem {
    std::string id;
    std::string name;
    std::string path;
};

class GalleryView {
public:
    static lv_obj_t* create(const std::string& imagePath, const std::string& imageName);

private:
    static HeaderBar* headerBar;
    static std::string currentImagePath;
    static std::string currentImageName;
};
