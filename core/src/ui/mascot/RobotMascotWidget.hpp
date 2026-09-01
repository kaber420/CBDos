#pragma once

#include "lvgl.h"
#include <cstdint>
#include <string>

namespace cbdos {
namespace ui {

enum class MascotState {
    IDLE,       ///< Flotando suavemente, parpadeo y antena pulsante
    DANCING,    ///< Bailando con música, audífonos y ecualizador activo
    REACTION    ///< Saludo alegre / guiño y chispas al ser tocado o por evento
};

class RobotMascotWidget {
public:
    /**
     * @brief Constructor
     * @param width Ancho del widget en píxeles (default 180)
     * @param height Alto del widget en píxeles (default 180)
     */
    RobotMascotWidget(int32_t width = 180, int32_t height = 180);
    ~RobotMascotWidget();

    /**
     * @brief Crea el widget en el contenedor padre de LVGL.
     * @param parent Objeto LVGL padre donde se alojará el robot.
     * @return lv_obj_t* Contenedor del widget creado.
     */
    lv_obj_t* create(lv_obj_t* parent);

    /**
     * @brief Destruye los recursos gráficos y buffers de PSRAM.
     */
    void destroy();

    /**
     * @brief Cambia el estado del robot.
     * @param state Nuevo estado (IDLE, DANCING, REACTION)
     */
    void setState(MascotState state);

    /**
     * @brief Retorna el estado actual del robot.
     */
    MascotState getState() const { return m_currentState; }

    /**
     * @brief Dispara una reacción temporal (ej. saludo) y vuelve al estado previo tras `durationMs`.
     * @param durationMs Duración de la reacción en milisegundos (default 2500 ms).
     */
    void triggerReaction(uint32_t durationMs = 2500);

    /**
     * @brief Obtiene el contenedor visual LVGL del widget.
     */
    lv_obj_t* getContainer() const { return m_wrapper; }

    /**
     * @brief Habilita o deshabilita la interacción táctil (toque para reaccionar).
     */
    void setInteractive(bool interactive);

private:
    void applyCurrentState();
    static void onClickCb(lv_event_t* e);
    static void onReactionTimerCb(lv_timer_t* timer);

    int32_t m_width;
    int32_t m_height;
    MascotState m_currentState;
    MascotState m_baseState; // Estado al que retorna después de una reacción

    lv_obj_t* m_wrapper;
    lv_obj_t* m_lottieObj;
    lv_draw_buf_t* m_drawBuf;
    lv_timer_t* m_reactionTimer;
    bool m_interactive;
};

} // namespace ui
} // namespace cbdos
