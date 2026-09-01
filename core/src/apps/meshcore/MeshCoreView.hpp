#pragma once

#include "views/BaseView.hpp"
#include "MeshCoreEngine.hpp"
#include <lvgl.h>
#include <vector>
#include <string>

namespace cbdos {
namespace ui {

class MeshCoreView : public BaseView {
public:
    MeshCoreView();
    ~MeshCoreView() override = default;

    bool onCreate(lv_obj_t* parent) override;
    void onDestroy() override;
    void onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) override;

private:
    void createTabViews(lv_obj_t* parent);
    void buildChatTab(lv_obj_t* parent);
    void buildNodesTab(lv_obj_t* parent);
    void buildInterfacesTab(lv_obj_t* parent);

    void refreshMessages();
    void refreshNodes();
    void refreshInterfacesState();
    void refreshChannelsDropdown();
    void addMessageBubble(const apps::meshcore::MeshMessage& msg);

    static void sendButtonClickedCb(lv_event_t* e);
    static void beaconButtonClickedCb(lv_event_t* e);
    static void channelDropdownChangedCb(lv_event_t* e);
    static void addChannelClickedCb(lv_event_t* e);
    static void interfaceSwitchChangedCb(lv_event_t* e);
    static void interfaceModeDropdownCb(lv_event_t* e);
    static void interfaceChannelDropdownCb(lv_event_t* e);
    static void inputFocusedCb(lv_event_t* e);
    static void inputDefocusedCb(lv_event_t* e);

    lv_obj_t* m_tabView = nullptr;
    
    // Elementos Tab Chat
    lv_obj_t* m_ddChannels = nullptr;
    lv_obj_t* m_btnAddChannel = nullptr;
    lv_obj_t* m_msgList = nullptr;
    lv_obj_t* m_taInput = nullptr;
    lv_obj_t* m_btnSend = nullptr;

    // Elementos Tab Nodos
    lv_obj_t* m_nodesList = nullptr;
    lv_obj_t* m_lblNodeCount = nullptr;

    // Elementos Tab Interfaces
    lv_obj_t* m_swIface1 = nullptr;
    lv_obj_t* m_swIface2 = nullptr;
    lv_obj_t* m_swIface3 = nullptr;
    lv_obj_t* m_lblIfaceStatus = nullptr;
};

} // namespace ui
} // namespace cbdos

