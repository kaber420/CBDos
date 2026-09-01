#pragma once

#include "views/BaseView.hpp"
#include "../../security/kerberos/KerberosManager.hpp"
#include <lvgl.h>

namespace cbdos {
namespace ui {

class KerberosView : public BaseView {
public:
    KerberosView();
    ~KerberosView() override = default;

    bool onCreate(lv_obj_t* parent) override;
    void onDestroy() override;
    void onShow() override;
    void onHide() override;
    void onUpdate() override;
    void onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) override;

private:
    void buildMainLayout(lv_obj_t* parent);
    void buildStatusCard(lv_obj_t* parent);
    void buildPromptCard(lv_obj_t* parent);
    void updateUIState();

    static void approveButtonClickedCb(lv_event_t* e);
    static void denyButtonClickedCb(lv_event_t* e);

    // Contenedores visuales
    lv_obj_t* m_statusCard = nullptr;
    lv_obj_t* m_lblUsbBadge = nullptr;
    lv_obj_t* m_lblUsbStatus = nullptr;
    lv_obj_t* m_lblDetails = nullptr;
    lv_obj_t* m_lblCounter = nullptr;

    // Tarjeta / Modal de Solicitud de Acceso WebAuthn
    lv_obj_t* m_promptCard = nullptr;
    lv_obj_t* m_lblPromptTitle = nullptr;
    lv_obj_t* m_lblRpId = nullptr;
    lv_obj_t* m_lblOpType = nullptr;
    lv_obj_t* m_btnApprove = nullptr;
    lv_obj_t* m_btnDeny = nullptr;

    bool m_lastPendingState = false;
    uint32_t m_lastPktCount = 0;
};

} // namespace ui
} // namespace cbdos
