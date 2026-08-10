#include "TlvBrowserView.h"
#include "../Components/HeaderBar.h"
#include "../Themes/DefaultTheme.h"
#include "../../Core/tlv_parser.h"
#include <cstring>

lv_obj_t* TlvBrowserView::contentArea = nullptr;

// ── Constructor de bytes TLV (mismo formato que tlv_parser.c) ───────
namespace {
constexpr uint8_t TYPE_ABS_PAGE  = 0x10;
constexpr uint8_t TYPE_ABS_TEXT  = 0x11;
constexpr uint8_t TYPE_ABS_LINK  = 0x12;
constexpr uint8_t TYPE_END       = 0xFE;

struct TlvBuilder {
    uint8_t buf[512];
    size_t len = 0;

    void push8(uint8_t v) { buf[len++] = v; }
    void push16(uint16_t v) { push8(v >> 8); push8(v & 0xFF); }
    void push(const uint8_t* p, size_t n) { while (n--) buf[len++] = *p++; }

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

// Página de plantilla base del navegador (ejecutable 100% offline)
void buildDemoPage(TlvBuilder& b) {
    b.page();
    b.text(20, 30, 200, 30, 1, "Navegador TLVGL");
    b.text(20, 70, 200, 30, 0, "Plantilla base");
    b.text(20, 100, 200, 30, 0, "Sin transporte todavia");
    b.link(20, 150, 200, 42, 1, "Ir a Galeria");
    b.link(20, 205, 200, 42, 2, "Ir a Musica");
    b.end();
}
} // namespace

void TlvBrowserView::render(const uint8_t* data, size_t length) {
    if (!contentArea || !data || length == 0) return;
    lv_obj_clean(contentArea);
    tlv_browser_render(contentArea, data, length);
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
    lv_obj_set_style_pad_all(scr, 12, 0);
    lv_obj_set_style_pad_row(scr, 10, 0);

    HeaderBar* headerBar = HeaderBar::create(scr, "Navegador", true, true);
    HeaderBar::setActiveHeader(headerBar);

    // Área de contenido donde el parser TLV despliega la página
    contentArea = lv_obj_create(scr);
    lv_obj_set_width(contentArea, lv_pct(100));
    lv_obj_set_flex_grow(contentArea, 1);
    lv_obj_set_style_bg_opa(contentArea, 0, 0);
    lv_obj_set_style_border_width(contentArea, 0, 0);
    lv_obj_set_style_pad_all(contentArea, 0, 0);

    renderDemo();
    return scr;
}