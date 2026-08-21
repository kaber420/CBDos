#pragma once

#include "BaseView.hpp"
#include "cbdos/cartridge.hpp"
#include "lvgl.h"
#include <vector>
#include <string>

namespace cbdos {
namespace ui {

class CartridgeView : public BaseView {
public:
    CartridgeView();
    ~CartridgeView() override = default;

    bool onCreate(lv_obj_t* parent) override;
    void onDestroy() override;
    void onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) override;

    void refreshSlots();
    void closeCurrentModal();

    static CartridgeView* getInstance() { return s_instance; }

private:
    void createSlotCard(lv_obj_t* parent, esp_partition_subtype_t subtype, 
                        const char* slotTitle, const char* capacityStr, 
                        uint32_t bgHex, uint32_t borderHex);
    void showSDPickerDialog(esp_partition_subtype_t targetSlot);
    void startFlashing(const std::string& binPath, esp_partition_subtype_t targetSlot);

    static CartridgeView* s_instance;

    lv_obj_t* m_slotsContainer = nullptr;
    lv_obj_t* m_modalBg = nullptr;

    std::vector<std::string> m_binFiles;
    esp_partition_subtype_t m_selectedSlot = ESP_PARTITION_SUBTYPE_APP_OTA_1;

    static void bootSlotCb(lv_event_t* e);
    static void openInstallModalCb(lv_event_t* e);
    static void fileSelectedCb(lv_event_t* e);
    static void closeModalCb(lv_event_t* e);
};

} // namespace ui
} // namespace cbdos
