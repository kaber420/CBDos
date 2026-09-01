#include "RobotMascotWidget.hpp"
#include "../../../assets/mascot_robot_assets.h"
#include "../themes/DefaultTheme.h"
#include <cstdio>
#include <cstring>

namespace cbdos {
namespace ui {

RobotMascotWidget::RobotMascotWidget(int32_t width, int32_t height)
    : m_width(width),
      m_height(height),
      m_currentState(MascotState::IDLE),
      m_baseState(MascotState::IDLE),
      m_wrapper(nullptr),
      m_lottieObj(nullptr),
      m_drawBuf(nullptr),
      m_reactionTimer(nullptr),
      m_interactive(true) {
}

RobotMascotWidget::~RobotMascotWidget() {
    destroy();
}

void RobotMascotWidget::destroy() {
    if (m_reactionTimer) {
        lv_timer_delete(m_reactionTimer);
        m_reactionTimer = nullptr;
    }
    if (m_lottieObj && lv_obj_is_valid(m_lottieObj)) {
        lv_obj_delete(m_lottieObj);
        m_lottieObj = nullptr;
    }
    if (m_drawBuf) {
        lv_draw_buf_destroy(m_drawBuf);
        m_drawBuf = nullptr;
    }
    if (m_wrapper && lv_obj_is_valid(m_wrapper)) {
        lv_obj_delete(m_wrapper);
        m_wrapper = nullptr;
    }
}

lv_obj_t* RobotMascotWidget::create(lv_obj_t* parent) {
    if (!parent) return nullptr;

    destroy();

    // Contenedor wrapper para permitir toques, centrado y recorte limpio
    m_wrapper = lv_obj_create(parent);
    lv_obj_set_size(m_wrapper, m_width, m_height);
    lv_obj_set_style_bg_opa(m_wrapper, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_wrapper, 0, 0);
    lv_obj_set_style_pad_all(m_wrapper, 0, 0);
    lv_obj_set_style_radius(m_wrapper, 0, 0);
    DefaultTheme::disableScroll(m_wrapper);

    if (m_interactive) {
        lv_obj_add_flag(m_wrapper, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(m_wrapper, onClickCb, LV_EVENT_CLICKED, this);
    }

    applyCurrentState();
    return m_wrapper;
}

void RobotMascotWidget::setInteractive(bool interactive) {
    m_interactive = interactive;
    if (m_wrapper && lv_obj_is_valid(m_wrapper)) {
        if (m_interactive) {
            lv_obj_add_flag(m_wrapper, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(m_wrapper, onClickCb, LV_EVENT_CLICKED, this);
        } else {
            lv_obj_remove_flag(m_wrapper, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_remove_event_cb(m_wrapper, onClickCb);
        }
    }
}

void RobotMascotWidget::setState(MascotState state) {
    if (m_reactionTimer) {
        lv_timer_delete(m_reactionTimer);
        m_reactionTimer = nullptr;
    }
    m_currentState = state;
    m_baseState = state;
    applyCurrentState();
}

void RobotMascotWidget::triggerReaction(uint32_t durationMs) {
    if (m_reactionTimer) {
        lv_timer_delete(m_reactionTimer);
        m_reactionTimer = nullptr;
    }

    // Si no estábamos en REACTION, guardar el estado base (IDLE o DANCING)
    if (m_currentState != MascotState::REACTION) {
        m_baseState = m_currentState;
    }

    m_currentState = MascotState::REACTION;
    applyCurrentState();

    m_reactionTimer = lv_timer_create(onReactionTimerCb, durationMs, this);
    lv_timer_set_repeat_count(m_reactionTimer, 1);
}

void RobotMascotWidget::applyCurrentState() {
    if (!m_wrapper || !lv_obj_is_valid(m_wrapper)) return;

    if (m_lottieObj && lv_obj_is_valid(m_lottieObj)) {
        lv_obj_delete(m_lottieObj);
        m_lottieObj = nullptr;
    }
    if (m_drawBuf) {
        lv_draw_buf_destroy(m_drawBuf);
        m_drawBuf = nullptr;
    }

#if LV_USE_LOTTIE
    m_lottieObj = lv_lottie_create(m_wrapper);
    lv_obj_center(m_lottieObj);

    m_drawBuf = lv_draw_buf_create(m_width, m_height, LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED, LV_STRIDE_AUTO);
    if (m_drawBuf) {
        lv_lottie_set_draw_buf(m_lottieObj, m_drawBuf);

        const char* jsonSrc = cbdos::assets::LOTTIE_BITBOT_IDLE_JSON;
        size_t jsonSize = cbdos::assets::LOTTIE_BITBOT_IDLE_JSON_SIZE;

        switch (m_currentState) {
            case MascotState::DANCING:
                jsonSrc = cbdos::assets::LOTTIE_BITBOT_DANCE_JSON;
                jsonSize = cbdos::assets::LOTTIE_BITBOT_DANCE_JSON_SIZE;
                break;
            case MascotState::REACTION:
                jsonSrc = cbdos::assets::LOTTIE_BITBOT_WAVE_JSON;
                jsonSize = cbdos::assets::LOTTIE_BITBOT_WAVE_JSON_SIZE;
                break;
            case MascotState::IDLE:
            default:
                jsonSrc = cbdos::assets::LOTTIE_BITBOT_IDLE_JSON;
                jsonSize = cbdos::assets::LOTTIE_BITBOT_IDLE_JSON_SIZE;
                break;
        }

        lv_lottie_set_src_data(m_lottieObj, jsonSrc, jsonSize);
    }
#else
    // Fallback simple si Lottie no estuviese activo en la build
    lv_obj_t* label = lv_label_create(m_wrapper);
    lv_obj_center(label);
    if (m_currentState == MascotState::DANCING) {
        lv_label_set_text(label, "[BitBot: ♫ 8-) ♫]");
    } else if (m_currentState == MascotState::REACTION) {
        lv_label_set_text(label, "[BitBot: \\o/ ^_-]");
    } else {
        lv_label_set_text(label, "[BitBot: •‿•]");
    }
#endif
}

void RobotMascotWidget::onClickCb(lv_event_t* e) {
    RobotMascotWidget* self = static_cast<RobotMascotWidget*>(lv_event_get_user_data(e));
    if (self) {
        self->triggerReaction(2500);
    }
}

void RobotMascotWidget::onReactionTimerCb(lv_timer_t* timer) {
    RobotMascotWidget* self = static_cast<RobotMascotWidget*>(lv_timer_get_user_data(timer));
    if (self) {
        self->m_reactionTimer = nullptr;
        self->m_currentState = self->m_baseState;
        self->applyCurrentState();
    }
}

} // namespace ui
} // namespace cbdos
