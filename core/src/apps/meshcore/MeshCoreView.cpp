#include "MeshCoreView.hpp"
#include "UIManager.hpp"
#include "themes/DefaultTheme.h"
#include "cbdos/display.hpp"
#include "cbdos/network_interface.hpp"
#include <cstdio>

namespace cbdos {
namespace ui {

using namespace apps::meshcore;

MeshCoreView::MeshCoreView()
    : BaseView("MeshCore") {
}

bool MeshCoreView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;

    // Iniciar motor bajo demanda al abrir la vista
    MeshCoreEngine::getInstance().init();

    // Contenedor base de BaseView
    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(m_container, DefaultTheme::getBgColor(), 0);
    lv_obj_set_style_border_width(m_container, 0, 0);
    lv_obj_set_style_pad_all(m_container, 0, 0);
    lv_obj_set_scrollbar_mode(m_container, LV_SCROLLBAR_MODE_OFF);

    createTabViews(m_container);

    // Callbacks del motor a la UI (Totalmente desacoplados: solo encolan en memoria C++, cero llamadas a LVGL desde la radio)
    MeshCoreEngine::getInstance().setMessageCallback([this](const MeshMessage& msg) {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        if (m_incomingMsgQueue.size() >= 50) {
            m_incomingMsgQueue.erase(m_incomingMsgQueue.begin());
        }
        m_incomingMsgQueue.push_back(msg);
    });

    MeshCoreEngine::getInstance().setNodeCallback([this](const MeshNode& /*node*/) {
        m_nodesDirty = true;
    });

    MeshCoreEngine::getInstance().setStatusCallback([this]() {
        m_statusDirty = true;
    });

    refreshMessages();
    refreshNodes();
    refreshInterfacesState();

    return true;
}

void MeshCoreView::onDestroy() {
    MeshCoreEngine::getInstance().setMessageCallback(nullptr);
    MeshCoreEngine::getInstance().setNodeCallback(nullptr);
    MeshCoreEngine::getInstance().setStatusCallback(nullptr);

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_incomingMsgQueue.clear();
    }

    UIManager::closeKeyboard();
    BaseView::onDestroy();
}

void MeshCoreView::onUpdate() {
    // 1. Procesar mensajes recibidos de forma segura en el hilo de la UI
    std::vector<MeshMessage> msgsToProcess;
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        if (!m_incomingMsgQueue.empty()) {
            msgsToProcess.swap(m_incomingMsgQueue);
        }
    }

    for (const auto& msg : msgsToProcess) {
        onMessageReceived(msg);
    }

    // 2. Refresco diferido del radar de nodos solo si hubo cambios
    if (m_nodesDirty) {
        m_nodesDirty = false;
        refreshNodes();
    }

    // 3. Refresco diferido de estado de radios
    if (m_statusDirty) {
        m_statusDirty = false;
        refreshInterfacesState();
    }
}

void MeshCoreView::createTabViews(lv_obj_t* parent) {
    m_tabView = lv_tabview_create(parent);
    lv_tabview_set_tab_bar_position(m_tabView, LV_DIR_TOP);
    lv_tabview_set_tab_bar_size(m_tabView, 44);
    lv_obj_set_size(m_tabView, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(m_tabView, DefaultTheme::getBgColor(), 0);

    lv_obj_t* tab_bar = lv_tabview_get_tab_bar(m_tabView);
    lv_obj_set_style_bg_color(tab_bar, lv_color_hex(0x131722), 0);
    lv_obj_set_style_border_color(tab_bar, lv_color_hex(0x242B3D), 0);
    lv_obj_set_style_border_width(tab_bar, 1, 0);

    // Pestañas
    lv_obj_t* tabChat = lv_tabview_add_tab(m_tabView, LV_SYMBOL_EDIT " Chat");
    lv_obj_t* tabNodes = lv_tabview_add_tab(m_tabView, LV_SYMBOL_WIFI " Radar");
    lv_obj_t* tabIfaces = lv_tabview_add_tab(m_tabView, LV_SYMBOL_SETTINGS " Radios");

    buildChatTab(tabChat);
    buildNodesTab(tabNodes);
    buildInterfacesTab(tabIfaces);
}

void MeshCoreView::buildChatTab(lv_obj_t* parent) {
    lv_obj_set_style_pad_all(parent, 6, 0);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(parent, 6, 0);

    // Fila superior de Canales
    lv_obj_t* channelRow = lv_obj_create(parent);
    lv_obj_set_size(channelRow, LV_PCT(100), 38);
    lv_obj_set_style_bg_opa(channelRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(channelRow, 0, 0);
    lv_obj_set_style_pad_all(channelRow, 0, 0);
    lv_obj_set_flex_flow(channelRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(channelRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    m_ddChannels = lv_dropdown_create(channelRow);
    lv_obj_set_size(m_ddChannels, LV_PCT(70), 36);
    lv_obj_set_style_bg_color(m_ddChannels, lv_color_hex(0x161C28), 0);
    lv_obj_set_style_border_color(m_ddChannels, lv_color_hex(0x28334A), 0);
    lv_obj_set_style_text_color(m_ddChannels, lv_color_white(), 0);
    lv_obj_add_event_cb(m_ddChannels, channelDropdownChangedCb, LV_EVENT_VALUE_CHANGED, this);

    m_btnAddChannel = lv_button_create(channelRow);
    lv_obj_set_size(m_btnAddChannel, LV_PCT(26), 36);
    lv_obj_set_style_bg_color(m_btnAddChannel, lv_color_hex(0x1F293D), 0);
    lv_obj_set_style_border_color(m_btnAddChannel, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_border_width(m_btnAddChannel, 1, 0);
    lv_obj_add_event_cb(m_btnAddChannel, addChannelClickedCb, LV_EVENT_CLICKED, this);
    lv_obj_t* lblAdd = lv_label_create(m_btnAddChannel);
    lv_label_set_text(lblAdd, LV_SYMBOL_PLUS " Canal");
    lv_obj_set_style_text_color(lblAdd, lv_color_hex(0x00E5FF), 0);
    lv_obj_center(lblAdd);

    // Contenedor lista de mensajes con scroll
    m_msgList = lv_obj_create(parent);
    lv_obj_set_size(m_msgList, LV_PCT(100), LV_PCT(68));
    lv_obj_set_style_bg_color(m_msgList, lv_color_hex(0x0E121B), 0);
    lv_obj_set_style_border_color(m_msgList, lv_color_hex(0x1F2636), 0);
    lv_obj_set_style_border_width(m_msgList, 1, 0);
    lv_obj_set_style_radius(m_msgList, 8, 0);
    lv_obj_set_style_pad_all(m_msgList, 8, 0);
    lv_obj_set_flex_flow(m_msgList, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(m_msgList, 6, 0);

    // Barra de entrada inferior
    lv_obj_t* inputRow = lv_obj_create(parent);
    lv_obj_set_size(inputRow, LV_PCT(100), 42);
    lv_obj_set_style_bg_opa(inputRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(inputRow, 0, 0);
    lv_obj_set_style_pad_all(inputRow, 0, 0);
    lv_obj_set_flex_flow(inputRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(inputRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    m_taInput = lv_textarea_create(inputRow);
    lv_obj_set_size(m_taInput, LV_PCT(74), 40);
    lv_textarea_set_placeholder_text(m_taInput, "Mensaje Mesh...");
    lv_textarea_set_one_line(m_taInput, true);
    lv_obj_set_style_bg_color(m_taInput, lv_color_hex(0x161C28), 0);
    lv_obj_set_style_border_color(m_taInput, lv_color_hex(0x28334A), 0);
    lv_obj_set_style_text_color(m_taInput, lv_color_white(), 0);
    lv_obj_set_style_radius(m_taInput, 6, 0);
    lv_obj_add_event_cb(m_taInput, inputFocusedCb, LV_EVENT_FOCUSED, this);
    lv_obj_add_event_cb(m_taInput, inputDefocusedCb, LV_EVENT_DEFOCUSED, this);

    m_btnSend = lv_button_create(inputRow);
    lv_obj_set_size(m_btnSend, LV_PCT(23), 40);
    lv_obj_set_style_bg_color(m_btnSend, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_radius(m_btnSend, 6, 0);
    lv_obj_add_event_cb(m_btnSend, sendButtonClickedCb, LV_EVENT_CLICKED, this);

    lv_obj_t* lblSend = lv_label_create(m_btnSend);
    lv_label_set_text(lblSend, LV_SYMBOL_PLAY " Enviar");
    lv_obj_set_style_text_color(lblSend, lv_color_black(), 0);
    lv_obj_center(lblSend);

    refreshChannelsDropdown();
}

void MeshCoreView::buildNodesTab(lv_obj_t* parent) {
    lv_obj_set_style_pad_all(parent, 8, 0);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(parent, 6, 0);

    // Fila superior con conteo y botón emitir presencia
    lv_obj_t* topRow = lv_obj_create(parent);
    lv_obj_set_size(topRow, LV_PCT(100), 40);
    lv_obj_set_style_bg_opa(topRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(topRow, 0, 0);
    lv_obj_set_style_pad_all(topRow, 0, 0);
    lv_obj_set_flex_flow(topRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(topRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    m_lblNodeCount = lv_label_create(topRow);
    lv_label_set_text(m_lblNodeCount, "Nodos Descubiertos: 0");
    lv_obj_set_style_text_color(m_lblNodeCount, lv_color_hex(0x94A3B8), 0);

    lv_obj_t* btnBeacon = lv_button_create(topRow);
    lv_obj_set_size(btnBeacon, 120, 34);
    lv_obj_set_style_bg_color(btnBeacon, lv_color_hex(0x1F293D), 0);
    lv_obj_set_style_border_color(btnBeacon, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_border_width(btnBeacon, 1, 0);
    lv_obj_set_style_radius(btnBeacon, 6, 0);
    lv_obj_add_event_cb(btnBeacon, beaconButtonClickedCb, LV_EVENT_CLICKED, this);

    lv_obj_t* lblBcn = lv_label_create(btnBeacon);
    lv_label_set_text(lblBcn, LV_SYMBOL_REFRESH " Emitir Beacon");
    lv_obj_set_style_text_color(lblBcn, lv_color_hex(0x00E5FF), 0);
    lv_obj_center(lblBcn);

    // Lista de nodos
    m_nodesList = lv_obj_create(parent);
    lv_obj_set_size(m_nodesList, LV_PCT(100), LV_PCT(86));
    lv_obj_set_style_bg_color(m_nodesList, lv_color_hex(0x0E121B), 0);
    lv_obj_set_style_border_color(m_nodesList, lv_color_hex(0x1F2636), 0);
    lv_obj_set_style_border_width(m_nodesList, 1, 0);
    lv_obj_set_style_radius(m_nodesList, 8, 0);
    lv_obj_set_style_pad_all(m_nodesList, 8, 0);
    lv_obj_set_flex_flow(m_nodesList, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(m_nodesList, 6, 0);
}

void MeshCoreView::buildInterfacesTab(lv_obj_t* parent) {
    lv_obj_set_style_pad_all(parent, 10, 0);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(parent, 10, 0);

    m_lblIfaceStatus = lv_label_create(parent);
    lv_label_set_text(m_lblIfaceStatus, "Estado: Conectado a NetworkInterfaceManager");
    lv_obj_set_style_text_color(m_lblIfaceStatus, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_text_font(m_lblIfaceStatus, &lv_font_montserrat_14, 0);

    // Iterar dinámicamente por los 3 slots de hardware
    for (uint8_t slot = 0; slot < 3; ++slot) {
        auto* iface = cbdos::network::NetworkInterfaceManager::getInstance().getInterface(slot);

        lv_obj_t* card = lv_obj_create(parent);
        lv_obj_set_size(card, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(card, lv_color_hex(0x141824), 0);
        lv_obj_set_style_border_color(card, lv_color_hex(0x222B3D), 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_radius(card, 8, 0);
        lv_obj_set_style_pad_all(card, 10, 0);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(card, 8, 0);

        // Fila 1: Cabecera con Nombre y Switch ON/OFF
        lv_obj_t* rowHeader = lv_obj_create(card);
        lv_obj_set_size(rowHeader, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(rowHeader, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(rowHeader, 0, 0);
        lv_obj_set_style_pad_all(rowHeader, 0, 0);
        lv_obj_set_flex_flow(rowHeader, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(rowHeader, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t* lblTitle = lv_label_create(rowHeader);
        char titleStr[64];
        if (slot == 0) {
            snprintf(titleStr, sizeof(titleStr), LV_SYMBOL_WIFI " Slot 0: %s", iface ? iface->getName() : "Radio Integrada (C6/S3)");
        } else if (slot == 1) {
            snprintf(titleStr, sizeof(titleStr), LV_SYMBOL_DRIVE " Slot 1: %s", iface ? iface->getName() : "Mochila LoRa (SX1262)");
        } else {
            snprintf(titleStr, sizeof(titleStr), LV_SYMBOL_USB " Slot 2: %s", iface ? iface->getName() : "USB Módem Serial TNC");
        }
        lv_label_set_text(lblTitle, titleStr);
        lv_obj_set_style_text_color(lblTitle, lv_color_white(), 0);
        lv_obj_set_style_text_font(lblTitle, &lv_font_montserrat_14, 0);

        lv_obj_t* sw = lv_switch_create(rowHeader);
        bool isEnabled = iface && iface->isReady() && (iface->getMode() != cbdos::network::InterfaceMode::Off);
        if (isEnabled) {
            lv_obj_add_state(sw, LV_STATE_CHECKED);
        }
        lv_obj_set_user_data(sw, (void*)(uintptr_t)slot);
        lv_obj_add_event_cb(sw, interfaceSwitchChangedCb, LV_EVENT_VALUE_CHANGED, this);
        if (slot == 0) m_swIface1 = sw;
        else if (slot == 1) m_swIface2 = sw;
        else if (slot == 2) m_swIface3 = sw;

        if (iface) {
            // Fila 2: Selectores de Modo y Canal
            lv_obj_t* rowControls = lv_obj_create(card);
            lv_obj_set_size(rowControls, LV_PCT(100), LV_SIZE_CONTENT);
            lv_obj_set_style_bg_opa(rowControls, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(rowControls, 0, 0);
            lv_obj_set_style_pad_all(rowControls, 0, 0);
            lv_obj_set_flex_flow(rowControls, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(rowControls, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

            // Dropdown de Modo Operativo
            lv_obj_t* ddMode = lv_dropdown_create(rowControls);
            lv_obj_set_size(ddMode, LV_PCT(48), 36);
            lv_dropdown_set_options(ddMode, "ESP-NOW\nESP-NOW LR\nWi-Fi STA\nApagado");
            
            auto curMode = iface->getMode();
            if (curMode == cbdos::network::InterfaceMode::EspNow) lv_dropdown_set_selected(ddMode, 0);
            else if (curMode == cbdos::network::InterfaceMode::EspNowLR) lv_dropdown_set_selected(ddMode, 1);
            else if (curMode == cbdos::network::InterfaceMode::WifiStation) lv_dropdown_set_selected(ddMode, 2);
            else lv_dropdown_set_selected(ddMode, 3);

            // Dropdown de Canal
            lv_obj_t* ddChannel = lv_dropdown_create(rowControls);
            lv_obj_set_size(ddChannel, LV_PCT(48), 36);
            lv_dropdown_set_options(ddChannel, "CH 1\nCH 2\nCH 3\nCH 4\nCH 5\nCH 6\nCH 7\nCH 8\nCH 9\nCH 10\nCH 11\nCH 12\nCH 13");
            uint8_t ch = iface->getChannel();
            if (ch >= 1 && ch <= 13) {
                lv_dropdown_set_selected(ddChannel, ch - 1);
            }

            // Fila 3: Botón Aplicar Configuración (Sin cambios accidentales automáticos)
            struct SlotApplyContext {
                uint8_t slot;
                lv_obj_t* ddMode;
                lv_obj_t* ddChannel;
                MeshCoreView* view;
            };
            auto* applyCtx = new SlotApplyContext{slot, ddMode, ddChannel, this};

            lv_obj_t* btnApply = lv_button_create(card);
            lv_obj_set_size(btnApply, LV_PCT(100), 36);
            lv_obj_set_style_bg_color(btnApply, lv_color_hex(0x1F293D), 0);
            lv_obj_set_style_border_color(btnApply, lv_color_hex(0x00E5FF), 0);
            lv_obj_set_style_border_width(btnApply, 1, 0);
            lv_obj_set_style_radius(btnApply, 6, 0);
            lv_obj_add_event_cb(btnApply, interfaceApplyClickedCb, LV_EVENT_CLICKED, applyCtx);
            lv_obj_add_event_cb(btnApply, [](lv_event_t* ev) {
                auto* c = static_cast<SlotApplyContext*>(lv_event_get_user_data(ev));
                delete c;
            }, LV_EVENT_DELETE, applyCtx);

            lv_obj_t* lblApply = lv_label_create(btnApply);
            lv_label_set_text(lblApply, LV_SYMBOL_SAVE " Guardar");
            lv_obj_set_style_text_color(lblApply, lv_color_hex(0x00E5FF), 0);
            lv_obj_center(lblApply);

            // Fila 4: MAC Address
            uint8_t mac[6] = {0};
            if (iface->getMacAddress(mac)) {
                lv_obj_t* lblMac = lv_label_create(card);
                char macStr[48];
                snprintf(macStr, sizeof(macStr), "MAC: %02X:%02X:%02X:%02X:%02X:%02X",
                         mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
                lv_label_set_text(lblMac, macStr);
                lv_obj_set_style_text_color(lblMac, lv_color_hex(0x64748B), 0);
                lv_obj_set_style_text_font(lblMac, &lv_font_montserrat_12, 0);
            }
        }
    }
}

void MeshCoreView::refreshChannelsDropdown() {
    if (!m_ddChannels || !lv_obj_is_valid(m_ddChannels)) return;

    auto channels = MeshCoreEngine::getInstance().getChannels();
    std::string options;
    uint16_t activeId = MeshCoreEngine::getInstance().getActiveChannelId();
    int activeIndex = 0;

    for (size_t i = 0; i < channels.size(); ++i) {
        if (i > 0) options += "\n";
        if (channels[i].isPrivate) {
            options += LV_SYMBOL_SETTINGS " ";
        }
        options += channels[i].name;
        if (channels[i].id == activeId) {
            activeIndex = static_cast<int>(i);
        }
    }

    lv_dropdown_set_options(m_ddChannels, options.c_str());
    lv_dropdown_set_selected(m_ddChannels, activeIndex);
}

void MeshCoreView::refreshMessages() {
    if (!m_msgList || !lv_obj_is_valid(m_msgList)) return;

    lv_obj_clean(m_msgList);
    uint16_t activeCh = MeshCoreEngine::getInstance().getActiveChannelId();
    auto msgs = MeshCoreEngine::getInstance().getMessages(activeCh);
    for (const auto& msg : msgs) {
        addMessageBubble(msg);
    }
    lv_obj_scroll_to_y(m_msgList, LV_COORD_MAX, LV_ANIM_OFF);
}

void MeshCoreView::onMessageReceived(const apps::meshcore::MeshMessage& msg) {
    if (!m_msgList || !lv_obj_is_valid(m_msgList)) return;

    uint16_t activeCh = MeshCoreEngine::getInstance().getActiveChannelId();
    if (msg.channelId == activeCh) {
        addMessageBubble(msg);

        // Limitar a un máximo de 50 burbujas en pantalla para no saturar memoria en MCUs
        if (lv_obj_get_child_count(m_msgList) > 50) {
            lv_obj_t* oldest = lv_obj_get_child(m_msgList, 0);
            if (oldest) {
                lv_obj_delete(oldest);
            }
        }

        lv_obj_scroll_to_y(m_msgList, LV_COORD_MAX, LV_ANIM_OFF);
    }
}

void MeshCoreView::addMessageBubble(const MeshMessage& msg) {
    lv_obj_t* bubble = lv_obj_create(m_msgList);
    lv_obj_set_width(bubble, LV_PCT(85));
    lv_obj_set_height(bubble, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(bubble, 10, 0);
    lv_obj_set_style_pad_all(bubble, 8, 0);

    if (msg.isOutgoing) {
        lv_obj_set_style_bg_color(bubble, msg.isEncrypted ? lv_color_hex(0x2D1B4E) : lv_color_hex(0x005577), 0);
        lv_obj_set_style_border_color(bubble, msg.isEncrypted ? lv_color_hex(0x9D4EDD) : lv_color_hex(0x00E5FF), 0);
        lv_obj_set_style_border_width(bubble, 1, 0);
        lv_obj_set_align(bubble, LV_ALIGN_TOP_RIGHT);
    } else {
        lv_obj_set_style_bg_color(bubble, msg.isEncrypted ? lv_color_hex(0x231834) : lv_color_hex(0x1F2636), 0);
        lv_obj_set_style_border_color(bubble, msg.isEncrypted ? lv_color_hex(0x7B2CBF) : lv_color_hex(0x323E56), 0);
        lv_obj_set_style_border_width(bubble, 1, 0);
        lv_obj_set_align(bubble, LV_ALIGN_TOP_LEFT);
    }

    lv_obj_set_flex_flow(bubble, LV_FLEX_FLOW_COLUMN);

    lv_obj_t* lblSender = lv_label_create(bubble);
    char hdr[80];
    if (msg.isEncrypted) {
        snprintf(hdr, sizeof(hdr), "%s [0x%04X] " LV_SYMBOL_SETTINGS " %s", msg.senderName.c_str(), msg.senderId, msg.channelName.c_str());
    } else {
        snprintf(hdr, sizeof(hdr), "%s [0x%04X] %s", msg.senderName.c_str(), msg.senderId, msg.channelName.c_str());
    }
    lv_label_set_text(lblSender, hdr);
    lv_obj_set_style_text_color(lblSender, msg.isEncrypted ? lv_color_hex(0xE0AAFF) : (msg.isOutgoing ? lv_color_hex(0x00E5FF) : lv_color_hex(0xFFB703)), 0);
    lv_obj_set_style_text_font(lblSender, &lv_font_montserrat_12, 0);


    lv_obj_t* lblText = lv_label_create(bubble);
    lv_label_set_text(lblText, msg.text.c_str());
    lv_obj_set_style_text_color(lblText, lv_color_white(), 0);
    lv_label_set_long_mode(lblText, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lblText, LV_PCT(100));
}

void MeshCoreView::refreshNodes() {
    if (!m_nodesList || !lv_obj_is_valid(m_nodesList)) return;

    lv_obj_clean(m_nodesList);
    auto nodes = MeshCoreEngine::getInstance().getNodes();

    if (m_lblNodeCount && lv_obj_is_valid(m_lblNodeCount)) {
        char countStr[48];
        snprintf(countStr, sizeof(countStr), "Nodos Descubiertos: %zu", nodes.size());
        lv_label_set_text(m_lblNodeCount, countStr);
    }

    for (const auto& node : nodes) {
        lv_obj_t* card = lv_obj_create(m_nodesList);
        lv_obj_set_size(card, LV_PCT(100), 50);
        lv_obj_set_style_bg_color(card, lv_color_hex(0x141824), 0);
        lv_obj_set_style_border_color(card, lv_color_hex(0x222B3D), 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_radius(card, 6, 0);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(card, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t* lblInfo = lv_label_create(card);
        char info[128];
        snprintf(info, sizeof(info), "%s  ID: 0x%04X", node.name.c_str(), node.shortId);
        lv_label_set_text(lblInfo, info);
        lv_obj_set_style_text_color(lblInfo, lv_color_white(), 0);

        lv_obj_t* lblRssi = lv_label_create(card);
        char rssiStr[32];
        snprintf(rssiStr, sizeof(rssiStr), "%d dBm (%d saltos)", node.rssi, node.hops);
        lv_label_set_text(lblRssi, rssiStr);
        lv_obj_set_style_text_color(lblRssi, lv_color_hex(0x00E5FF), 0);
    }
}

void MeshCoreView::refreshInterfacesState() {
    if (!m_lblIfaceStatus || !lv_obj_is_valid(m_lblIfaceStatus)) return;

    size_t activeCount = MeshCoreEngine::getInstance().getActiveInterfaceCount();
    char statusStr[64];
    if (activeCount == 0) {
        snprintf(statusStr, sizeof(statusStr), "Estado: Modo Offline (0 Radios)");
        lv_obj_set_style_text_color(m_lblIfaceStatus, lv_color_hex(0xEF4444), 0);
    } else {
        snprintf(statusStr, sizeof(statusStr), "Estado: %zu Interfaz/ces Activa(s)", activeCount);
        lv_obj_set_style_text_color(m_lblIfaceStatus, lv_color_hex(0x00E5FF), 0);
    }
    lv_label_set_text(m_lblIfaceStatus, statusStr);
}

void MeshCoreView::sendButtonClickedCb(lv_event_t* e) {
    auto* view = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (!view || !view->m_taInput) return;

    const char* txt = lv_textarea_get_text(view->m_taInput);
    if (!txt || txt[0] == '\0') return;

    MeshCoreEngine::getInstance().sendMessage(0xFFFF, txt);
    lv_textarea_set_text(view->m_taInput, "");
}

void MeshCoreView::beaconButtonClickedCb(lv_event_t* e) {
    MeshCoreEngine::getInstance().sendBeacon();
    UIManager::showToast("Baliza MeshCore emitida");
}

void MeshCoreView::channelDropdownChangedCb(lv_event_t* e) {
    auto* dd = (lv_obj_t*)lv_event_get_target(e);
    auto* view = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (!dd || !view) return;

    uint16_t sel = lv_dropdown_get_selected(dd);
    auto channels = MeshCoreEngine::getInstance().getChannels();
    if (sel < channels.size()) {
        MeshCoreEngine::getInstance().setActiveChannelId(channels[sel].id);
        view->refreshMessages();
    }
}

void MeshCoreView::addChannelClickedCb(lv_event_t* e) {
    auto* view = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (!view) return;

    lv_obj_t* modal = lv_obj_create(lv_screen_active());
    lv_obj_set_size(modal, 280, 240);
    lv_obj_center(modal);
    lv_obj_set_style_bg_color(modal, lv_color_hex(0x131722), 0);
    lv_obj_set_style_border_color(modal, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_border_width(modal, 2, 0);
    lv_obj_set_style_radius(modal, 12, 0);
    lv_obj_set_flex_flow(modal, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(modal, 12, 0);
    lv_obj_set_style_pad_row(modal, 8, 0);

    lv_obj_t* lblTitle = lv_label_create(modal);
    lv_label_set_text(lblTitle, LV_SYMBOL_SETTINGS " Crear / Unirse a Canal");
    lv_obj_set_style_text_color(lblTitle, lv_color_hex(0x00E5FF), 0);

    lv_obj_t* taName = lv_textarea_create(modal);
    lv_obj_set_size(taName, LV_PCT(100), 38);
    lv_textarea_set_placeholder_text(taName, "Nombre (ej. #tactico)");
    lv_textarea_set_one_line(taName, true);
    lv_obj_add_event_cb(taName, inputFocusedCb, LV_EVENT_FOCUSED, view);
    lv_obj_add_event_cb(taName, inputDefocusedCb, LV_EVENT_DEFOCUSED, view);

    lv_obj_t* taKey = lv_textarea_create(modal);
    lv_obj_set_size(taKey, LV_PCT(100), 38);
    lv_textarea_set_placeholder_text(taKey, "Contraseña / PIN Cifrado");
    lv_textarea_set_password_mode(taKey, true);
    lv_textarea_set_one_line(taKey, true);
    lv_obj_add_event_cb(taKey, inputFocusedCb, LV_EVENT_FOCUSED, view);
    lv_obj_add_event_cb(taKey, inputDefocusedCb, LV_EVENT_DEFOCUSED, view);

    lv_obj_t* btnRow = lv_obj_create(modal);
    lv_obj_set_size(btnRow, LV_PCT(100), 42);
    lv_obj_set_style_bg_opa(btnRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btnRow, 0, 0);
    lv_obj_set_flex_flow(btnRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btnRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* btnCancel = lv_button_create(btnRow);
    lv_obj_set_size(btnCancel, LV_PCT(45), 36);
    lv_obj_set_style_bg_color(btnCancel, lv_color_hex(0x334155), 0);
    lv_obj_t* lblC = lv_label_create(btnCancel);
    lv_label_set_text(lblC, "Cancelar");
    lv_obj_center(lblC);
    lv_obj_add_event_cb(btnCancel, [](lv_event_t* ev) {
        lv_obj_t* m = (lv_obj_t*)lv_event_get_user_data(ev);
        UIManager::closeKeyboard();
        lv_obj_delete(m);
    }, LV_EVENT_CLICKED, modal);

    struct ModalContext {
        MeshCoreView* view;
        lv_obj_t* modal;
        lv_obj_t* taName;
        lv_obj_t* taKey;
    };
    auto* ctx = new ModalContext{view, modal, taName, taKey};

    lv_obj_t* btnOk = lv_button_create(btnRow);
    lv_obj_set_size(btnOk, LV_PCT(48), 36);
    lv_obj_set_style_bg_color(btnOk, lv_color_hex(0x00E5FF), 0);
    lv_obj_t* lblOk = lv_label_create(btnOk);
    lv_label_set_text(lblOk, "Guardar");
    lv_obj_set_style_text_color(lblOk, lv_color_black(), 0);
    lv_obj_center(lblOk);

    lv_obj_add_event_cb(btnOk, [](lv_event_t* ev) {
        auto* c = static_cast<ModalContext*>(lv_event_get_user_data(ev));
        if (c) {
            const char* name = lv_textarea_get_text(c->taName);
            const char* key = lv_textarea_get_text(c->taKey);
            if (name && name[0] != '\0') {
                std::string chName = (name[0] == '#') ? name : ("#" + std::string(name));
                bool isPriv = (key && key[0] != '\0');
                MeshCoreEngine::getInstance().addChannel(chName, isPriv, key ? key : "");
                c->view->refreshChannelsDropdown();
                UIManager::showToast("Canal añadido");
            }
            UIManager::closeKeyboard();
            lv_obj_delete(c->modal);
            delete c;
        }
    }, LV_EVENT_CLICKED, ctx);
}

void MeshCoreView::interfaceSwitchChangedCb(lv_event_t* e) {
    auto* sw = (lv_obj_t*)lv_event_get_target(e);
    auto* view = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (!sw || !view) return;

    uint8_t slot = static_cast<uint8_t>((uintptr_t)lv_obj_get_user_data(sw));
    bool enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
    MeshCoreEngine::getInstance().setInterfaceEnabled(static_cast<MeshInterfaceId>(slot), enabled);
    view->refreshInterfacesState();
}

void MeshCoreView::interfaceApplyClickedCb(lv_event_t* e) {
    struct SlotApplyContext {
        uint8_t slot;
        lv_obj_t* ddMode;
        lv_obj_t* ddChannel;
        MeshCoreView* view;
    };
    auto* ctx = static_cast<SlotApplyContext*>(lv_event_get_user_data(e));
    if (!ctx || !ctx->ddMode || !ctx->ddChannel || !ctx->view) return;

    auto* iface = cbdos::network::NetworkInterfaceManager::getInstance().getInterface(ctx->slot);
    if (!iface) return;

    uint16_t modeSel = lv_dropdown_get_selected(ctx->ddMode);
    uint16_t chanSel = lv_dropdown_get_selected(ctx->ddChannel);
    uint8_t channel = static_cast<uint8_t>(chanSel + 1);

    cbdos::network::InterfaceMode mode = cbdos::network::InterfaceMode::Off;
    const char* modeName = "Apagado";
    if (modeSel == 0) { mode = cbdos::network::InterfaceMode::EspNow; modeName = "ESP-NOW"; }
    else if (modeSel == 1) { mode = cbdos::network::InterfaceMode::EspNowLR; modeName = "ESP-NOW LR"; }
    else if (modeSel == 2) { mode = cbdos::network::InterfaceMode::WifiStation; modeName = "Wi-Fi STA"; }

    // 1. Aplicar canal
    iface->setChannel(channel);

    // 2. Aplicar modo operativo
    iface->setMode(mode);

    // 3. Sincronizar con el motor MeshCore
    bool enabled = (mode != cbdos::network::InterfaceMode::Off);
    MeshCoreEngine::getInstance().setInterfaceEnabled(static_cast<MeshInterfaceId>(ctx->slot), enabled);

    auto cfg = MeshCoreEngine::getInstance().getInterfaceConfig(static_cast<MeshInterfaceId>(ctx->slot));
    cfg.channel = channel;
    cfg.enabled = enabled;
    MeshCoreEngine::getInstance().setInterfaceConfig(cfg);

    // 4. Actualizar estado visual
    ctx->view->refreshInterfacesState();

    char toastBuf[64];
    snprintf(toastBuf, sizeof(toastBuf), "Slot %u: %s (CH %u)", ctx->slot, modeName, channel);
    UIManager::showToast(toastBuf);
}

void MeshCoreView::inputFocusedCb(lv_event_t* e) {
    auto* ta = (lv_obj_t*)lv_event_get_target(e);
    UIManager::attachKeyboard(ta);
}

void MeshCoreView::inputDefocusedCb(lv_event_t* e) {
    UIManager::closeKeyboard();
}

void MeshCoreView::onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) {
    if (m_container && lv_obj_is_valid(m_container)) {
        lv_obj_set_style_bg_color(m_container, DefaultTheme::getBgColor(), 0);
    }
}

} // namespace ui
} // namespace cbdos
