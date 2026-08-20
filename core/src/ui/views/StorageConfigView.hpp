#pragma once

#include "BaseView.hpp"
#include "cbdos/storage.hpp"

namespace cbdos {
namespace ui {

class StorageConfigView : public BaseView {
public:
    StorageConfigView();
    virtual ~StorageConfigView() = default;

    bool onCreate(lv_obj_t* parent) override;
    void onUpdate() override;

private:
    void renderStorageUI();
    static void mount_btn_cb(lv_event_t* e);
    static void unmount_btn_cb(lv_event_t* e);

    lv_obj_t* m_sdCardStatusLabel = nullptr;
    lv_obj_t* m_sdCardBar = nullptr;
    lv_obj_t* m_sdCardCapacityLabel = nullptr;
    lv_obj_t* m_flashBar = nullptr;
    lv_obj_t* m_flashCapacityLabel = nullptr;
};

} // namespace ui
} // namespace cbdos
