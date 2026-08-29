#include "AnimatedWallpaper.hpp"
#include "cbdos/log.hpp"
#include <cmath>
#include <cstdlib>

namespace cbdos {
namespace ui {

static const char* TAG = "AnimatedWallpaper";

AnimatedWallpaper::AnimatedWallpaper()
    : m_canvasObj(nullptr),
      m_timer(nullptr),
      m_running(false),
      m_tick(0) {
}

AnimatedWallpaper::~AnimatedWallpaper() {
    destroy();
}

void AnimatedWallpaper::init(lv_obj_t* parent) {
    if (!parent) return;

    if (m_canvasObj) {
        destroy();
    }

    // Crear widget transparente de pantalla completa
    m_canvasObj = lv_obj_create(parent);
    lv_obj_remove_style_all(m_canvasObj);
    lv_obj_set_size(m_canvasObj, LV_PCT(100), LV_PCT(100));
    lv_obj_set_align(m_canvasObj, LV_ALIGN_CENTER);
    lv_obj_remove_flag(m_canvasObj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(m_canvasObj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_to_index(m_canvasObj, 0); // Ponerlo en el fondo de la jerarquía de z-index

    // Registrar callback de dibujo vectorial LVGL 9.5
    lv_obj_add_event_cb(m_canvasObj, drawCallback, LV_EVENT_DRAW_MAIN, this);

    // Inicializar partículas vectoriales aleatorias
    for (int i = 0; i < PARTICLE_COUNT; i++) {
        m_particles[i].x = (float)(rand() % 480);
        m_particles[i].y = (float)(rand() % 800);
        m_particles[i].vx = ((rand() % 100) / 100.0f - 0.5f) * 1.2f;
        m_particles[i].vy = ((rand() % 100) / 100.0f - 0.5f) * 1.2f;
        m_particles[i].radius = 4.0f + (rand() % 12);
        m_particles[i].alpha = 0.3f + ((rand() % 50) / 100.0f);
        m_particles[i].pulseSpeed = 0.02f + ((rand() % 5) / 100.0f);
        m_particles[i].phase = (float)(rand() % 360);
    }

    CBD_LOG_I(TAG, "AnimatedWallpaper inicializado exitosamente");
    start();
}

void AnimatedWallpaper::start() {
    if (m_running) return;

    if (!m_timer) {
        // Temporizador de 20 ms (~50 FPS fluído)
        m_timer = lv_timer_create(timerCallback, 20, this);
    } else {
        lv_timer_resume(m_timer);
    }
    m_running = true;
}

void AnimatedWallpaper::stop() {
    if (!m_running) return;

    if (m_timer) {
        lv_timer_pause(m_timer);
    }
    m_running = false;
}

void AnimatedWallpaper::destroy() {
    stop();
    if (m_timer) {
        lv_timer_delete(m_timer);
        m_timer = nullptr;
    }
    if (m_canvasObj) {
        if (lv_obj_is_valid(m_canvasObj)) {
            lv_obj_delete(m_canvasObj);
        }
        m_canvasObj = nullptr;
    }
}

void AnimatedWallpaper::timerCallback(lv_timer_t* timer) {
    auto* self = static_cast<AnimatedWallpaper*>(lv_timer_get_user_data(timer));
    if (!self || !self->m_canvasObj || !lv_obj_is_valid(self->m_canvasObj)) return;

    self->m_tick++;
    self->updatePhysics();
    lv_obj_invalidate(self->m_canvasObj);
}

void AnimatedWallpaper::updatePhysics() {
    int width = lv_obj_get_width(m_canvasObj);
    int height = lv_obj_get_height(m_canvasObj);
    if (width <= 0) width = 480;
    if (height <= 0) height = 800;

    for (int i = 0; i < PARTICLE_COUNT; i++) {
        m_particles[i].x += m_particles[i].vx;
        m_particles[i].y += m_particles[i].vy;
        m_particles[i].phase += m_particles[i].pulseSpeed;

        // Rebote en bordes
        if (m_particles[i].x < 0 || m_particles[i].x > width) m_particles[i].vx *= -1.0f;
        if (m_particles[i].y < 0 || m_particles[i].y > height) m_particles[i].vy *= -1.0f;
    }
}

void AnimatedWallpaper::drawCallback(lv_event_t* e) {
    auto* self = static_cast<AnimatedWallpaper*>(lv_event_get_user_data(e));
    if (!self) return;

    lv_layer_t* layer = lv_event_get_layer(e);
    if (!layer) return;

    self->draw(layer);
}

void AnimatedWallpaper::draw(lv_layer_t* layer) {
    int width = lv_obj_get_width(m_canvasObj);
    int height = lv_obj_get_height(m_canvasObj);
    if (width <= 0) width = 480;
    if (height <= 0) height = 800;

    // 1. Dibujar Fondo Gradiente Sombreado
    lv_draw_rect_dsc_t bg_dsc;
    lv_draw_rect_dsc_init(&bg_dsc);
    bg_dsc.bg_color = lv_color_hex(0x0a0d1a);       // Azul noche profundo
    bg_dsc.bg_grad.dir = LV_GRAD_DIR_VER;
    bg_dsc.bg_grad.stops[0].color = lv_color_hex(0x070914);
    bg_dsc.bg_grad.stops[0].frac = 0;
    bg_dsc.bg_grad.stops[1].color = lv_color_hex(0x181f3a);
    bg_dsc.bg_grad.stops[1].frac = 255;
    bg_dsc.bg_grad.stops_count = 2;
    bg_dsc.radius = 0;

    lv_area_t full_area = {0, 0, (int16_t)(width - 1), (int16_t)(height - 1)};
    lv_draw_rect(layer, &bg_dsc, &full_area);

    // 2. Dibujar Ondas Senoidales Vectoriales en Movimiento
    float timeVal = m_tick * 0.03f;
    
    // Onda 1 (Cyan Neón)
    lv_draw_line_dsc_t line_dsc1;
    lv_draw_line_dsc_init(&line_dsc1);
    line_dsc1.color = lv_color_hex(0x00e5ff);
    line_dsc1.width = 2;
    line_dsc1.opa = LV_OPA_60;

    // Onda 2 (Violeta Neón)
    lv_draw_line_dsc_t line_dsc2;
    lv_draw_line_dsc_init(&line_dsc2);
    line_dsc2.color = lv_color_hex(0x9d4edd);
    line_dsc2.width = 3;
    line_dsc2.opa = LV_OPA_50;

    int step = 16;
    for (int x = 0; x < width; x += step) {
        int nextX = x + step;
        if (nextX >= width) nextX = width - 1;

        // Ecuaciones matemáticas de onda
        float y1_curr = (height * 0.45f) + sinf(x * 0.015f + timeVal) * 45.0f + cosf(x * 0.008f - timeVal * 0.5f) * 20.0f;
        float y1_next = (height * 0.45f) + sinf(nextX * 0.015f + timeVal) * 45.0f + cosf(nextX * 0.008f - timeVal * 0.5f) * 20.0f;

        float y2_curr = (height * 0.60f) + cosf(x * 0.012f - timeVal * 0.8f) * 55.0f + sinf(x * 0.005f + timeVal * 0.4f) * 30.0f;
        float y2_next = (height * 0.60f) + cosf(nextX * 0.012f - timeVal * 0.8f) * 55.0f + sinf(nextX * 0.005f + timeVal * 0.4f) * 30.0f;

        line_dsc1.p1.x = x; line_dsc1.p1.y = (int32_t)y1_curr;
        line_dsc1.p2.x = nextX; line_dsc1.p2.y = (int32_t)y1_next;
        lv_draw_line(layer, &line_dsc1);

        line_dsc2.p1.x = x; line_dsc2.p1.y = (int32_t)y2_curr;
        line_dsc2.p2.x = nextX; line_dsc2.p2.y = (int32_t)y2_next;
        lv_draw_line(layer, &line_dsc2);
    }

    // 3. Dibujar Líneas de Constelación entre Nodos Cercanos
    lv_draw_line_dsc_t conn_dsc;
    lv_draw_line_dsc_init(&conn_dsc);
    conn_dsc.color = lv_color_hex(0x4cc9f0);
    conn_dsc.width = 1;

    for (int i = 0; i < PARTICLE_COUNT; i++) {
        for (int j = i + 1; j < PARTICLE_COUNT; j++) {
            float dx = m_particles[i].x - m_particles[j].x;
            float dy = m_particles[i].y - m_particles[j].y;
            float distSq = dx * dx + dy * dy;

            if (distSq < 130.0f * 130.0f) {
                float dist = sqrtf(distSq);
                float alphaRatio = 1.0f - (dist / 130.0f);
                conn_dsc.opa = (lv_opa_t)(alphaRatio * 90);

                conn_dsc.p1.x = (int32_t)m_particles[i].x;
                conn_dsc.p1.y = (int32_t)m_particles[i].y;
                conn_dsc.p2.x = (int32_t)m_particles[j].x;
                conn_dsc.p2.y = (int32_t)m_particles[j].y;
                lv_draw_line(layer, &conn_dsc);
            }
        }
    }

    // 4. Dibujar Orbes / Partículas Vectoriales Flotantes
    lv_draw_rect_dsc_t orb_dsc;
    for (int i = 0; i < PARTICLE_COUNT; i++) {
        lv_draw_rect_dsc_init(&orb_dsc);
        
        float pulse = sinf(m_particles[i].phase) * 0.3f + 0.7f;
        float currentRadius = m_particles[i].radius * pulse;

        orb_dsc.radius = LV_RADIUS_CIRCLE; // Círculo perfecto
        orb_dsc.bg_color = (i % 2 == 0) ? lv_color_hex(0x00f5d4) : lv_color_hex(0x7209b7);
        orb_dsc.bg_opa = (lv_opa_t)(m_particles[i].alpha * pulse * 255.0f);
        
        // Aura exterior brillante
        orb_dsc.outline_width = 2;
        orb_dsc.outline_color = lv_color_hex(0xffffff);
        orb_dsc.outline_opa = (lv_opa_t)(orb_dsc.bg_opa * 0.4f);

        lv_area_t orb_area = {
            (int16_t)(m_particles[i].x - currentRadius),
            (int16_t)(m_particles[i].y - currentRadius),
            (int16_t)(m_particles[i].x + currentRadius),
            (int16_t)(m_particles[i].y + currentRadius)
        };
        lv_draw_rect(layer, &orb_dsc, &orb_area);
    }
}

} // namespace ui
} // namespace cbdos
