#include "SystemIcons.hpp"
#include "cbdos/display.hpp"
#include <cstring>
#include <unordered_map>

// Símbolos generados para los archivos .bin de 48x48 (ARGB8888)
extern const uint8_t recorder_bin[]   asm("_binary_app_recorder_bin_start");
extern const uint8_t radio_bin[]      asm("_binary_app_radio_bin_start");
extern const uint8_t browser_bin[]    asm("_binary_app_browser_bin_start");
extern const uint8_t terminal_bin[]   asm("_binary_app_terminal_bin_start");
extern const uint8_t cartridge_bin[]  asm("_binary_app_cartridge_bin_start");
extern const uint8_t lua_bin[]        asm("_binary_app_lua_bin_start");
extern const uint8_t editor_bin[]     asm("_binary_app_editor_bin_start");
extern const uint8_t utilities_bin[]  asm("_binary_app_utilities_bin_start");
extern const uint8_t gallery_bin[]    asm("_binary_app_gallery_bin_start");
extern const uint8_t files_bin[]      asm("_binary_app_files_bin_start");
extern const uint8_t music_bin[]      asm("_binary_app_music_bin_start");
extern const uint8_t flasher_bin[]    asm("_binary_app_flasher_bin_start");
extern const uint8_t config_bin[]     asm("_binary_app_config_bin_start");

namespace cbdos {
namespace ui {

void SystemIcons::init() {
}

const char* SystemIcons::getSvgData(const std::string& appId) {
    (void)appId;
    return nullptr;
}

static const uint8_t* getBinData(const std::string& appId) {
    if (appId == "recorder") return recorder_bin;
    if (appId == "radio") return radio_bin;
    if (appId == "browser") return browser_bin;
    if (appId == "terminal") return terminal_bin;
    if (appId == "cartridge") return cartridge_bin;
    if (appId == "lua") return lua_bin;
    if (appId == "editor") return editor_bin;
    if (appId == "utilities") return utilities_bin;
    if (appId == "gallery") return gallery_bin;
    if (appId == "files") return files_bin;
    if (appId == "music") return music_bin;
    if (appId == "flasher") return flasher_bin;
    if (appId == "config") return config_bin;
    return nullptr;
}

lv_obj_t* SystemIcons::createIcon(lv_obj_t* parent, const std::string& appId, int32_t size) {
    if (!parent) return nullptr;

    const uint8_t* binData = getBinData(appId);
    if (!binData) return nullptr;

    static std::unordered_map<std::string, lv_image_dsc_t> s_binDscMap;
    auto it = s_binDscMap.find(appId);
    if (it == s_binDscMap.end()) {
        lv_image_dsc_t dsc;
        lv_memzero(&dsc, sizeof(lv_image_dsc_t));
        dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
        dsc.header.cf = LV_COLOR_FORMAT_ARGB8888;
        dsc.header.w = 48;
        dsc.header.h = 48;
        dsc.data_size = 48 * 48 * 4;
        dsc.data = binData;
        s_binDscMap[appId] = dsc;
        it = s_binDscMap.find(appId);
    }

    lv_obj_t* bin_img = lv_image_create(parent);
    lv_image_set_src(bin_img, &(it->second));
    if (size > 0 && size != 48) {
        lv_image_set_scale(bin_img, (size * 256) / 48);
    }
    lv_obj_center(bin_img);
    lv_obj_remove_flag(bin_img, LV_OBJ_FLAG_CLICKABLE);
    return bin_img;
}

} // namespace ui
} // namespace cbdos
