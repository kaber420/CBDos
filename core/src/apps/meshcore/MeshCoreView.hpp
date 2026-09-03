#pragma once

#include "views/BaseView.hpp"
#include "MeshCoreEngine.hpp"
#include <lvgl.h>
#include <vector>
#include <string>
#include <mutex>

namespace cbdos {
namespace ui {

class MeshCoreView : public BaseView {
public:
    MeshCoreView();
    ~MeshCoreView() override = default;

    bool onCreate(lv_obj_t* parent) override;
    void onDestroy() override;
    void onUpdate() override;
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
    void onMessageReceived(const apps::meshcore::MeshMessage& msg);

    static void sendButtonClickedCb(lv_event_t* e);
    static void beaconButtonClickedCb(lv_event_t* e);
    static void channelDropdownChangedCb(lv_event_t* e);
    static void addChannelClickedCb(lv_event_t* e);
    static void interfaceSwitchChangedCb(lv_event_t* e);
    static void interfaceApplyClickedCb(lv_event_t* e);
    static void inputFocusedCb(lv_event_t* e);
    static void inputDefocusedCb(lv_event_t* e);

    // Buzón Thread-Safe (Productor = Tarea Wi-Fi, Consumidor = Hilo UI onUpdate)
    std::mutex m_queueMutex;
    std::vector<apps::meshcore::MeshMessage> m_incomingMsgQueue;
    bool m_nodesDirty = false;
    bool m_statusDirty = false;

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
    lv_obj_t* m_lblMac[3] = {nullptr, nullptr, nullptr};
    uint32_t m_lastIfaceCheckMs = 0;
};

} // namespace ui
} // namespace cbdos

