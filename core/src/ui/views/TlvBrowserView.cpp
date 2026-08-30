#include "TlvBrowserView.hpp"
#include "../UIManager.hpp"
#include "../components/HeaderBar.hpp"
#include "../themes/DefaultTheme.h"
#include "cbdos/display.hpp"
#include "cbdos/storage.hpp"
#include "cbdos/network.hpp"
#include "cbdos/radio.hpp"
#include "cbdos/mesh/mesh_engine.hpp"
#include "cbdos/log.hpp"
#include <cstring>
#include <cstdio>
#include <vector>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>

static const char* TAG = "TlvBrowser";

namespace cbdos {
namespace ui {

TlvBrowserView* TlvBrowserView::s_instance = nullptr;

struct FetchParams {
    std::string host;
    uint16_t port;
    std::string path;
    uint8_t uplink_frame[256];
    size_t uplink_len;
};

struct RenderAsyncPayload {
    std::vector<uint8_t> data;
};

static void render_async_cb(void* user_data) {
    RenderAsyncPayload* payload = (RenderAsyncPayload*)user_data;
    if (payload && TlvBrowserView::getInstance()) {
        TlvBrowserView::getInstance()->processNetworkPacket(payload->data.data(), payload->data.size());
        UIManager::showToast("Pagina cargada");
        delete payload;
    }
}

TlvBrowserView::TlvBrowserView()
    : BaseView("TlvBrowser") {
    s_instance = this;
}

TlvBrowserView::~TlvBrowserView() {
    if (s_instance == this) {
        s_instance = nullptr;
    }
    if (m_fetchTaskHandle) {
        cbdos::rtos::deleteTask(m_fetchTaskHandle);
        m_fetchTaskHandle = nullptr;
    }
}

void TlvBrowserView::onDestroy() {
    if (s_instance == this) {
        s_instance = nullptr;
    }
    if (m_fetchTaskHandle) {
        cbdos::rtos::deleteTask(m_fetchTaskHandle);
        m_fetchTaskHandle = nullptr;
    }
    cbdos::mesh::MeshEngine::getInstance().unregisterServiceHandler(cbdos::mesh::ServiceId::TlvglResponse);
    UIManager::getInstance().getHeaderBar().clearRightAction();
    m_urlContainer = nullptr;
    m_urlInput = nullptr;
    m_bookmarkBar = nullptr;
    m_contentArea = nullptr;
    BaseView::onDestroy();
}

void TlvBrowserView::setGateway(const char* host, uint16_t port) {
    if (host && strlen(host) > 0) {
        m_gatewayHost = host;
    }
    if (port > 0) {
        m_gatewayPort = port;
    }
}

void TlvBrowserView::onUplinkFrameGenerated(const uint8_t* frame, size_t len) {
    if (!frame || len == 0 || !s_instance) return;

    uint8_t tag = frame[0];

    if (tag == TYPE_REQ_LINK_CLICK && len >= 4) {
        uint8_t link_id = frame[3];
        CBD_LOG_I(TAG, "Link click #%u -> enviando al gateway", link_id);
    } else if (tag == TYPE_REQ_CONTROL_EVT && len >= 6) {
        uint8_t elem_id = frame[3];
        int16_t val = (frame[4] << 8) | frame[5];
        char toastMsg[64];
        snprintf(toastMsg, sizeof(toastMsg), "Control %u: %d", elem_id, val);
        UIManager::showToast(toastMsg);
    } else if (tag == TYPE_REQ_INPUT_SUBMIT && len >= 4) {
        uint8_t elem_id = frame[3];
        char toastMsg[64];
        snprintf(toastMsg, sizeof(toastMsg), "Input %u enviado", elem_id);
        UIManager::showToast(toastMsg);
    }

    // Si estamos en modo radio / espnow, emitir trama por ESP-NOW
    if (s_instance->m_currentUrl.starts_with("espnow://") || s_instance->m_currentUrl.starts_with("radio://")) {
        auto& meshEngine = cbdos::mesh::MeshEngine::getInstance();
        if (meshEngine.isRunning()) {
            meshEngine.sendPacket(cbdos::mesh::ServiceId::TlvglRequest, 0xFFFF, frame, len, true, nullptr);
            return;
        }
    }

    // Enviar trama binaria al Gateway TCP
    FetchParams* params = new FetchParams();
    params->host = s_instance->m_gatewayHost;
    params->port = s_instance->m_gatewayPort;
    params->path = "";
    params->uplink_len = (len < sizeof(params->uplink_frame)) ? len : sizeof(params->uplink_frame);
    memcpy(params->uplink_frame, frame, params->uplink_len);

    cbdos::rtos::createTask(fetchTask, "tlv_fetch", 8192, params, 5, 1);
}

void TlvBrowserView::render(const uint8_t* data, size_t length) {
    if (!m_contentArea || !lv_obj_is_valid(m_contentArea) || !data || length == 0) return;
    lv_obj_clean(m_contentArea);
    tlv_browser_render(m_contentArea, data, length);
}

void TlvBrowserView::processNetworkPacket(const uint8_t* packet, size_t length) {
    if (!packet || length < 2) return;

    // Si empieza con 'PH', es payload TLV directo
    if (packet[0] == 0x50 && packet[1] == 0x48) {
        render(packet, length);
        return;
    }

    // Si viene con MeshHeader
    MeshHeader hdr;
    size_t hdr_len = parse_mesh_header(packet, length, &hdr);
    if (hdr_len > 0 && hdr_len < length) {
        render(packet + hdr_len, length - hdr_len);
    } else {
        render(packet, length);
    }
}

void TlvBrowserView::navigateToUrl(const char* url) {
    if (!url || strlen(url) == 0) return;
    m_currentUrl = url;

    if (m_urlInput && lv_obj_is_valid(m_urlInput)) {
        lv_textarea_set_text(m_urlInput, url);
    }

    // 1. Archivo local en almacenamiento MicroSD o Flash
    if (strncmp(url, "file://", 7) == 0) {
        loadLocalFile(url + 7);
        return;
    }
    if (url[0] == '/') {
        loadLocalFile(url);
        return;
    }

    // 2. Limpieza de protocolo y autodetección
    std::string clean = url;
    bool explicit_mesh = false;
    bool explicit_http = false;

    if (clean.starts_with("espnow://")) {
        clean = clean.substr(9);
        explicit_mesh = true;
    } else if (clean.starts_with("radio://")) {
        clean = clean.substr(8);
        explicit_mesh = true;
    } else if (clean.starts_with("mesh://")) {
        clean = clean.substr(7);
        explicit_mesh = true;
    } else if (clean.starts_with("http://")) {
        clean = clean.substr(7);
        explicit_http = true;
    } else if (clean.starts_with("https://")) {
        clean = clean.substr(8);
        explicit_http = true;
    }

    // Identificar si es un dominio .mesh o nombre directo (ej: "clima.mesh", "bento", "home.mesh")
    bool is_mesh_domain = clean.ends_with(".mesh") || explicit_mesh || 
                         (!explicit_http && clean.find('/') == std::string::npos && clean.find(':') == std::string::npos);

    std::string path = clean;
    if (is_mesh_domain && !path.ends_with(".mesh")) {
        path += ".mesh";
    }

    cbdos::radio::RadioMode currentRadio = cbdos::radio::getMode();
    bool use_radio = (currentRadio != cbdos::radio::RadioMode::WifiSta);

    // Si el modo de radio es ESP-NOW, ESP-NOW LR, Híbrido o similar: emitir por radio
    if (use_radio) {
        auto& meshEngine = cbdos::mesh::MeshEngine::getInstance();
        if (!meshEngine.isRunning()) {
            meshEngine.init(cbdos::radio::getChannel());
        }

        meshEngine.registerServiceHandler(cbdos::mesh::ServiceId::TlvglResponse, [](const cbdos::mesh::MeshPacket& packet) {
            if (!packet.payload.empty() && s_instance) {
                RenderAsyncPayload* async_payload = new RenderAsyncPayload();
                async_payload->data = packet.payload;
                lv_async_call(render_async_cb, async_payload);
            }
        });

        // Extraer únicamente el nombre de archivo si contiene slash
        size_t slash_pos = path.find('/');
        std::string filename = (slash_pos != std::string::npos) ? path.substr(slash_pos + 1) : path;
        if (filename.empty()) filename = "home.mesh";

        bool ok = meshEngine.sendTlvRequest(filename.c_str(), 0xFFFF);
        if (ok) {
            char tbuf[64];
            snprintf(tbuf, sizeof(tbuf), "Transmitiendo por %s", cbdos::radio::getModeName(currentRadio));
            UIManager::showToast(tbuf);
        } else {
            UIManager::showToast("Fallo transmision radio");
        }
        return;
    }

    // 3. Wi-Fi conectado: Consultar al Gateway TCP
    std::string host = m_gatewayHost;
    uint16_t port = m_gatewayPort;

    size_t slash_pos = clean.find('/');
    std::string host_port_part = (slash_pos != std::string::npos) ? clean.substr(0, slash_pos) : clean;
    if (slash_pos != std::string::npos) {
        path = clean.substr(slash_pos + 1);
    }

    if (host_port_part.find('.') != std::string::npos && !host_port_part.ends_with(".mesh")) {
        size_t colon_pos = host_port_part.find(':');
        if (colon_pos != std::string::npos) {
            host = host_port_part.substr(0, colon_pos);
            port = (uint16_t)atoi(host_port_part.substr(colon_pos + 1).c_str());
        } else {
            host = host_port_part;
            port = 8080;
        }
        m_gatewayHost = host;
        m_gatewayPort = port;
    }

    fetchFromGatewayAsync(host, port, path);
}

void TlvBrowserView::fetchFromGatewayAsync(const std::string& host, uint16_t port, const std::string& path) {
    FetchParams* params = new FetchParams();
    params->host = host;
    params->port = port;
    params->path = path;
    params->uplink_len = 0;

    cbdos::rtos::createTask(fetchTask, "tlv_fetch", 8192, params, 5, 1);
}

void TlvBrowserView::fetchTask(void* param) {
    FetchParams* p = (FetchParams*)param;
    if (!p) {
        cbdos::rtos::deleteTask(nullptr);
        return;
    }

    CBD_LOG_I(TAG, "Conectando a Gateway en %s:%u (path: %s)...", p->host.c_str(), p->port, p->path.c_str());

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        CBD_LOG_E(TAG, "No se pudo crear socket");
        UIManager::showToast("Error de socket");
        delete p;
        cbdos::rtos::deleteTask(nullptr);
        return;
    }

    // Timeout de 3 segundos
    struct timeval tv;
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(p->port);
    dest_addr.sin_addr.s_addr = inet_addr(p->host.c_str());

    if (dest_addr.sin_addr.s_addr == INADDR_NONE) {
        struct hostent *hp = gethostbyname(p->host.c_str());
        if (hp != NULL) {
            memcpy(&dest_addr.sin_addr, hp->h_addr_list[0], hp->h_length);
        } else {
            CBD_LOG_E(TAG, "DNS fallo para %s", p->host.c_str());
            close(sock);
            UIManager::showToast("Error de host/DNS");
            delete p;
            cbdos::rtos::deleteTask(nullptr);
            return;
        }
    }

    if (connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) != 0) {
        CBD_LOG_E(TAG, "Fallo al conectar con %s:%u", p->host.c_str(), p->port);
        char errBuf[64];
        snprintf(errBuf, sizeof(errBuf), "No se pudo conectar a %s:%u", p->host.c_str(), p->port);
        UIManager::showToast(errBuf);
        close(sock);
        delete p;
        cbdos::rtos::deleteTask(nullptr);
        return;
    }

    // 1. Construir trama MeshHeader + Payload
    uint8_t packet[512];
    MeshHeader hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.control = MESH_CTRL_DST_ONLY | MESH_SVC_TLVGL_REQUEST;
    hdr.dst_id = 0x0001;
    hdr.is_dst_only = true;

    size_t hdr_len = build_mesh_header(packet, sizeof(packet), &hdr);
    size_t total_tx = hdr_len;

    if (p->uplink_len > 0) {
        // Trama uplink ya provista (link click, input, control)
        memcpy(packet + total_tx, p->uplink_frame, p->uplink_len);
        total_tx += p->uplink_len;
    } else {
        // Trama de petición URL
        size_t req_len = tlv_build_req_url(packet + total_tx, sizeof(packet) - total_tx, p->path.c_str());
        total_tx += req_len;
    }

    // 2. Enviar petición
    send(sock, packet, total_tx, 0);
    CBD_LOG_I(TAG, "Enviados %u bytes al Gateway", (unsigned int)total_tx);

    // 3. Recibir respuesta
    std::vector<uint8_t> response_buf;
    uint8_t rx_chunk[1024];
    int r;
    while ((r = recv(sock, rx_chunk, sizeof(rx_chunk), 0)) > 0) {
        response_buf.insert(response_buf.end(), rx_chunk, rx_chunk + r);
        if (response_buf.size() > 65536) break;
    }

    close(sock);
    CBD_LOG_I(TAG, "Recibidos %u bytes desde Gateway", (unsigned int)response_buf.size());

    if (!response_buf.empty()) {
        RenderAsyncPayload* async_payload = new RenderAsyncPayload();
        async_payload->data = std::move(response_buf);
        lv_async_call(render_async_cb, async_payload);
    } else {
        UIManager::showToast("Respuesta vacia del servidor");
    }

    delete p;
    cbdos::rtos::deleteTask(nullptr);
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
        UIManager::showToast("Archivo invalido");
        return false;
    }

    std::vector<uint8_t> buffer(sz);
    size_t readBytes = fread(buffer.data(), 1, sz, f);
    fclose(f);

    if (readBytes == (size_t)sz) {
        processNetworkPacket(buffer.data(), readBytes);
        UIManager::showToast("Archivo cargado");
        return true;
    }
    return false;
}

void TlvBrowserView::showInitialState() {
    if (!m_contentArea || !lv_obj_is_valid(m_contentArea)) return;
    lv_obj_clean(m_contentArea);

    // Mensaje de estado inicial del navegador
    lv_obj_t* card = lv_obj_create(m_contentArea);
    lv_obj_set_size(card, LV_PCT(100), 130);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_20, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(card, 12, 0);

    lv_obj_t* title = lv_label_create(card);
    lv_label_set_text(title, "Navegador Alternet / TLVGL");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, lv_palette_main(LV_PALETTE_CYAN), 0);

    lv_obj_t* sub = lv_label_create(card);
    lv_label_set_text(sub, "Escribe un dominio (ej: clima.mesh) o toca un marcador.\nEnrutamiento automático vía Wi-Fi o Radio Mesh.");
    lv_obj_set_width(sub, LV_PCT(100));
    lv_label_set_long_mode(sub, LV_LABEL_LONG_MODE_WRAP);
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
        s_instance->navigateToUrl(url);
    }
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
        this->navigateToUrl(this->m_currentUrl.c_str());
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
    lv_textarea_set_placeholder_text(m_urlInput, "clima.mesh, bento.mesh...");
    lv_textarea_set_text(m_urlInput, m_currentUrl.c_str());

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
        "home.mesh",
        "clima.mesh",
        "bento.mesh",
        "config.mesh"
    };

    const char* bmLabels[] = {
        "⚡ Inicio",
        "🌤️ Clima",
        "📱 Bento",
        "⚙️ Config"
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

    // Estado inicial
    showInitialState();

    return true;
}

void TlvBrowserView::onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) {
    if (!m_container || !lv_obj_is_valid(m_container)) return;
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
}

} // namespace ui
} // namespace cbdos
