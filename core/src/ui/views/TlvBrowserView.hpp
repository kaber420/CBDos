#pragma once

#include "BaseView.hpp"
#include <string>
#include <vector>
#include "../../tlv/mesh_header.hpp"
#include "../../tlv/tlv_parser.hpp"

namespace cbdos {
namespace ui {

class TlvBrowserView : public BaseView {
public:
    TlvBrowserView();
    virtual ~TlvBrowserView();

    bool onCreate(lv_obj_t* parent) override;
    void onDestroy() override;
    void onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) override;

    // Renderiza directamente un buffer TLV binario plano
    void render(const uint8_t* data, size_t length);

    // Procesa un paquete completo de la red mesh (MeshHeader + TLV)
    void processNetworkPacket(const uint8_t* packet, size_t length);

    // Navega a una URL especificada codificando la trama de petición
    void navigateToUrl(const char* url);

    // Renderiza la página de demostración offline
    void renderDemo();

    // Carga un archivo binario TLV desde almacenamiento local (MicroSD / Flash)
    bool loadLocalFile(const char* path);

    static TlvBrowserView* getInstance() { return s_instance; }

private:
    static TlvBrowserView* s_instance;

    lv_obj_t* m_urlContainer = nullptr;
    lv_obj_t* m_urlInput = nullptr;
    lv_obj_t* m_bookmarkBar = nullptr;
    lv_obj_t* m_contentArea = nullptr;

    static void onUrlSubmit(lv_event_t* e);
    static void onBookmarkClick(lv_event_t* e);
    static void onUplinkFrameGenerated(const uint8_t* frame, size_t len);
};

} // namespace ui
} // namespace cbdos
