#include "DashboardView.hpp"
#include "ConfigView.hpp"
#include "MusicPlayerView.hpp"
#include "AudioRecorderView.hpp"
#include "GalleryListView.hpp"
#include "FlasherView.hpp"
#include "RadioView.hpp"
#include "LuaRunnerView.hpp"
#include "TextEditorView.hpp"
#include "CartridgeView.hpp"
#include "FileManagerView.hpp"
#include "SerialTerminalView.hpp"
#include "UtilitiesView.hpp"
#include "TlvBrowserView.hpp"
#include "LuappView.hpp"
#include "../../lua/LuappManager.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include "cbdos/display.hpp"
#include "cbdos/system.hpp"
#include <cstring>

namespace cbdos {
namespace ui {

DashboardView::DashboardView()
    : BaseView("Dashboard") {
}

bool DashboardView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;

    // Resetear lista de aplicaciones
    m_apps = {
        {"browser", "Navegador", LV_SYMBOL_EYE_OPEN, 0x00B4D8, false, ""},
        {"gallery", "Galeria", LV_SYMBOL_IMAGE, 0xEC4899, false, ""},
        {"files", "Archivos", LV_SYMBOL_DIRECTORY, 0xF77F00, false, ""},
        {"utilities", "Utilidades", LV_SYMBOL_LIST, 0x00F5D4, false, ""},
        {"cartridge", "Cartuchos", LV_SYMBOL_PLAY, 0x3F68D9, false, ""},
        {"lua", "Lua Runner", LV_SYMBOL_FILE, 0x06B6D4, false, ""},
        {"editor", "Editor", LV_SYMBOL_EDIT, 0x3B82F6, false, ""},
        {"radio", "Radio Online", LV_SYMBOL_WIFI, 0x10B981, false, ""},
        {"flasher", "Flasheador", LV_SYMBOL_DOWNLOAD, 0xF59E0B, false, ""},
        {"terminal", "Terminal UART", LV_SYMBOL_KEYBOARD, 0x10B981, false, ""},
        {"recorder", "Grabadora", LV_SYMBOL_AUDIO, 0xEF4444, false, ""},
        {"music", "Musica", LV_SYMBOL_AUDIO, 0x00E5FF, false, ""},
        {"config", "Configuracion", LV_SYMBOL_SETTINGS, 0x9D4EDD, false, ""}
    };

    // Escanear dinámicamente aplicaciones .luapp en la MicroSD
    auto& luappMgr = cbdos::lua::LuappManager::getInstance();
    luappMgr.scanApps("/sdcard/apps");
    for (const auto& app : luappMgr.getDiscoveredApps()) {
        m_apps.push_back({
            "luapp_" + app.name,
            app.name,
            app.iconSymbol,
            app.accentColor,
            true,
            app.filePath
        });
    }

    // Contenedor principal con scroll vertical suave (Fondo transparente para ver el Wallpaper)
    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_container, 0, 0);
    lv_obj_set_style_radius(m_container, 0, 0);
    lv_obj_set_style_pad_all(m_container, 12, 0);
    lv_obj_set_scrollbar_mode(m_container, LV_SCROLLBAR_MODE_AUTO);

    setupLayout();
    createCards();
    return true;
}

void DashboardView::onDestroy() {
    m_cardObjs.clear();
    BaseView::onDestroy();
}

void DashboardView::setupLayout() {
    auto caps = cbdos::display::getCapabilities();

    // Configurar Flex Wrap para distribución automática de 3 columnas
    lv_obj_set_flex_flow(m_container, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(m_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    
    if (caps.width >= 480) {
        // ESP32-P4 (480x800): 3 columnas amplias
        lv_obj_set_style_pad_row(m_container, 20, 0);
        lv_obj_set_style_pad_column(m_container, 8, 0);
        lv_obj_set_style_pad_hor(m_container, 16, 0);
        lv_obj_set_style_pad_ver(m_container, 16, 0);
    } else {
        // ESP32-S3 (320x480): 3 columnas compactas
        lv_obj_set_style_pad_row(m_container, 14, 0);
        lv_obj_set_style_pad_column(m_container, 6, 0);
        lv_obj_set_style_pad_hor(m_container, 10, 0);
        lv_obj_set_style_pad_ver(m_container, 12, 0);
    }
}

void DashboardView::createCards() {
    auto caps = cbdos::display::getCapabilities();

    // Dimensiones para 3 iconos por fila: más grandes y cómodos al tacto
    int32_t itemWidth = (caps.width >= 480) ? 140 : 96;
    int32_t iconSize = (caps.width >= 480) ? 84 : 64;
    int32_t iconRadius = (caps.width >= 480) ? 24 : 18;

    m_cardObjs.clear();

    for (size_t i = 0; i < m_apps.size(); ++i) {
        const auto& app = m_apps[i];

        // 1. Contenedor vertical transparente de la app (Ícono + Título)
        lv_obj_t* appItem = lv_obj_create(m_container);
        lv_obj_set_size(appItem, itemWidth, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(appItem, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(appItem, 0, 0);
        lv_obj_set_style_pad_all(appItem, 0, 0);
        DefaultTheme::disableScroll(appItem);

        lv_obj_set_flex_flow(appItem, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(appItem, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        // 2. Botón Squircle del Icono (iOS / Android style)
        lv_obj_t* btnIcon = lv_button_create(appItem);
        lv_obj_set_size(btnIcon, iconSize, iconSize);
        lv_obj_set_style_radius(btnIcon, iconRadius, 0);
        lv_obj_set_style_bg_color(btnIcon, lv_color_hex(0x1E2230), 0);
        lv_obj_set_style_bg_grad_color(btnIcon, lv_color_hex(0x13161F), 0);
        lv_obj_set_style_bg_grad_dir(btnIcon, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_border_color(btnIcon, lv_color_hex(app.accentColor), 0);
        lv_obj_set_style_border_width(btnIcon, 1, 0);
        lv_obj_set_style_border_opa(btnIcon, LV_OPA_60, 0);
        lv_obj_set_style_shadow_width(btnIcon, 12, 0);
        lv_obj_set_style_shadow_color(btnIcon, lv_color_black(), 0);
        lv_obj_set_style_shadow_opa(btnIcon, LV_OPA_40, 0);
        lv_obj_set_style_pad_all(btnIcon, 0, 0);

        if (app.id == "recorder") {
            // 🎙️ Icono vectorial estilizado de Micrófono
            lv_obj_t* micBox = lv_obj_create(btnIcon);
            lv_obj_set_size(micBox, 28, 36);
            lv_obj_set_style_bg_opa(micBox, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(micBox, 0, 0);
            lv_obj_set_style_pad_all(micBox, 0, 0);
            lv_obj_center(micBox);
            lv_obj_remove_flag(micBox, LV_OBJ_FLAG_CLICKABLE);
            DefaultTheme::disableScroll(micBox);

            lv_obj_t* capsule = lv_obj_create(micBox);
            lv_obj_set_size(capsule, 12, 20);
            lv_obj_set_style_radius(capsule, 6, 0);
            lv_obj_set_style_bg_color(capsule, lv_color_hex(0xEF4444), 0);
            lv_obj_set_style_border_color(capsule, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_border_width(capsule, 1, 0);
            lv_obj_align(capsule, LV_ALIGN_TOP_MID, 0, 0);
            lv_obj_remove_flag(capsule, LV_OBJ_FLAG_CLICKABLE);

            lv_obj_t* arcHolder = lv_obj_create(micBox);
            lv_obj_set_size(arcHolder, 22, 14);
            lv_obj_set_style_radius(arcHolder, 11, 0);
            lv_obj_set_style_bg_opa(arcHolder, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_color(arcHolder, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_border_width(arcHolder, 2, 0);
            lv_obj_set_style_border_side(arcHolder, (lv_border_side_t)(LV_BORDER_SIDE_BOTTOM | LV_BORDER_SIDE_LEFT | LV_BORDER_SIDE_RIGHT), 0);
            lv_obj_align(arcHolder, LV_ALIGN_TOP_MID, 0, 9);
            lv_obj_remove_flag(arcHolder, LV_OBJ_FLAG_CLICKABLE);

            lv_obj_t* stem = lv_obj_create(micBox);
            lv_obj_set_size(stem, 2, 6);
            lv_obj_set_style_bg_color(stem, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_border_width(stem, 0, 0);
            lv_obj_align(stem, LV_ALIGN_BOTTOM_MID, 0, -4);
            lv_obj_remove_flag(stem, LV_OBJ_FLAG_CLICKABLE);

            lv_obj_t* base = lv_obj_create(micBox);
            lv_obj_set_size(base, 14, 2);
            lv_obj_set_style_bg_color(base, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_border_width(base, 0, 0);
            lv_obj_align(base, LV_ALIGN_BOTTOM_MID, 0, -2);
            lv_obj_remove_flag(base, LV_OBJ_FLAG_CLICKABLE);

        } else if (app.id == "radio") {
            // 📻 Icono vectorial de Radio Vintage / Boombox con antena y dial
            lv_obj_t* radioBox = lv_obj_create(btnIcon);
            lv_obj_set_size(radioBox, 38, 32);
            lv_obj_set_style_bg_opa(radioBox, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(radioBox, 0, 0);
            lv_obj_set_style_pad_all(radioBox, 0, 0);
            lv_obj_center(radioBox);
            lv_obj_remove_flag(radioBox, LV_OBJ_FLAG_CLICKABLE);
            DefaultTheme::disableScroll(radioBox);

            // Antena telescópica inclinada
            lv_obj_t* ant = lv_obj_create(radioBox);
            lv_obj_set_size(ant, 18, 2);
            lv_obj_set_style_bg_color(ant, lv_color_hex(0x10B981), 0);
            lv_obj_set_style_border_width(ant, 0, 0);
            lv_obj_set_style_transform_rotation(ant, 250, 0); // ~25 grados
            lv_obj_align(ant, LV_ALIGN_TOP_LEFT, 2, 2);
            lv_obj_remove_flag(ant, LV_OBJ_FLAG_CLICKABLE);

            // Cuerpo del radio
            lv_obj_t* body = lv_obj_create(radioBox);
            lv_obj_set_size(body, 36, 22);
            lv_obj_set_style_radius(body, 6, 0);
            lv_obj_set_style_bg_color(body, lv_color_hex(0x064E3B), 0);
            lv_obj_set_style_border_color(body, lv_color_hex(0x10B981), 0);
            lv_obj_set_style_border_width(body, 2, 0);
            lv_obj_align(body, LV_ALIGN_BOTTOM_MID, 0, 0);
            lv_obj_remove_flag(body, LV_OBJ_FLAG_CLICKABLE);

            // Altavoz circular izquierdo
            lv_obj_t* spk = lv_obj_create(body);
            lv_obj_set_size(spk, 12, 12);
            lv_obj_set_style_radius(spk, 6, 0);
            lv_obj_set_style_bg_color(spk, lv_color_hex(0x10B981), 0);
            lv_obj_set_style_border_width(spk, 0, 0);
            lv_obj_align(spk, LV_ALIGN_LEFT_MID, 2, 0);
            lv_obj_remove_flag(spk, LV_OBJ_FLAG_CLICKABLE);

            // Dial / Botón sintonizador derecho
            lv_obj_t* knob1 = lv_obj_create(body);
            lv_obj_set_size(knob1, 10, 4);
            lv_obj_set_style_radius(knob1, 2, 0);
            lv_obj_set_style_bg_color(knob1, lv_color_hex(0xA7F3D0), 0);
            lv_obj_set_style_border_width(knob1, 0, 0);
            lv_obj_align(knob1, LV_ALIGN_TOP_RIGHT, -2, 2);
            lv_obj_remove_flag(knob1, LV_OBJ_FLAG_CLICKABLE);

            lv_obj_t* knob2 = lv_obj_create(body);
            lv_obj_set_size(knob2, 6, 6);
            lv_obj_set_style_radius(knob2, 3, 0);
            lv_obj_set_style_bg_color(knob2, lv_color_hex(0x34D399), 0);
            lv_obj_set_style_border_width(knob2, 0, 0);
            lv_obj_align(knob2, LV_ALIGN_BOTTOM_RIGHT, -4, -2);
            lv_obj_remove_flag(knob2, LV_OBJ_FLAG_CLICKABLE);

        } else if (app.id == "cartridge") {
            // 🎮 Icono de Cartucho Retro (GameBoy / NES)
            lv_obj_t* cartBox = lv_obj_create(btnIcon);
            lv_obj_set_size(cartBox, 32, 34);
            lv_obj_set_style_bg_opa(cartBox, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(cartBox, 0, 0);
            lv_obj_set_style_pad_all(cartBox, 0, 0);
            lv_obj_center(cartBox);
            lv_obj_remove_flag(cartBox, LV_OBJ_FLAG_CLICKABLE);
            DefaultTheme::disableScroll(cartBox);

            // Carcasa plástica del cartucho
            lv_obj_t* shell = lv_obj_create(cartBox);
            lv_obj_set_size(shell, 30, 32);
            lv_obj_set_style_radius(shell, 4, 0);
            lv_obj_set_style_bg_color(shell, lv_color_hex(0x1E3A8A), 0);
            lv_obj_set_style_border_color(shell, lv_color_hex(0x3B82F6), 0);
            lv_obj_set_style_border_width(shell, 2, 0);
            lv_obj_center(shell);
            lv_obj_remove_flag(shell, LV_OBJ_FLAG_CLICKABLE);

            // Etiqueta del juego (Sticker frontal)
            lv_obj_t* labelSticker = lv_obj_create(shell);
            lv_obj_set_size(labelSticker, 18, 16);
            lv_obj_set_style_radius(labelSticker, 2, 0);
            lv_obj_set_style_bg_color(labelSticker, lv_color_hex(0x60A5FA), 0);
            lv_obj_set_style_border_width(labelSticker, 0, 0);
            lv_obj_align(labelSticker, LV_ALIGN_CENTER, 0, 2);
            lv_obj_remove_flag(labelSticker, LV_OBJ_FLAG_CLICKABLE);

            // Muesca superior típica de GameBoy
            lv_obj_t* notch = lv_obj_create(shell);
            lv_obj_set_size(notch, 8, 3);
            lv_obj_set_style_bg_color(notch, lv_color_hex(0x1E2230), 0);
            lv_obj_set_style_border_width(notch, 0, 0);
            lv_obj_align(notch, LV_ALIGN_TOP_RIGHT, -2, -2);
            lv_obj_remove_flag(notch, LV_OBJ_FLAG_CLICKABLE);

        } else if (app.id == "lua") {
            // 🌙 Icono oficial de Lua (Planeta azul con luna creciente y órbita)
            lv_obj_t* luaBox = lv_obj_create(btnIcon);
            lv_obj_set_size(luaBox, 34, 34);
            lv_obj_set_style_bg_opa(luaBox, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(luaBox, 0, 0);
            lv_obj_set_style_pad_all(luaBox, 0, 0);
            lv_obj_center(luaBox);
            lv_obj_remove_flag(luaBox, LV_OBJ_FLAG_CLICKABLE);
            DefaultTheme::disableScroll(luaBox);

            // Planeta Tierra / Cuerpo principal azul
            lv_obj_t* planet = lv_obj_create(luaBox);
            lv_obj_set_size(planet, 22, 22);
            lv_obj_set_style_radius(planet, 11, 0);
            lv_obj_set_style_bg_color(planet, lv_color_hex(0x0284C7), 0);
            lv_obj_set_style_border_color(planet, lv_color_hex(0x38BDF8), 0);
            lv_obj_set_style_border_width(planet, 2, 0);
            lv_obj_align(planet, LV_ALIGN_BOTTOM_LEFT, 0, 0);
            lv_obj_remove_flag(planet, LV_OBJ_FLAG_CLICKABLE);

            // Luna satélite en órbita
            lv_obj_t* moon = lv_obj_create(luaBox);
            lv_obj_set_size(moon, 10, 10);
            lv_obj_set_style_radius(moon, 5, 0);
            lv_obj_set_style_bg_color(moon, lv_color_hex(0xF0F9FF), 0);
            lv_obj_set_style_border_color(moon, lv_color_hex(0x7DD3FC), 0);
            lv_obj_set_style_border_width(moon, 1, 0);
            lv_obj_align(moon, LV_ALIGN_TOP_RIGHT, 0, 0);
            lv_obj_remove_flag(moon, LV_OBJ_FLAG_CLICKABLE);

        } else if (app.id == "terminal") {
            // 💻 Icono de Terminal de Comandos CLI (>_)
            lv_obj_t* termBox = lv_obj_create(btnIcon);
            lv_obj_set_size(termBox, 36, 28);
            lv_obj_set_style_radius(termBox, 5, 0);
            lv_obj_set_style_bg_color(termBox, lv_color_hex(0x052E16), 0);
            lv_obj_set_style_border_color(termBox, lv_color_hex(0x22C55E), 0);
            lv_obj_set_style_border_width(termBox, 2, 0);
            lv_obj_set_style_pad_all(termBox, 0, 0);
            lv_obj_center(termBox);
            lv_obj_remove_flag(termBox, LV_OBJ_FLAG_CLICKABLE);
            DefaultTheme::disableScroll(termBox);

            // Texto de prompt ">_" verde fósforo
            lv_obj_t* lblPrompt = lv_label_create(termBox);
            lv_label_set_text(lblPrompt, ">_");
            lv_obj_set_style_text_color(lblPrompt, lv_color_hex(0x4ADE80), 0);
            lv_obj_set_style_text_font(lblPrompt, &lv_font_montserrat_16, 0);
            lv_obj_center(lblPrompt);

        } else if (app.id == "browser") {
            // 🌐 Navegador Cyberpunk (Globo con meridianos y anillo orbital neón)
            lv_obj_t* globBox = lv_obj_create(btnIcon);
            lv_obj_set_size(globBox, 36, 36);
            lv_obj_set_style_bg_opa(globBox, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(globBox, 0, 0);
            lv_obj_set_style_pad_all(globBox, 0, 0);
            lv_obj_center(globBox);
            lv_obj_remove_flag(globBox, LV_OBJ_FLAG_CLICKABLE);
            DefaultTheme::disableScroll(globBox);

            // Esfera central cian
            lv_obj_t* globe = lv_obj_create(globBox);
            lv_obj_set_size(globe, 24, 24);
            lv_obj_set_style_radius(globe, 12, 0);
            lv_obj_set_style_bg_color(globe, lv_color_hex(0x083344), 0);
            lv_obj_set_style_border_color(globe, lv_color_hex(0x06B6D4), 0);
            lv_obj_set_style_border_width(globe, 2, 0);
            lv_obj_center(globe);
            lv_obj_remove_flag(globe, LV_OBJ_FLAG_CLICKABLE);

            // Meridiano vertical interno
            lv_obj_t* merid = lv_obj_create(globe);
            lv_obj_set_size(merid, 10, 20);
            lv_obj_set_style_radius(merid, 5, 0);
            lv_obj_set_style_bg_opa(merid, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_color(merid, lv_color_hex(0x22D3EE), 0);
            lv_obj_set_style_border_width(merid, 1, 0);
            lv_obj_center(merid);
            lv_obj_remove_flag(merid, LV_OBJ_FLAG_CLICKABLE);

            // Anillo orbital magenta neón inclinado
            lv_obj_t* ring = lv_obj_create(globBox);
            lv_obj_set_size(ring, 34, 12);
            lv_obj_set_style_radius(ring, 6, 0);
            lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_color(ring, lv_color_hex(0xF43F5E), 0);
            lv_obj_set_style_border_width(ring, 2, 0);
            lv_obj_set_style_transform_rotation(ring, 300, 0); // ~30 grados
            lv_obj_center(ring);
            lv_obj_remove_flag(ring, LV_OBJ_FLAG_CLICKABLE);

        } else if (app.id == "editor") {
            // 📝 Editor Glassmorphism (Hoja de código con syntax highlighting + lápiz)
            lv_obj_t* editBox = lv_obj_create(btnIcon);
            lv_obj_set_size(editBox, 34, 36);
            lv_obj_set_style_bg_opa(editBox, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(editBox, 0, 0);
            lv_obj_set_style_pad_all(editBox, 0, 0);
            lv_obj_center(editBox);
            lv_obj_remove_flag(editBox, LV_OBJ_FLAG_CLICKABLE);
            DefaultTheme::disableScroll(editBox);

            // Hoja de código
            lv_obj_t* sheet = lv_obj_create(editBox);
            lv_obj_set_size(sheet, 24, 30);
            lv_obj_set_style_radius(sheet, 4, 0);
            lv_obj_set_style_bg_color(sheet, lv_color_hex(0x1E293B), 0);
            lv_obj_set_style_border_color(sheet, lv_color_hex(0x38BDF8), 0);
            lv_obj_set_style_border_width(sheet, 1, 0);
            lv_obj_align(sheet, LV_ALIGN_LEFT_MID, 0, 0);
            lv_obj_remove_flag(sheet, LV_OBJ_FLAG_CLICKABLE);

            // Renglones de código sintáctico
            lv_obj_t* line1 = lv_obj_create(sheet);
            lv_obj_set_size(line1, 10, 2);
            lv_obj_set_style_bg_color(line1, lv_color_hex(0xA855F7), 0);
            lv_obj_set_style_border_width(line1, 0, 0);
            lv_obj_align(line1, LV_ALIGN_TOP_LEFT, 2, 4);
            lv_obj_remove_flag(line1, LV_OBJ_FLAG_CLICKABLE);

            lv_obj_t* line2 = lv_obj_create(sheet);
            lv_obj_set_size(line2, 14, 2);
            lv_obj_set_style_bg_color(line2, lv_color_hex(0x38BDF8), 0);
            lv_obj_set_style_border_width(line2, 0, 0);
            lv_obj_align(line2, LV_ALIGN_TOP_LEFT, 2, 9);
            lv_obj_remove_flag(line2, LV_OBJ_FLAG_CLICKABLE);

            lv_obj_t* line3 = lv_obj_create(sheet);
            lv_obj_set_size(line3, 12, 2);
            lv_obj_set_style_bg_color(line3, lv_color_hex(0x22C55E), 0);
            lv_obj_set_style_border_width(line3, 0, 0);
            lv_obj_align(line3, LV_ALIGN_TOP_LEFT, 2, 14);
            lv_obj_remove_flag(line3, LV_OBJ_FLAG_CLICKABLE);

            // Lápiz flotante dorado
            lv_obj_t* pen = lv_obj_create(editBox);
            lv_obj_set_size(pen, 6, 18);
            lv_obj_set_style_radius(pen, 2, 0);
            lv_obj_set_style_bg_color(pen, lv_color_hex(0xF59E0B), 0);
            lv_obj_set_style_border_color(pen, lv_color_hex(0xFEF08A), 0);
            lv_obj_set_style_border_width(pen, 1, 0);
            lv_obj_set_style_transform_rotation(pen, 450, 0); // 45 grados
            lv_obj_align(pen, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
            lv_obj_remove_flag(pen, LV_OBJ_FLAG_CLICKABLE);

        } else if (app.id == "utilities") {
            // 🧰 Utilidades Tácticas (Llave inglesa y herramienta cruzada)
            lv_obj_t* utilBox = lv_obj_create(btnIcon);
            lv_obj_set_size(utilBox, 34, 34);
            lv_obj_set_style_bg_opa(utilBox, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(utilBox, 0, 0);
            lv_obj_set_style_pad_all(utilBox, 0, 0);
            lv_obj_center(utilBox);
            lv_obj_remove_flag(utilBox, LV_OBJ_FLAG_CLICKABLE);
            DefaultTheme::disableScroll(utilBox);

            // Llave inglesa cruzada (+45°)
            lv_obj_t* bar1 = lv_obj_create(utilBox);
            lv_obj_set_size(bar1, 6, 26);
            lv_obj_set_style_radius(bar1, 2, 0);
            lv_obj_set_style_bg_color(bar1, lv_color_hex(0x0D9488), 0);
            lv_obj_set_style_border_color(bar1, lv_color_hex(0x2DD4BF), 0);
            lv_obj_set_style_border_width(bar1, 1, 0);
            lv_obj_set_style_transform_rotation(bar1, 450, 0);
            lv_obj_center(bar1);
            lv_obj_remove_flag(bar1, LV_OBJ_FLAG_CLICKABLE);

            // Destornillador cruzado (-45°)
            lv_obj_t* bar2 = lv_obj_create(utilBox);
            lv_obj_set_size(bar2, 4, 26);
            lv_obj_set_style_radius(bar2, 2, 0);
            lv_obj_set_style_bg_color(bar2, lv_color_hex(0xF59E0B), 0);
            lv_obj_set_style_border_color(bar2, lv_color_hex(0xFDE68A), 0);
            lv_obj_set_style_border_width(bar2, 1, 0);
            lv_obj_set_style_transform_rotation(bar2, -450, 0);
            lv_obj_center(bar2);
            lv_obj_remove_flag(bar2, LV_OBJ_FLAG_CLICKABLE);

            // Núcleo / Tuerca central
            lv_obj_t* nut = lv_obj_create(utilBox);
            lv_obj_set_size(nut, 10, 10);
            lv_obj_set_style_radius(nut, 5, 0);
            lv_obj_set_style_bg_color(nut, lv_color_hex(0x134E4A), 0);
            lv_obj_set_style_border_color(nut, lv_color_hex(0x2DD4BF), 0);
            lv_obj_set_style_border_width(nut, 2, 0);
            lv_obj_center(nut);
            lv_obj_remove_flag(nut, LV_OBJ_FLAG_CLICKABLE);

        } else {
            // Icono estándar por símbolo LVGL
            lv_obj_t* lblIcon = lv_label_create(btnIcon);
            lv_label_set_text(lblIcon, app.icon.c_str());
            lv_obj_set_style_text_color(lblIcon, lv_color_hex(app.accentColor), 0);
            lv_obj_set_style_text_font(lblIcon, &lv_font_montserrat_24, 0);
            lv_obj_center(lblIcon);
        }

        // 3. Título de la App situado debajo del ícono (como en un OS)
        lv_obj_t* lblTitle = lv_label_create(appItem);
        lv_label_set_text(lblTitle, app.title.c_str());
        lv_obj_set_style_text_color(lblTitle, DefaultTheme::getTextColor(), 0);
        lv_obj_set_style_text_font(lblTitle, &lv_font_montserrat_12, 0);
        lv_obj_set_style_margin_top(lblTitle, 6, 0);
        lv_obj_set_style_text_align(lblTitle, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(lblTitle, itemWidth);

        // Evento de Click pasando el índice de la App
        lv_obj_set_user_data(btnIcon, (void*)(uintptr_t)i);
        lv_obj_add_event_cb(btnIcon, cardClickedEventCb, LV_EVENT_CLICKED, this);

        m_cardObjs.push_back(btnIcon);
    }
}

void DashboardView::cardClickedEventCb(lv_event_t* e) {
    auto* view = static_cast<DashboardView*>(lv_event_get_user_data(e));
    lv_obj_t* target = (lv_obj_t*)lv_event_get_target(e);
    if (!view || !target) return;

    size_t idx = (size_t)(uintptr_t)lv_obj_get_user_data(target);
    if (idx >= view->m_apps.size()) return;
    const auto& app = view->m_apps[idx];

    if (app.isLuapp) {
        UIManager::getInstance().pushView(std::make_shared<LuappView>(app.luappPath, app.title, app.icon));
        return;
    }

    if (app.id == "browser") {
        UIManager::getInstance().pushView(std::make_shared<TlvBrowserView>());
    } else if (app.id == "gallery") {
        UIManager::getInstance().pushView(std::make_shared<GalleryListView>());
    } else if (app.id == "files") {
        UIManager::getInstance().pushView(std::make_shared<FileManagerView>());
    } else if (app.id == "utilities") {
        UIManager::getInstance().pushView(std::make_shared<UtilitiesView>());
    } else if (app.id == "cartridge") {
        UIManager::getInstance().pushView(std::make_shared<CartridgeView>());
    } else if (app.id == "lua") {
        UIManager::getInstance().pushView(std::make_shared<LuaRunnerView>());
    } else if (app.id == "editor") {
        UIManager::getInstance().pushView(std::make_shared<TextEditorView>());
    } else if (app.id == "radio") {
        UIManager::getInstance().pushView(std::make_shared<RadioView>());
    } else if (app.id == "config") {
        UIManager::getInstance().pushView(std::make_shared<ConfigView>());
    } else if (app.id == "recorder") {
        UIManager::getInstance().pushView(std::make_shared<AudioRecorderView>());
    } else if (app.id == "music") {
        UIManager::getInstance().pushView(std::make_shared<MusicPlayerView>());
    } else if (app.id == "flasher") {
        UIManager::getInstance().pushView(std::make_shared<FlasherView>());
    } else if (app.id == "terminal") {
        UIManager::getInstance().pushView(std::make_shared<SerialTerminalView>());
    }
}

void DashboardView::onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) {
    if (!m_container || !lv_obj_is_valid(m_container)) return;
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
}

} // namespace ui
} // namespace cbdos
