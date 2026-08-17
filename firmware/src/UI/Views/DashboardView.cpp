#include "DashboardView.h"
#include "../UIManager.h"
#include "../Themes/DefaultTheme.h"
#include "../WallpaperManager.h"
#include <cstdio>

HeaderBar* DashboardView::headerBar = nullptr;
DashboardView::CommandCallback DashboardView::commandCb = nullptr;

// ── Registro dinámico de apps del launcher ─────────────────────────
// Agregar una app nueva = añadir una entrada aquí. El grid se adapta solo.
static void launchMeshChat()    { UIManager::getInstance().loadMeshChat(); }
static void launchGallery()     { UIManager::getInstance().loadMediaGallery(); }
static void launchMusic()       { UIManager::getInstance().loadMusicPlayer(); }
static void launchRadio()       { UIManager::getInstance().loadRadioPlayer(); }
static void launchConfig()      { UIManager::getInstance().loadConfigView(); }
static void launchTlvBrowser()  { UIManager::getInstance().loadTlvBrowser(); }
static void launchUtilities()   { UIManager::getInstance().loadUtilities(); }
static void launchFileManager() { UIManager::getInstance().loadFileManager(); }
static void launchDoom()        { UIManager::getInstance().loadDoom(); }
static void launchLua()         { UIManager::getInstance().loadLuaRunner(); }

static const AppDescriptor kApps[] = {
    {1, "Mesh Chat",     LV_SYMBOL_WIFI,      0x00F5D4, launchMeshChat},
    {2, "Galeria",       LV_SYMBOL_IMAGE,     0xFF2E93, launchGallery},
    {3, "Musica SD",     LV_SYMBOL_AUDIO,     0xFFB800, launchMusic},
    {4, "Radio Web",     LV_SYMBOL_WIFI,      0x70E000, launchRadio},
    {5, "Configuracion", LV_SYMBOL_SETTINGS,  0x9D4EDD, launchConfig},
    {6, "Navegador",     LV_SYMBOL_EYE_OPEN,  0x00B4D8, launchTlvBrowser},
    {7, "Utilidades",    LV_SYMBOL_LIST,      0x00F5D4, launchUtilities},
    {8, "Archivos",      LV_SYMBOL_DIRECTORY, 0xF77F00, launchFileManager},
    {9, "Cargador",      LV_SYMBOL_PLAY,      0xE50000, launchDoom},
    {10, "Lua Script",   LV_SYMBOL_FILE,      0x00E676, launchLua},
};

void DashboardView::btn_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) return;

    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
    const AppDescriptor* app = (const AppDescriptor*)lv_obj_get_user_data(btn);
    if (app && app->action) app->action();
}

void DashboardView::refreshState() {
}

void DashboardView::renderAppGrid(lv_obj_t* parent) {
    // Contenedor flexible con scroll vertical. Las apps fluyen en filas de 2.
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_all(parent, 2, 0);
    lv_obj_set_style_pad_column(parent, 12, 0);
    lv_obj_set_style_pad_row(parent, 12, 0);
    // LV_OBJ_FLAG_SCROLLABLE se deja activo de forma predeterminada
    lv_obj_set_scroll_dir(parent, LV_DIR_VER);
    lv_obj_set_scroll_snap_y(parent, LV_SCROLL_SNAP_NONE);

    size_t appCount = sizeof(kApps) / sizeof(kApps[0]);
    for (size_t i = 0; i < appCount; i++) {
        const AppDescriptor& app = kApps[i];

        lv_obj_t * btn = lv_button_create(parent);
        lv_obj_set_width(btn, LV_PCT(48));
        lv_obj_set_height(btn, LV_SIZE_CONTENT);
        DefaultTheme::applyButton(btn, 16);
        DefaultTheme::disableScroll(btn);

        lv_obj_set_user_data(btn, (void*)&app);
        lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(btn, 8, 0);

        // Contenedor para el Icono
        lv_obj_t * iconContainer = lv_obj_create(btn);
        lv_obj_set_size(iconContainer, 50, 50);
        DefaultTheme::applySunkenCard(iconContainer, 25);
        DefaultTheme::disableScroll(iconContainer);
        lv_obj_set_style_pad_all(iconContainer, 0, 0);
        lv_obj_remove_flag(iconContainer, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t * icon = lv_label_create(iconContainer);
        lv_label_set_text(icon, app.icon);
        lv_obj_set_style_text_color(icon, lv_color_hex(app.colorHex), 0);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_24, 0);
        lv_obj_center(icon);

        // Etiqueta de Texto
        lv_obj_t * label = lv_label_create(btn);
        lv_label_set_text(label, app.title);
        lv_obj_set_style_text_color(label, DefaultTheme::getTextColor(), 0);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_margin_top(label, 6, 0);
    }
}

lv_obj_t* DashboardView::create() {
    lv_obj_t* screen = lv_obj_create(NULL);
    DefaultTheme::applyFlatBg(screen);
    DefaultTheme::disableScroll(screen);

    // Aplicar fondo de pantalla (Flash default o SD JPG)
    WallpaperManager::getInstance().applyWallpaper(screen);

    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(screen, 12, 0);
    lv_obj_set_style_pad_row(screen, 10, 0);

    // --- Header Bar ---
    headerBar = HeaderBar::create(screen, "CBDos", false, true);
    HeaderBar::setActiveHeader(headerBar);

    // --- Dashboard Grid (dinámico + scroll) ---
    lv_obj_t * grid = lv_obj_create(screen);
    lv_obj_set_width(grid, lv_pct(100));
    lv_obj_set_flex_grow(grid, 1);

    lv_obj_set_style_bg_opa(grid, 0, 0);
    lv_obj_set_style_border_width(grid, 0, 0);

    renderAppGrid(grid);

    return screen;
}