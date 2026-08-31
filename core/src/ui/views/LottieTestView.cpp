#include "LottieTestView.hpp"
#include "../../assets/lottie_sample.h"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include "cbdos/storage.hpp"
#include "cbdos/system.hpp"
#include "cbdos/display.hpp"
#include <cstdio>
#include <algorithm>

namespace cbdos {
namespace ui {

LottieTestView::LottieTestView()
    : BaseView("Lottie Vector Test"),
      m_lottieObj(nullptr),
      m_fpsLabel(nullptr),
      m_infoLabel(nullptr),
      m_fileLabel(nullptr),
      m_animCard(nullptr),
      m_drawBuf(nullptr),
      m_currentFileIndex(0),
      m_lastTime(0),
      m_frameCount(0) {
}

LottieTestView::~LottieTestView() {
    onDestroy();
}

void LottieTestView::onDestroy() {
    if (m_drawBuf) {
        lv_draw_buf_destroy(m_drawBuf);
        m_drawBuf = nullptr;
    }
    m_lottieObj = nullptr;
    m_fpsLabel = nullptr;
    m_infoLabel = nullptr;
    m_fileLabel = nullptr;
    m_animCard = nullptr;
    m_fileList.clear();
    m_currentJsonData.clear();
    BaseView::onDestroy();
}

void LottieTestView::scanLottieFiles() {
    m_fileList.clear();
    m_fileList.push_back("__EMBEDDED_STAR__");

    // Escanear directorio raíz de la MicroSD (/sdcard)
    if (cbdos::storage::isSdMounted()) {
        auto rootFiles = cbdos::storage::listDir("/sdcard");
        for (const auto& f : rootFiles) {
            if (!f.isDirectory && f.name.length() >= 5) {
                std::string ext = f.name.substr(f.name.length() - 5);
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".json") {
                    m_fileList.push_back("/sdcard/" + f.name);
                }
            }
        }

        // Escanear subcarpeta /sdcard/lottie si existe
        auto lottieFolderFiles = cbdos::storage::listDir("/sdcard/lottie");
        for (const auto& f : lottieFolderFiles) {
            if (!f.isDirectory && f.name.length() >= 5) {
                std::string ext = f.name.substr(f.name.length() - 5);
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".json") {
                    m_fileList.push_back("/sdcard/lottie/" + f.name);
                }
            }
        }
    }
}

bool LottieTestView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;

    // Configurar cabecera
    UIManager::getInstance().getHeaderBar().setTitle("Visor Lottie Vectorial");
    UIManager::getInstance().getHeaderBar().showWifi(false);

    // Contenedor principal
    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_container, 0, 0);
    lv_obj_set_style_radius(m_container, 0, 0);
    lv_obj_set_style_pad_all(m_container, 8, 0);
    lv_obj_set_flex_flow(m_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(m_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    DefaultTheme::disableScroll(m_container);

    // 1. Etiqueta con el archivo activo
    m_fileLabel = lv_label_create(m_container);
    lv_obj_set_style_text_font(m_fileLabel, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(m_fileLabel, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_set_style_margin_top(m_fileLabel, 4, 0);
    lv_label_set_text(m_fileLabel, "Cargando archivos...");

    // 2. Card de renderizado de la animación
    m_animCard = lv_obj_create(m_container);
    lv_obj_set_size(m_animCard, 280, 280);
    DefaultTheme::applySunkenCard(m_animCard, 16);
    lv_obj_set_style_pad_all(m_animCard, 10, 0);
    lv_obj_set_style_border_color(m_animCard, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_set_style_border_width(m_animCard, 1, 0);
    lv_obj_set_style_border_opa(m_animCard, LV_OPA_60, 0);
    lv_obj_set_style_margin_top(m_animCard, 6, 0);
    DefaultTheme::disableScroll(m_animCard);

    // 3. Barra de Control de Navegación (< Anterior | Recargar | Siguiente >)
    lv_obj_t* navRow = lv_obj_create(m_container);
    lv_obj_set_size(navRow, 380, 44);
    lv_obj_set_style_bg_opa(navRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(navRow, 0, 0);
    lv_obj_set_style_pad_all(navRow, 0, 0);
    lv_obj_set_style_margin_top(navRow, 8, 0);
    lv_obj_set_flex_flow(navRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(navRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Botón Anterior
    lv_obj_t* btnPrev = lv_button_create(navRow);
    lv_obj_set_size(btnPrev, 110, 40);
    DefaultTheme::applyButton(btnPrev);
    lv_obj_t* lblPrev = lv_label_create(btnPrev);
    lv_label_set_text(lblPrev, LV_SYMBOL_PREV " Anterior");
    lv_obj_set_style_text_font(lblPrev, &lv_font_montserrat_12, 0);
    lv_obj_center(lblPrev);
    lv_obj_add_event_cb(btnPrev, [](lv_event_t* e) {
        auto* v = static_cast<LottieTestView*>(lv_event_get_user_data(e));
        if (v) v->prevLottie();
    }, LV_EVENT_CLICKED, this);

    // Botón Recargar SD
    lv_obj_t* btnReload = lv_button_create(navRow);
    lv_obj_set_size(btnReload, 110, 40);
    DefaultTheme::applyButton(btnReload);
    lv_obj_t* lblReload = lv_label_create(btnReload);
    lv_label_set_text(lblReload, LV_SYMBOL_REFRESH " Escanear");
    lv_obj_set_style_text_font(lblReload, &lv_font_montserrat_12, 0);
    lv_obj_center(lblReload);
    lv_obj_add_event_cb(btnReload, [](lv_event_t* e) {
        auto* v = static_cast<LottieTestView*>(lv_event_get_user_data(e));
        if (v) v->rescanFiles();
    }, LV_EVENT_CLICKED, this);

    // Botón Siguiente
    lv_obj_t* btnNext = lv_button_create(navRow);
    lv_obj_set_size(btnNext, 110, 40);
    DefaultTheme::applyButton(btnNext);
    lv_obj_t* lblNext = lv_label_create(btnNext);
    lv_label_set_text(lblNext, "Sig " LV_SYMBOL_NEXT);
    lv_obj_set_style_text_font(lblNext, &lv_font_montserrat_12, 0);
    lv_obj_center(lblNext);
    lv_obj_add_event_cb(btnNext, [](lv_event_t* e) {
        auto* v = static_cast<LottieTestView*>(lv_event_get_user_data(e));
        if (v) v->nextLottie();
    }, LV_EVENT_CLICKED, this);

    // 4. Panel de Métricas de Rendimiento
    lv_obj_t* statsCard = lv_obj_create(m_container);
    lv_obj_set_width(statsCard, 380);
    lv_obj_set_height(statsCard, LV_SIZE_CONTENT);
    DefaultTheme::applyRaisedCard(statsCard, 12);
    lv_obj_set_style_pad_all(statsCard, 8, 0);
    lv_obj_set_style_margin_top(statsCard, 8, 0);
    lv_obj_set_flex_flow(statsCard, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(statsCard, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    m_fpsLabel = lv_label_create(statsCard);
    lv_obj_set_style_text_font(m_fpsLabel, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(m_fpsLabel, DefaultTheme::getPrimaryAccent(), 0);
    lv_label_set_text(m_fpsLabel, "FPS: Calculando...");

    m_infoLabel = lv_label_create(statsCard);
    lv_obj_set_style_text_font(m_infoLabel, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_infoLabel, DefaultTheme::getMutedTextColor(), 0);
    lv_label_set_text(m_infoLabel, "Canvas: 240x240 ARGB8888 (PSRAM)");

    // 5. Botón de Salir
    lv_obj_t* btnBack = lv_button_create(m_container);
    lv_obj_set_size(btnBack, 160, 38);
    lv_obj_set_style_margin_top(btnBack, 8, 0);
    DefaultTheme::applyButton(btnBack);
    lv_obj_t* btnLbl = lv_label_create(btnBack);
    lv_label_set_text(btnLbl, LV_SYMBOL_LEFT " Volver");
    lv_obj_set_style_text_font(btnLbl, &lv_font_montserrat_12, 0);
    lv_obj_center(btnLbl);
    lv_obj_add_event_cb(btnBack, [](lv_event_t* e) {
        UIManager::getInstance().popView();
    }, LV_EVENT_CLICKED, nullptr);

    // Escanear y cargar primera animación
    scanLottieFiles();
    loadCurrentLottie();

    m_lastTime = lv_tick_get();
    m_frameCount = 0;
    return true;
}

void LottieTestView::loadCurrentLottie() {
    if (!m_animCard || !lv_obj_is_valid(m_animCard)) return;

    // Destruir objeto y buffer previos de forma limpia
    if (m_lottieObj && lv_obj_is_valid(m_lottieObj)) {
        lv_obj_delete(m_lottieObj);
        m_lottieObj = nullptr;
    }
    if (m_drawBuf) {
        lv_draw_buf_destroy(m_drawBuf);
        m_drawBuf = nullptr;
    }

    if (m_fileList.empty()) {
        if (m_fileLabel) lv_label_set_text(m_fileLabel, "Sin archivos Lottie (.json)");
        return;
    }

    if (m_currentFileIndex >= m_fileList.size()) {
        m_currentFileIndex = 0;
    }

    const std::string& currentPath = m_fileList[m_currentFileIndex];
    char labelBuf[128];

#if LV_USE_LOTTIE
    const int32_t animW = 240;
    const int32_t animH = 240;

    m_lottieObj = lv_lottie_create(m_animCard);
    lv_obj_center(m_lottieObj);

    m_drawBuf = lv_draw_buf_create(animW, animH, LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED, LV_STRIDE_AUTO);
    if (m_drawBuf) {
        lv_lottie_set_draw_buf(m_lottieObj, m_drawBuf);

        if (currentPath == "__EMBEDDED_STAR__") {
            lv_lottie_set_src_data(m_lottieObj, LOTTIE_TEST_JSON, LOTTIE_TEST_JSON_SIZE);
            snprintf(labelBuf, sizeof(labelBuf), "[%zu/%zu] Estrella Demo (Embebida)", 
                     m_currentFileIndex + 1, m_fileList.size());
        } else {
            m_currentJsonData = cbdos::storage::readFile(currentPath.c_str());
            if (!m_currentJsonData.empty()) {
                lv_lottie_set_src_data(m_lottieObj, m_currentJsonData.c_str(), m_currentJsonData.size());
                
                // Extraer solo el nombre de archivo para el título
                std::string fname = currentPath;
                size_t slashPos = fname.find_last_of('/');
                if (slashPos != std::string::npos) fname = fname.substr(slashPos + 1);

                snprintf(labelBuf, sizeof(labelBuf), "[%zu/%zu] %s (%.1f KB)", 
                         m_currentFileIndex + 1, m_fileList.size(), fname.c_str(), 
                         (float)m_currentJsonData.size() / 1024.0f);
            } else {
                snprintf(labelBuf, sizeof(labelBuf), "Error al leer: %s", currentPath.c_str());
            }
        }
    }
#else
    snprintf(labelBuf, sizeof(labelBuf), "LV_USE_LOTTIE no habilitado");
#endif

    if (m_fileLabel && lv_obj_is_valid(m_fileLabel)) {
        lv_label_set_text(m_fileLabel, labelBuf);
    }
}

void LottieTestView::nextLottie() {
    if (m_fileList.empty()) return;
    m_currentFileIndex = (m_currentFileIndex + 1) % m_fileList.size();
    loadCurrentLottie();
}

void LottieTestView::prevLottie() {
    if (m_fileList.empty()) return;
    if (m_currentFileIndex == 0) {
        m_currentFileIndex = m_fileList.size() - 1;
    } else {
        m_currentFileIndex--;
    }
    loadCurrentLottie();
}

void LottieTestView::rescanFiles() {
    scanLottieFiles();
    m_currentFileIndex = 0;
    loadCurrentLottie();
}

void LottieTestView::onUpdate() {
    m_frameCount++;
    uint32_t now = lv_tick_get();
    uint32_t elapsed = now - m_lastTime;

    if (elapsed >= 1000) {
        float fps = (float)m_frameCount * 1000.0f / (float)elapsed;
        m_frameCount = 0;
        m_lastTime = now;

        if (m_fpsLabel && lv_obj_is_valid(m_fpsLabel)) {
            char buf[64];
            snprintf(buf, sizeof(buf), "Rendimiento: %.1f FPS (ThorVG HW)", fps);
            lv_label_set_text(m_fpsLabel, buf);
        }
    }
}

void LottieTestView::onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) {
    (void)theme;
    (void)palette;
    if (!m_container || !lv_obj_is_valid(m_container)) return;
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
}

} // namespace ui
} // namespace cbdos
