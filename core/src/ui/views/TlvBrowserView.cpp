#include "TlvBrowserView.hpp"
#include "../UIManager.hpp"
#include "../components/HeaderBar.hpp"
#include "../themes/DefaultTheme.h"
#include "cbdos/display.hpp"
#include "cbdos/storage.hpp"
#include <cstring>
#include <cstdio>
#include <vector>

namespace cbdos {
namespace ui {

TlvBrowserView* TlvBrowserView::s_instance = nullptr;

namespace {

struct TlvBuilder {
    uint8_t buf[1024];
    size_t len = 0;

    void push8(uint8_t v) { 
        if (len < sizeof(buf)) buf[len++] = v; 
    }
    
    void push16(uint16_t v) { 
        push8(v >> 8); 
        push8(v & 0xFF); 
    }
    
    void push(const uint8_t* p, size_t n) { 
        while (n-- && len < sizeof(buf)) buf[len++] = *p++; 
    }

    void magic() { 
        push8(0x50); 
        push8(0x48); 
    }

    void node(uint8_t type, const uint8_t* value, size_t vlen) {
        if (len + 3 + vlen > sizeof(buf)) return;
        push8(type);
        push16((uint16_t)vlen);
        if (value && vlen > 0) {
            push(value, vlen);
        }
    }

    void page() { 
        node(TYPE_ABS_PAGE, nullptr, 0); 
    }

    void panel(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
        uint8_t v[8];
        v[0]=x>>8; v[1]=x&0xFF; v[2]=y>>8; v[3]=y&0xFF;
        v[4]=w>>8; v[5]=w&0xFF; v[6]=h>>8; v[7]=h&0xFF;
        node(TYPE_ABS_PANEL, v, 8);
    }

    void text(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t style, const char* s) {
        if (!s) return;
        uint8_t v[9];
        v[0]=x>>8; v[1]=x&0xFF; v[2]=y>>8; v[3]=y&0xFF;
        v[4]=w>>8; v[5]=w&0xFF; v[6]=h>>8; v[7]=h&0xFF; v[8]=style;
        size_t slen = strlen(s);
        uint8_t combined[256];
        if (9 + slen > sizeof(combined)) slen = sizeof(combined) - 9;
        memcpy(combined, v, 9);
        memcpy(combined + 9, s, slen);
        node(TYPE_ABS_TEXT, combined, 9 + slen);
    }

    void link(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t id, const char* s) {
        if (!s) return;
        uint8_t v[9];
        v[0]=x>>8; v[1]=x&0xFF; v[2]=y>>8; v[3]=y&0xFF;
        v[4]=w>>8; v[5]=w&0xFF; v[6]=h>>8; v[7]=h&0xFF; v[8]=id;
        size_t slen = strlen(s);
        uint8_t combined[256];
        if (9 + slen > sizeof(combined)) slen = sizeof(combined) - 9;
        memcpy(combined, v, 9);
        memcpy(combined + 9, s, slen);
        node(TYPE_ABS_LINK, combined, 9 + slen);
    }

    void checkbox(uint8_t id, uint8_t state, const char* text) {
        if (!text) return;
        size_t slen = strlen(text);
        uint8_t combined[128];
        if (2 + slen > sizeof(combined)) slen = sizeof(combined) - 2;
        combined[0] = id;
        combined[1] = state;
        memcpy(combined + 2, text, slen);
        node(TYPE_ABS_CHECKBOX, combined, 2 + slen);
    }

    void end() { push8(TYPE_END); }
};

void buildDemoPage(TlvBuilder& b) {
    b.magic();
    b.page();
    b.panel(10, 10, 290, 75);
    b.text(20, 20, 270, 25, 1, "Navegador TLV / CBML");
    b.text(20, 48, 270, 20, 0, "Renderizador binario ultraligero");

    b.text(15, 95, 270, 20, 1, "Servicios Alternet / Mesh:");
    b.link(15, 125, 290, 38, 1, "📰 Noticias Locales");
    b.link(15, 170, 290, 38, 2, "🌤️ Estacion Meteorologica");
    b.link(15, 215, 290, 38, 3, "🖼️ Galeria Distribuida");
    b.end();
}

} // namespace

TlvBrowserView::TlvBrowserView()
    : BaseView("TlvBrowser") {
    s_instance = this;
}

TlvBrowserView::~TlvBrowserView() {
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

void TlvBrowserView::onDestroy() {
    if (s_instance == this) {
        s_instance = nullptr;
    }
    UIManager::getInstance().getHeaderBar().clearRightAction();
    m_urlContainer = nullptr;
    m_urlInput = nullptr;
    m_bookmarkBar = nullptr;
    m_contentArea = nullptr;
    BaseView::onDestroy();
}

void TlvBrowserView::onUplinkFrameGenerated(const uint8_t* frame, size_t len) {
    if (!frame || len == 0 || !s_instance) return;
    
    // Construir trama Mesh Header ultra-corta
    MeshHeader hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.control = MESH_CTRL_DST_ONLY | MESH_SVC_TLVGL_REQUEST;
    hdr.dst_id = 0x0001; // ID de Gateway / Servidor de Hosting
    hdr.is_dst_only = true;

    uint8_t packet[512];
    size_t hdr_len = build_mesh_header(packet, sizeof(packet), &hdr);
    if (hdr_len > 0 && hdr_len + len <= sizeof(packet)) {
        memcpy(packet + hdr_len, frame, len);
        // Si en el futuro se conecta el cliente de red o mesh, se enruta aquí
        char toastMsg[64];
        snprintf(toastMsg, sizeof(toastMsg), "Trama TLV: %uB enviada", (unsigned int)(hdr_len + len));
        UIManager::showToast(toastMsg);
    }
}

void TlvBrowserView::render(const uint8_t* data, size_t length) {
    if (!m_contentArea || !lv_obj_is_valid(m_contentArea) || !data || length == 0) return;
    lv_obj_clean(m_contentArea);
    tlv_browser_render(m_contentArea, data, length);
}

void TlvBrowserView::processNetworkPacket(const uint8_t* packet, size_t length) {
    MeshHeader hdr;
    size_t hdr_len = parse_mesh_header(packet, length, &hdr);
    if (hdr_len > 0 && hdr_len < length) {
        uint8_t service = hdr.control & 0x3F;
        if (service == MESH_SVC_TLVGL_RESPONSE) {
            render(packet + hdr_len, length - hdr_len);
        }
    }
}

void TlvBrowserView::navigateToUrl(const char* url) {
    if (!url || strlen(url) == 0) return;
    
    if (strcmp(url, "demo") == 0 || strcmp(url, "about:demo") == 0) {
        renderDemo();
        return;
    }

    // Verificar si es un archivo local en SD o Storage
    if (strncmp(url, "file://", 7) == 0) {
        loadLocalFile(url + 7);
        return;
    }
    if (url[0] == '/') {
        loadLocalFile(url);
        return;
    }

    uint8_t tlv_frame[256];
    size_t frame_len = tlv_build_req_url(tlv_frame, sizeof(tlv_frame), url);
    if (frame_len > 0) {
        onUplinkFrameGenerated(tlv_frame, frame_len);
    }
}

bool TlvBrowserView::loadLocalFile(const char* path) {
    if (!path || !m_contentArea) return false;

    FILE* f = fopen(path, "rb");
    if (!f) {
        char errBuf[128];
        snprintf(errBuf, sizeof(errBuf), "No se pudo abrir: %s", path);
        UIManager::showToast(errBuf);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz <= 0 || sz > 65536) {
        fclose(f);
        UIManager::showToast("Archivo TLV invalido o muy grande");
        return false;
    }

    std::vector<uint8_t> buffer(sz);
    size_t readBytes = fread(buffer.data(), 1, sz, f);
    fclose(f);

    if (readBytes == (size_t)sz) {
        render(buffer.data(), readBytes);
        UIManager::showToast("Pagina TLV cargada localmente");
        return true;
    }
    return false;
}

void TlvBrowserView::onUrlSubmit(lv_event_t* e) {
    lv_obj_t* ta = (lv_obj_t*)lv_event_get_user_data(e);
    if (ta && s_instance) {
        const char* url = lv_textarea_get_text(ta);
        s_instance->navigateToUrl(url);
    }
}

void TlvBrowserView::onBookmarkClick(lv_event_t* e) {
    const char* url = (const char*)lv_event_get_user_data(e);
    if (url && s_instance) {
        if (s_instance->m_urlInput && lv_obj_is_valid(s_instance->m_urlInput)) {
            lv_textarea_set_text(s_instance->m_urlInput, url);
        }
        s_instance->navigateToUrl(url);
    }
}

void TlvBrowserView::renderDemo() {
    TlvBuilder b;
    buildDemoPage(b);
    render(b.buf, b.len);
}

bool TlvBrowserView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;

    // Configurar HeaderBar global
    UIManager::getInstance().getHeaderBar().showWifi(false);
    UIManager::getInstance().getHeaderBar().setTitle("Navegador");
    UIManager::getInstance().getHeaderBar().showBackButton(true, []() {
        UIManager::getInstance().popView();
    });
    UIManager::getInstance().getHeaderBar().setRightAction(LV_SYMBOL_REFRESH, [this]() {
        this->renderDemo();
    });

    // Contenedor base de la vista
    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_container, 0, 0);
    DefaultTheme::disableScroll(m_container);

    lv_obj_set_flex_flow(m_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_container, 8, 0);
    lv_obj_set_style_pad_row(m_container, 6, 0);

    // 1. Barra de Direcciones URL
    m_urlContainer = lv_obj_create(m_container);
    lv_obj_set_width(m_urlContainer, lv_pct(100));
    lv_obj_set_height(m_urlContainer, 44);
    lv_obj_set_flex_flow(m_urlContainer, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(m_urlContainer, 4, 0);
    lv_obj_set_style_pad_column(m_urlContainer, 6, 0);
    lv_obj_set_style_bg_opa(m_urlContainer, LV_OPA_30, 0);
    lv_obj_set_style_border_width(m_urlContainer, 1, 0);
    lv_obj_set_style_border_color(m_urlContainer, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_radius(m_urlContainer, 8, 0);

    m_urlInput = lv_textarea_create(m_urlContainer);
    lv_obj_set_flex_grow(m_urlInput, 1);
    lv_obj_set_height(m_urlInput, LV_PCT(100));
    lv_textarea_set_one_line(m_urlInput, true);
    lv_textarea_set_placeholder_text(m_urlInput, "http://... o demo");
    lv_textarea_set_text(m_urlInput, "http://noticias.mesh");

    lv_obj_t* goBtn = lv_button_create(m_urlContainer);
    lv_obj_set_size(goBtn, 55, LV_PCT(100));
    lv_obj_t* btnLabel = lv_label_create(goBtn);
    lv_label_set_text(btnLabel, "Ir");
    lv_obj_center(btnLabel);

    lv_obj_add_event_cb(goBtn, onUrlSubmit, LV_EVENT_CLICKED, m_urlInput);
    lv_obj_add_event_cb(m_urlInput, onUrlSubmit, LV_EVENT_READY, m_urlInput);
    UIManager::attachKeyboard(m_urlInput);

    // 2. Barra de Marcadores Rápidos
    m_bookmarkBar = lv_obj_create(m_container);
    lv_obj_set_width(m_bookmarkBar, lv_pct(100));
    lv_obj_set_height(m_bookmarkBar, 36);
    lv_obj_set_flex_flow(m_bookmarkBar, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(m_bookmarkBar, 2, 0);
    lv_obj_set_style_pad_column(m_bookmarkBar, 6, 0);
    lv_obj_set_style_bg_opa(m_bookmarkBar, 0, 0);
    lv_obj_set_style_border_width(m_bookmarkBar, 0, 0);

    const char* bookmarks[] = {
        "http://noticias.mesh",
        "http://clima.mesh",
        "http://galeria.mesh",
        "demo"
    };

    const char* bmLabels[] = {
        "📰 noticias",
        "🌤️ clima",
        "🖼️ galeria",
        "⚡ demo"
    };

    for (int i = 0; i < 4; i++) {
        lv_obj_t* bmBtn = lv_button_create(m_bookmarkBar);
        lv_obj_set_height(bmBtn, LV_PCT(100));
        lv_obj_set_style_pad_hor(bmBtn, 8, 0);
        lv_obj_t* lbl = lv_label_create(bmBtn);
        lv_label_set_text(lbl, bmLabels[i]);
        lv_obj_center(lbl);
        lv_obj_add_event_cb(bmBtn, onBookmarkClick, LV_EVENT_CLICKED, (void*)bookmarks[i]);
    }

    // Configurar callback de eventos uplink
    tlv_browser_set_uplink_cb(onUplinkFrameGenerated);

    // 3. Área de contenido dinámico
    m_contentArea = lv_obj_create(m_container);
    lv_obj_set_width(m_contentArea, lv_pct(100));
    lv_obj_set_flex_grow(m_contentArea, 1);
    lv_obj_set_style_bg_opa(m_contentArea, 0, 0);
    lv_obj_set_style_border_width(m_contentArea, 0, 0);
    lv_obj_set_style_pad_all(m_contentArea, 0, 0);
    lv_obj_set_scrollbar_mode(m_contentArea, LV_SCROLLBAR_MODE_AUTO);

    // Cargar página inicial de demostración
    renderDemo();

    return true;
}

void TlvBrowserView::onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) {
    if (!m_container || !lv_obj_is_valid(m_container)) return;
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
}

} // namespace ui
} // namespace cbdos
