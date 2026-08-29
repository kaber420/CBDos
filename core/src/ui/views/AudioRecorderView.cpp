#include "AudioRecorderView.hpp"
#include "cbdos/audio.hpp"
#include "cbdos/storage.hpp"
#include "cbdos/time.hpp"
#include "cbdos/system.hpp"
#include "../themes/DefaultTheme.h"
#include <cstdio>
#include <ctime>
#include <algorithm>
#include <sys/stat.h>

namespace cbdos {
namespace ui {

AudioRecorderView::AudioRecorderView()
    : BaseView("Grabadora") {
}

bool AudioRecorderView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;

    // Contenedor principal de la vista
    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    DefaultTheme::applyFlatBg(m_container);
    lv_obj_set_flex_flow(m_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_container, 12, 0);
    lv_obj_set_style_pad_gap(m_container, 10, 0);

    // ==========================================
    // 1. Tarjeta Superior: Control de Grabación & VU
    // ==========================================
    m_topCard = lv_obj_create(m_container);
    lv_obj_set_width(m_topCard, LV_PCT(100));
    lv_obj_set_height(m_topCard, 190);
    DefaultTheme::applyRaisedCard(m_topCard, 14);
    lv_obj_set_flex_flow(m_topCard, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(m_topCard, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(m_topCard, 12, 0);
    DefaultTheme::disableScroll(m_topCard);

    // Estado ("Listo para grabar" / "Grabando...")
    m_statusLabel = lv_label_create(m_topCard);
    lv_label_set_text(m_statusLabel, "Micrófono ES8311 Listo");
    lv_obj_set_style_text_color(m_statusLabel, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(m_statusLabel, &lv_font_montserrat_14, 0);

    // Cronómetro de Grabación
    m_timerLabel = lv_label_create(m_topCard);
    lv_label_set_text(m_timerLabel, "00:00:00");
    lv_obj_set_style_text_color(m_timerLabel, lv_color_hex(0x00F5D4), 0);
    lv_obj_set_style_text_font(m_timerLabel, &lv_font_montserrat_24, 0);
    lv_obj_set_style_margin_top(m_timerLabel, 2, 0);
    lv_obj_set_style_margin_bottom(m_timerLabel, 6, 0);

    // Barra Vúmetro (Reactiva a la amplitud de voz)
    m_vuBar = lv_bar_create(m_topCard);
    lv_obj_set_size(m_vuBar, LV_PCT(85), 8);
    lv_bar_set_range(m_vuBar, 0, 100);
    lv_bar_set_value(m_vuBar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(m_vuBar, lv_color_hex(0x1F2937), LV_PART_MAIN);
    lv_obj_set_style_bg_color(m_vuBar, lv_color_hex(0x00F5D4), LV_PART_INDICATOR);
    lv_obj_set_style_radius(m_vuBar, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(m_vuBar, 4, LV_PART_INDICATOR);

    // Botón Principal de Grabación (Círculo rojo/stop)
    m_recordBtn = lv_button_create(m_topCard);
    lv_obj_set_size(m_recordBtn, 56, 56);
    lv_obj_set_style_radius(m_recordBtn, 28, 0);
    lv_obj_set_style_bg_color(m_recordBtn, lv_color_hex(0xEF4444), 0); // Rojo brillante
    lv_obj_set_style_margin_top(m_recordBtn, 8, 0);
    lv_obj_add_event_cb(m_recordBtn, recordBtnCb, LV_EVENT_CLICKED, this);

    m_recordBtnLabel = lv_label_create(m_recordBtn);
    lv_label_set_text(m_recordBtnLabel, LV_SYMBOL_PLAY); // Ícono inicial
    lv_obj_set_style_text_color(m_recordBtnLabel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(m_recordBtnLabel, &lv_font_montserrat_24, 0);
    lv_obj_center(m_recordBtnLabel);

    // ==========================================
    // 2. Tarjeta Inferior: Historial de Grabaciones
    // ==========================================
    lv_obj_t* listTitle = lv_label_create(m_container);
    lv_label_set_text(listTitle, "NOTAS DE VOZ GUARDADAS (/sdcard/recordings)");
    lv_obj_set_style_text_color(listTitle, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(listTitle, &lv_font_montserrat_12, 0);
    lv_obj_set_style_margin_top(listTitle, 4, 0);

    m_listContainer = lv_obj_create(m_container);
    lv_obj_set_width(m_listContainer, LV_PCT(100));
    lv_obj_set_flex_grow(m_listContainer, 1);
    DefaultTheme::applyRaisedCard(m_listContainer, 12);
    lv_obj_set_flex_flow(m_listContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_listContainer, 8, 0);
    lv_obj_set_style_pad_gap(m_listContainer, 6, 0);

    // Escanear y renderizar lista
    scanRecordings();
    renderHistoryList();

    // Timer de refresco para vúmetro y cronómetro
    m_updateTimer = lv_timer_create(timerCb, 50, this);

    return true;
}

void AudioRecorderView::onDestroy() {
    if (m_updateTimer) {
        lv_timer_delete(m_updateTimer);
        m_updateTimer = nullptr;
    }
    if (cbdos::audio::isRecording()) {
        cbdos::audio::recordStop();
    }
    BaseView::onDestroy();
}

void AudioRecorderView::scanRecordings() {
    m_recordings.clear();

    // Crear carpeta si no existe
    cbdos::storage::makeDir("/sdcard/recordings");

    auto files = cbdos::storage::listDir("/sdcard/recordings");
    for (const auto& f : files) {
        if (!f.isDirectory && f.name.ends_with(".wav")) {
            char sizeStr[32];
            if (f.size < 1024) {
                snprintf(sizeStr, sizeof(sizeStr), "%u B", (unsigned)f.size);
            } else if (f.size < 1024 * 1024) {
                snprintf(sizeStr, sizeof(sizeStr), "%.1f KB", (float)f.size / 1024.0f);
            } else {
                snprintf(sizeStr, sizeof(sizeStr), "%.1f MB", (float)f.size / (1024.0f * 1024.0f));
            }
            m_recordings.push_back({
                f.name,
                "/sdcard/recordings/" + f.name,
                std::string(sizeStr)
            });
        }
    }

    // Ordenar de más reciente a más antiguo
    std::reverse(m_recordings.begin(), m_recordings.end());
}

void AudioRecorderView::renderHistoryList() {
    if (!m_listContainer || !lv_obj_is_valid(m_listContainer)) return;
    lv_obj_clean(m_listContainer);

    if (m_recordings.empty()) {
        lv_obj_t* emptyLabel = lv_label_create(m_listContainer);
        lv_label_set_text(emptyLabel, "No hay grabaciones guardadas aún.\nToca el botón rojo para grabar.");
        lv_obj_set_style_text_color(emptyLabel, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_font(emptyLabel, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_align(emptyLabel, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(emptyLabel);
        return;
    }

    for (size_t i = 0; i < m_recordings.size(); ++i) {
        const auto& item = m_recordings[i];

        lv_obj_t* row = lv_obj_create(m_listContainer);
        lv_obj_set_width(row, LV_PCT(100));
        lv_obj_set_height(row, 48);
        DefaultTheme::applySunkenCard(row, 8);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_hor(row, 10, 0);
        lv_obj_set_style_pad_ver(row, 4, 0);
        DefaultTheme::disableScroll(row);

        // Info izquierda (Nombre + Tamaño)
        lv_obj_t* infoBox = lv_obj_create(row);
        lv_obj_set_flex_grow(infoBox, 1);
        lv_obj_set_height(infoBox, LV_PCT(100));
        lv_obj_set_style_bg_opa(infoBox, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(infoBox, 0, 0);
        lv_obj_set_flex_flow(infoBox, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_all(infoBox, 0, 0);
        DefaultTheme::disableScroll(infoBox);

        lv_obj_t* nameLbl = lv_label_create(infoBox);
        lv_label_set_text(nameLbl, item.name.c_str());
        lv_obj_set_style_text_color(nameLbl, DefaultTheme::getTextColor(), 0);
        lv_obj_set_style_text_font(nameLbl, &lv_font_montserrat_14, 0);
        lv_label_set_long_mode(nameLbl, LV_LABEL_LONG_DOT);

        lv_obj_t* sizeLbl = lv_label_create(infoBox);
        lv_label_set_text(sizeLbl, item.sizeStr.c_str());
        lv_obj_set_style_text_color(sizeLbl, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_font(sizeLbl, &lv_font_montserrat_12, 0);

        // Botón Play
        lv_obj_t* playBtn = lv_button_create(row);
        lv_obj_set_size(playBtn, 36, 36);
        DefaultTheme::applyButton(playBtn, 18);
        lv_obj_set_style_bg_color(playBtn, lv_color_hex(0x10B981), 0); // Verde
        lv_obj_set_user_data(playBtn, (void*)(uintptr_t)i);
        lv_obj_add_event_cb(playBtn, playBtnCb, LV_EVENT_CLICKED, this);

        lv_obj_t* playIcon = lv_label_create(playBtn);
        lv_label_set_text(playIcon, LV_SYMBOL_PLAY);
        lv_obj_set_style_text_color(playIcon, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(playIcon);

        // Botón Delete
        lv_obj_t* delBtn = lv_button_create(row);
        lv_obj_set_size(delBtn, 36, 36);
        DefaultTheme::applyButton(delBtn, 18);
        lv_obj_set_style_bg_color(delBtn, lv_color_hex(0x374151), 0);
        lv_obj_set_user_data(delBtn, (void*)(uintptr_t)i);
        lv_obj_add_event_cb(delBtn, deleteBtnCb, LV_EVENT_CLICKED, this);

        lv_obj_t* delIcon = lv_label_create(delBtn);
        lv_label_set_text(delIcon, LV_SYMBOL_TRASH);
        lv_obj_set_style_text_color(delIcon, lv_color_hex(0xF87171), 0);
        lv_obj_center(delIcon);
    }
}

void AudioRecorderView::toggleRecord() {
    if (cbdos::audio::isRecording()) {
        // Detener grabación
        cbdos::audio::recordStop();
        lv_label_set_text(m_statusLabel, "Grabación guardada");
        lv_label_set_text(m_recordBtnLabel, LV_SYMBOL_PLAY);
        lv_obj_set_style_bg_color(m_recordBtn, lv_color_hex(0xEF4444), 0);
        lv_bar_set_value(m_vuBar, 0, LV_ANIM_OFF);

        // Refrescar historial
        scanRecordings();
        renderHistoryList();
    } else {
        // Diagnóstico previo: verificar montaje de storage
        if (!cbdos::storage::isSdMounted()) {
            cbdos::system::log(cbdos::system::LogLevel::Warn, "AudioRecorder", "MicroSD no montada, intentando montar...");
            cbdos::storage::mountSd();
        }

        cbdos::storage::makeDir("/sdcard/recordings");

        time_t now = ::time(nullptr);
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        char filename[64];
        snprintf(filename, sizeof(filename), "/sdcard/recordings/memo_%02d%02d%02d_%02d%02d%02d.wav",
                 (timeinfo.tm_year + 1900) % 100, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                 timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

        cbdos::audio::RecordConfig cfg;
        cfg.sampleRate = 44100; // 44.1 kHz HD
        cfg.channels = 2;       // 2 canales estéreo
        cfg.bitsPerSample = 16;
        cfg.micGainDb = 24;     // Ganancia calibrada

        if (cbdos::audio::recordStart(filename, cfg)) {
            lv_label_set_text(m_statusLabel, "🔴 Grabando (44.1 kHz HD)...");
            lv_label_set_text(m_recordBtnLabel, LV_SYMBOL_STOP);
            lv_obj_set_style_bg_color(m_recordBtn, lv_color_hex(0x991B1B), 0);
        } else {
            lv_label_set_text(m_statusLabel, "Error iniciando grabación");
        }
    }
}

void AudioRecorderView::playRecording(const std::string& path) {
    if (cbdos::audio::isRecording()) {
        cbdos::audio::recordStop();
    }
    cbdos::audio::stop();
    cbdos::audio::playFile(path.c_str());
    m_currentPlayingPath = path;
    m_isPlaying = true;
    lv_label_set_text(m_statusLabel, "🔊 Reproduciendo nota de voz...");
}

void AudioRecorderView::deleteRecording(const std::string& path) {
    cbdos::storage::deleteFile(path.c_str());
    scanRecordings();
    renderHistoryList();
}

void AudioRecorderView::timerCb(lv_timer_t* timer) {
    auto* view = static_cast<AudioRecorderView*>(lv_timer_get_user_data(timer));
    if (!view) return;

    if (cbdos::audio::isRecording()) {
        uint32_t ms = cbdos::audio::getRecordDurationMs();
        uint32_t totalSec = ms / 1000;
        uint32_t sec = totalSec % 60;
        uint32_t min = (totalSec / 60) % 60;
        uint32_t hrs = totalSec / 3600;

        char buf[32];
        snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", (unsigned long)hrs, (unsigned long)min, (unsigned long)sec);
        lv_label_set_text(view->m_timerLabel, buf);

        // Actualizar Vúmetro
        float level = cbdos::audio::getMicPeakLevel();
        int vuVal = (int)(level * 100.0f);
        if (vuVal > 100) vuVal = 100;
        lv_bar_set_value(view->m_vuBar, vuVal, LV_ANIM_OFF);
    } else {
        // Vúmetro en reposo
        lv_bar_set_value(view->m_vuBar, 0, LV_ANIM_OFF);
    }
}

void AudioRecorderView::recordBtnCb(lv_event_t* e) {
    auto* view = static_cast<AudioRecorderView*>(lv_event_get_user_data(e));
    if (view) {
        view->toggleRecord();
    }
}

void AudioRecorderView::playBtnCb(lv_event_t* e) {
    auto* view = static_cast<AudioRecorderView*>(lv_event_get_user_data(e));
    lv_obj_t* target = (lv_obj_t*)lv_event_get_target(e);
    if (!view || !target) return;

    size_t idx = (size_t)(uintptr_t)lv_obj_get_user_data(target);
    if (idx < view->m_recordings.size()) {
        view->playRecording(view->m_recordings[idx].path);
    }
}

void AudioRecorderView::deleteBtnCb(lv_event_t* e) {
    auto* view = static_cast<AudioRecorderView*>(lv_event_get_user_data(e));
    lv_obj_t* target = (lv_obj_t*)lv_event_get_target(e);
    if (!view || !target) return;

    size_t idx = (size_t)(uintptr_t)lv_obj_get_user_data(target);
    if (idx < view->m_recordings.size()) {
        view->deleteRecording(view->m_recordings[idx].path);
    }
}

void AudioRecorderView::onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) {
    if (!m_container || !lv_obj_is_valid(m_container)) return;
    DefaultTheme::applyFlatBg(m_container);
}

} // namespace ui
} // namespace cbdos
