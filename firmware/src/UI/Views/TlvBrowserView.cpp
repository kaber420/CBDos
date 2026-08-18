#include "TlvBrowserView.h"
#include "../UIManager.h"
#include "../Components/HeaderBar.h"
#include "../Themes/DefaultTheme.h"
#include "../../Core/tlv_parser.h"
#include "../../Core/mesh_header.h"
#include "../../Network/TlvNetworkClient.h"
#include <cstring>
#include <cstdio>

lv_obj_t* TlvBrowserView::contentArea = nullptr;
lv_obj_t* TlvBrowserView::urlInput = nullptr;

namespace {

struct TlvBuilder {
    uint8_t buf[512];
    size_t len = 0;

    void push8(uint8_t v) { buf[len++] = v; }
    void push16(uint16_t v) { push8(v >> 8); push8(v & 0xFF); }
    void push(const uint8_t* p, size_t n) { while (n--) buf[len++] = *p++; }

    void magic() { push8(0x50); push8(0x48); }

    void node(uint8_t type, const uint8_t* value, size_t vlen) {
        if (len + 3 + vlen > sizeof(buf)) return;
        push8(type);
        push16((uint16_t)vlen);
        push(value, vlen);
    }

    void page() { node(TYPE_ABS_PAGE, nullptr, 0); }

    void text(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t style, const char* s) {
        uint8_t v[9];
        v[0]=x>>8; v[1]=x&0xFF; v[2]=y>>8; v[3]=y&0xFF;
        v[4]=w>>8; v[5]=w&0xFF; v[6]=h>>8; v[7]=h&0xFF; v[8]=style;
        node(TYPE_ABS_TEXT, v, 9);
        size_t n = strlen(s);
        if (len + n <= sizeof(buf)) push((const uint8_t*)s, n);
    }

    void link(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t id, const char* s) {
        uint8_t v[9];
        v[0]=x>>8; v[1]=x&0xFF; v[2]=y>>8; v[3]=y&0xFF;
        v[4]=w>>8; v[5]=w&0xFF; v[6]=h>>8; v[7]=h&0xFF; v[8]=id;
        node(TYPE_ABS_LINK, v, 9);
        size_t n = strlen(s);
        if (len + n <= sizeof(buf)) push((const uint8_t*)s, n);
    }

    void end() { push8(TYPE_END); }
};

void buildDemoPage(TlvBuilder& b) {
    b.magic();
    b.page();
    b.text(20, 20, 240, 30, 1, "Navegador TLVGL");
    b.text(20, 60, 240, 30, 0, "Red Mesh C/C++ Activa");
    b.text(20, 90, 240, 30, 0, "Pseudo-BGP & OSPF Ready");
    b.link(20, 140, 200, 42, 1, "Ir a Galeria");
    b.link(20, 195, 200, 42, 2, "Ir a Musica");
    b.end();
}
} // namespace

void TlvBrowserView::onUplinkFrameGenerated(const uint8_t* frame, size_t len) {
    if (!frame || len == 0) return;
    
    MeshHeader hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.control = MESH_CTRL_DST_ONLY | MESH_SVC_TLVGL_REQUEST; // Cabecera ultra-corta de 3 bytes
    hdr.dst_id = 0x0001; // Short ID de destino del Servidor Hosting (0x0001)
    hdr.is_dst_only = true;

    
    uint8_t packet[512];
    size_t hdr_len = build_mesh_header(packet, sizeof(packet), &hdr);
    if (hdr_len > 0 && hdr_len + len <= sizeof(packet)) {
        memcpy(packet + hdr_len, frame, len);
        TlvNetworkClient::sendRequest(packet, hdr_len + len);
    }
}

void TlvBrowserView::render(const uint8_t* data, size_t length) {
    if (!contentArea || !data || length == 0) return;
    lv_obj_clean(contentArea);
    tlv_browser_render(contentArea, data, length);
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
    
    uint8_t tlv_frame[256];
    size_t frame_len = tlv_build_req_url(tlv_frame, sizeof(tlv_frame), url);
    if (frame_len > 0) {
        onUplinkFrameGenerated(tlv_frame, frame_len);
    }
}

void TlvBrowserView::onUrlSubmit(lv_event_t* e) {
    lv_obj_t* ta = (lv_obj_t*)lv_event_get_user_data(e);
    if (ta) {
        const char* url = lv_textarea_get_text(ta);
        navigateToUrl(url);
    }
}


void TlvBrowserView::onBookmarkClick(lv_event_t* e) {
    const char* url = (const char*)lv_event_get_user_data(e);
    if (url && urlInput) {
        lv_textarea_set_text(urlInput, url);
        navigateToUrl(url);
    }
}

void TlvBrowserView::renderDemo() {
    TlvBuilder b;
    buildDemoPage(b);
    render(b.buf, b.len);
}

lv_obj_t* TlvBrowserView::create() {
    lv_obj_t* scr = lv_obj_create(NULL);
    DefaultTheme::applyFlatBg(scr);
    DefaultTheme::disableScroll(scr);

    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(scr, 8, 0);
    lv_obj_set_style_pad_row(scr, 6, 0);

    HeaderBar* headerBar = HeaderBar::create(scr, "Navegador", true, true);
    HeaderBar::setActiveHeader(headerBar);

    // 1. Contenedor de la barra de direcciones URL
    lv_obj_t* urlContainer = lv_obj_create(scr);
    lv_obj_set_width(urlContainer, lv_pct(100));
    lv_obj_set_height(urlContainer, 42);
    lv_obj_set_flex_flow(urlContainer, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(urlContainer, 4, 0);
    lv_obj_set_style_pad_column(urlContainer, 6, 0);
    lv_obj_set_style_bg_opa(urlContainer, LV_OPA_30, 0);
    lv_obj_set_style_border_width(urlContainer, 1, 0);
    lv_obj_set_style_border_color(urlContainer, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_radius(urlContainer, 8, 0);

    urlInput = lv_textarea_create(urlContainer);
    lv_obj_set_flex_grow(urlInput, 1);
    lv_obj_set_height(urlInput, LV_PCT(100));
    lv_textarea_set_one_line(urlInput, true);
    lv_textarea_set_placeholder_text(urlInput, "http://...");
    lv_textarea_set_text(urlInput, "http://noticias.mesh");

    lv_obj_t* goBtn = lv_button_create(urlContainer);
    lv_obj_set_size(goBtn, 55, LV_PCT(100));
    lv_obj_t* btnLabel = lv_label_create(goBtn);
    lv_label_set_text(btnLabel, "Ir");
    lv_obj_center(btnLabel);

    lv_obj_add_event_cb(goBtn, onUrlSubmit, LV_EVENT_CLICKED, urlInput);
    lv_obj_add_event_cb(urlInput, onUrlSubmit, LV_EVENT_READY, urlInput);
    UIManager::attachKeyboard(urlInput);

    // 2. Barra de Marcadores Rápidos (1-Touch Bookmarks)
    lv_obj_t* bookmarkBar = lv_obj_create(scr);
    lv_obj_set_width(bookmarkBar, lv_pct(100));
    lv_obj_set_height(bookmarkBar, 36);
    lv_obj_set_flex_flow(bookmarkBar, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(bookmarkBar, 2, 0);
    lv_obj_set_style_pad_column(bookmarkBar, 4, 0);
    lv_obj_set_style_bg_opa(bookmarkBar, 0, 0);
    lv_obj_set_style_border_width(bookmarkBar, 0, 0);

    const char* bookmarks[] = {
        "http://noticias.mesh",
        "http://clima.mesh",
        "http://galeria.mesh"
    };

    const char* bmLabels[] = {
        "📰 noticias",
        "🌤️ clima",
        "🖼️ galeria"
    };

    for (int i = 0; i < 3; i++) {
        lv_obj_t* bmBtn = lv_button_create(bookmarkBar);
        lv_obj_set_height(bmBtn, LV_PCT(100));
        lv_obj_set_style_pad_hor(bmBtn, 8, 0);
        lv_obj_t* lbl = lv_label_create(bmBtn);
        lv_label_set_text(lbl, bmLabels[i]);
        lv_obj_center(lbl);
        lv_obj_add_event_cb(bmBtn, onBookmarkClick, LV_EVENT_CLICKED, (void*)bookmarks[i]);
    }

    // Configurar callback de eventos uplink y recepción en la capa de red C++
    tlv_browser_set_uplink_cb(onUplinkFrameGenerated);
    TlvNetworkClient::setPacketRecvCallback(processNetworkPacket);

    // 3. Área de contenido donde el parser TLV despliega la página
    contentArea = lv_obj_create(scr);
    lv_obj_set_width(contentArea, lv_pct(100));
    lv_obj_set_flex_grow(contentArea, 1);
    lv_obj_set_style_bg_opa(contentArea, 0, 0);
    lv_obj_set_style_border_width(contentArea, 0, 0);
    lv_obj_set_style_pad_all(contentArea, 0, 0);

    renderDemo();
    return scr;
}