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
      m_tick(0),
      m_style(Style::Constellation) {
}

AnimatedWallpaper::~AnimatedWallpaper() {
    destroy();
}

void AnimatedWallpaper::init(lv_obj_t* parent) {
    if (!parent) return;

    if (m_canvasObj) {
        destroy();
    }

    // Crear widget de pantalla completa con fondo plano oscuro
    m_canvasObj = lv_obj_create(parent);
    lv_obj_remove_style_all(m_canvasObj);
    lv_obj_set_size(m_canvasObj, LV_PCT(100), LV_PCT(100));
    lv_obj_set_align(m_canvasObj, LV_ALIGN_CENTER);
    lv_obj_set_style_bg_color(m_canvasObj, lv_color_hex(0x0a0d1a), 0);
    lv_obj_set_style_bg_opa(m_canvasObj, LV_OPA_COVER, 0);
    lv_obj_remove_flag(m_canvasObj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(m_canvasObj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_to_index(m_canvasObj, 0); // Ponerlo en el fondo de la jerarquía de z-index

    // Registrar callback de dibujo vectorial LVGL 9.5
    lv_obj_add_event_cb(m_canvasObj, drawCallback, LV_EVENT_DRAW_MAIN, this);

    // Inicializar partículas vectoriales aleatorias (movimiento dinámico y fluido a 60 FPS)
    for (int i = 0; i < PARTICLE_COUNT; i++) {
        m_particles[i].x = (float)(rand() % 480);
        m_particles[i].y = (float)(rand() % 800);
        m_particles[i].vx = ((rand() % 100) / 100.0f - 0.5f) * 1.0f;
        m_particles[i].vy = ((rand() % 100) / 100.0f - 0.5f) * 1.0f;
        m_particles[i].radius = 4.0f + (rand() % 10);
        m_particles[i].alpha = 0.35f + ((rand() % 45) / 100.0f);
        m_particles[i].pulseSpeed = 0.02f + ((rand() % 4) / 100.0f);
        m_particles[i].phase = (float)(rand() % 360);
    }

    CBD_LOG_I(TAG, "AnimatedWallpaper inicializado exitosamente");
    start();
}

void AnimatedWallpaper::start() {
    if (m_running) return;

    if (!m_timer) {
        // Temporizador de 16 ms (60 FPS alineado con VSYNC MIPI-DSI)
        m_timer = lv_timer_create(timerCallback, 16, this);
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

    // Detectar punto de toque activo para interactividad táctil
    bool isTouchPressed = false;
    lv_point_t touchPoint = {0, 0};
    lv_indev_t* indev = lv_indev_get_next(NULL);
    if (indev) {
        lv_indev_state_t state = lv_indev_get_state(indev);
        if (state == LV_INDEV_STATE_PRESSED) {
            isTouchPressed = true;
            lv_indev_get_point(indev, &touchPoint);
        }
    }

    for (int i = 0; i < PARTICLE_COUNT; i++) {
        if (m_style == Style::TouchSwarm && isTouchPressed) {
            // Atracción magnética de partículas hacia el toque del dedo
            float dx = touchPoint.x - m_particles[i].x;
            float dy = touchPoint.y - m_particles[i].y;
            float distSq = dx * dx + dy * dy;
            if (distSq > 1.0f) {
                float dist = sqrtf(distSq);
                float force = 1.2f / (dist + 12.0f);
                m_particles[i].vx += (dx / dist) * force * 18.0f;
                m_particles[i].vy += (dy / dist) * force * 18.0f;
            }
            m_particles[i].vx *= 0.93f;
            m_particles[i].vy *= 0.93f;
        }

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

    if (m_style == Style::Waves) {
        // Estilo: Ondas Senoidales Vectoriales en Movimiento
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

        int step = 24;
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
    } else if (m_style == Style::ComicDrive) {
        // Estilo: Carrito Cómic 2D en Parallax Infinito (100% Vectorial)
        // 1. Cielo Nocturno y Luna Neón
        lv_draw_rect_dsc_t moon_dsc;
        lv_draw_rect_dsc_init(&moon_dsc);
        moon_dsc.radius = LV_RADIUS_CIRCLE;
        moon_dsc.bg_color = lv_color_hex(0xfff3b0);
        moon_dsc.bg_opa = LV_OPA_COVER;
        
        int32_t moonX = (int32_t)(width * 0.80f);
        int32_t moonY = (int32_t)(height * 0.14f);
        lv_area_t moon_area = { (int16_t)(moonX - 22), (int16_t)(moonY - 22), (int16_t)(moonX + 22), (int16_t)(moonY + 22) };
        lv_draw_rect(layer, &moon_dsc, &moon_area);

        // Halo de la luna
        moon_dsc.bg_opa = LV_OPA_30;
        lv_area_t halo_area = { (int16_t)(moonX - 32), (int16_t)(moonY - 32), (int16_t)(moonX + 32), (int16_t)(moonY + 32) };
        lv_draw_rect(layer, &moon_dsc, &halo_area);

        // 2. Estrellas brillantes en el cielo
        lv_draw_rect_dsc_t star_dsc;
        for (int i = 0; i < 10; i++) {
            lv_draw_rect_dsc_init(&star_dsc);
            star_dsc.radius = LV_RADIUS_CIRCLE;
            star_dsc.bg_color = lv_color_hex(0xffffff);
            float starPulse = sinf(m_tick * 0.08f + i * 1.5f) * 0.4f + 0.6f;
            star_dsc.bg_opa = (lv_opa_t)(starPulse * 200.0f);
            
            int32_t sx = (i * 47 + 23) % width;
            int32_t sy = (i * 29 + 15) % (int32_t)(height * 0.35f);
            lv_area_t star_area = { (int16_t)(sx - 2), (int16_t)(sy - 2), (int16_t)(sx + 2), (int16_t)(sy + 2) };
            lv_draw_rect(layer, &star_dsc, &star_area);
        }

        // 3. Montañas lejanas en Parallax Lento (Velocidad: 0.4 px/frame)
        lv_draw_rect_dsc_t mountain_dsc;
        lv_draw_rect_dsc_init(&mountain_dsc);
        mountain_dsc.bg_color = lv_color_hex(0x1f1435);
        mountain_dsc.bg_opa = LV_OPA_COVER;
        mountain_dsc.radius = 16;

        int horizonY = (int)(height * 0.68f);
        float mountainOffset = fmodf(m_tick * 0.4f, 160.0f);
        for (int i = -1; i < 4; i++) {
            int32_t mx = (int32_t)(i * 150 - mountainOffset);
            int32_t my = horizonY - 90;
            lv_area_t m_area = { (int16_t)mx, (int16_t)my, (int16_t)(mx + 140), (int16_t)horizonY };
            lv_draw_rect(layer, &mountain_dsc, &m_area);
        }

        // 4. Objetos del Camino (Casitas y Cactus en Parallax Medio - 1.8 px/frame)
        float midOffset = fmodf(m_tick * 1.8f, (float)(width + 120));
        
        // Cactus 1 (Verde Neón)
        int32_t cactusX = (int32_t)(width + 60 - midOffset);
        if (cactusX > -40 && cactusX < width + 40) {
            lv_draw_rect_dsc_t cactus_dsc;
            lv_draw_rect_dsc_init(&cactus_dsc);
            cactus_dsc.bg_color = lv_color_hex(0x2d6a4f);
            cactus_dsc.bg_opa = LV_OPA_COVER;
            cactus_dsc.radius = 4;
            
            // Tallo central
            lv_area_t c_main = { (int16_t)cactusX, (int16_t)(horizonY - 50), (int16_t)(cactusX + 12), (int16_t)horizonY };
            lv_draw_rect(layer, &cactus_dsc, &c_main);
            // Brazo izquierdo
            lv_area_t c_arm1 = { (int16_t)(cactusX - 10), (int16_t)(horizonY - 38), (int16_t)cactusX, (int16_t)(horizonY - 26) };
            lv_draw_rect(layer, &cactus_dsc, &c_arm1);
            // Brazo derecho
            lv_area_t c_arm2 = { (int16_t)(cactusX + 12), (int16_t)(horizonY - 44), (int16_t)(cactusX + 22), (int16_t)(horizonY - 30) };
            lv_draw_rect(layer, &cactus_dsc, &c_arm2);
        }

        // Casita Cómic 1
        float houseOffset = fmodf(m_tick * 1.8f + 240.0f, (float)(width + 120));
        int32_t houseX = (int32_t)(width + 60 - houseOffset);
        if (houseX > -60 && houseX < width + 60) {
            lv_draw_rect_dsc_t house_dsc;
            lv_draw_rect_dsc_init(&house_dsc);
            house_dsc.bg_color = lv_color_hex(0x3a2e39);
            house_dsc.bg_opa = LV_OPA_COVER;
            house_dsc.radius = 4;
            
            // Cuerpo de la casa
            lv_area_t h_body = { (int16_t)houseX, (int16_t)(horizonY - 45), (int16_t)(houseX + 48), (int16_t)horizonY };
            lv_draw_rect(layer, &house_dsc, &h_body);

            // Techo de la casa
            house_dsc.bg_color = lv_color_hex(0x6d2e46);
            lv_area_t h_roof = { (int16_t)(houseX - 4), (int16_t)(horizonY - 58), (int16_t)(houseX + 52), (int16_t)(horizonY - 43) };
            lv_draw_rect(layer, &house_dsc, &h_roof);

            // Ventana iluminada
            house_dsc.bg_color = lv_color_hex(0xffd166);
            lv_area_t h_win = { (int16_t)(houseX + 16), (int16_t)(horizonY - 35), (int16_t)(houseX + 30), (int16_t)(horizonY - 22) };
            lv_draw_rect(layer, &house_dsc, &h_win);
        }

        // 5. Carretera Asfaltada y Líneas de Línea Central (Parallax Rápido - 7 px/frame)
        lv_draw_rect_dsc_t road_dsc;
        lv_draw_rect_dsc_init(&road_dsc);
        road_dsc.bg_color = lv_color_hex(0x16161d);
        road_dsc.bg_opa = LV_OPA_COVER;
        lv_area_t road_area = { 0, (int16_t)horizonY, (int16_t)width, (int16_t)height };
        lv_draw_rect(layer, &road_dsc, &road_area);

        // Borde superior de la carretera
        lv_draw_line_dsc_t edge_dsc;
        lv_draw_line_dsc_init(&edge_dsc);
        edge_dsc.color = lv_color_hex(0x444455);
        edge_dsc.width = 2;
        edge_dsc.p1.x = 0; edge_dsc.p1.y = horizonY;
        edge_dsc.p2.x = width; edge_dsc.p2.y = horizonY;
        lv_draw_line(layer, &edge_dsc);

        // Líneas discontinuas de carril
        float lineOffset = fmodf(m_tick * 7.0f, 60.0f);
        lv_draw_rect_dsc_t dash_dsc;
        lv_draw_rect_dsc_init(&dash_dsc);
        dash_dsc.bg_color = lv_color_hex(0xffd166);
        dash_dsc.bg_opa = LV_OPA_COVER;
        int dashY = horizonY + 35;
        for (int lx = -60; lx < width + 60; lx += 60) {
            int32_t dashX = (int32_t)(lx - lineOffset);
            lv_area_t d_area = { (int16_t)dashX, (int16_t)dashY, (int16_t)(dashX + 32), (int16_t)(dashY + 5) };
            lv_draw_rect(layer, &dash_dsc, &d_area);
        }

        // 6. El Carrito Cómic 2D
        float bounce = sinf(m_tick * 0.35f) * 1.5f;
        int32_t carX = (int32_t)(width * 0.26f);
        int32_t carY = (int32_t)(horizonY + 12 + bounce);

        // Partículas de humo saliendo del escape (atrás a la izquierda)
        lv_draw_rect_dsc_t smoke_dsc;
        for (int s = 0; s < 3; s++) {
            lv_draw_rect_dsc_init(&smoke_dsc);
            smoke_dsc.radius = LV_RADIUS_CIRCLE;
            smoke_dsc.bg_color = lv_color_hex(0xaaaaaa);
            float sPhase = fmodf(m_tick * 0.15f + s * 1.0f, 3.0f);
            smoke_dsc.bg_opa = (lv_opa_t)((1.0f - (sPhase / 3.0f)) * 160.0f);
            
            int32_t smX = (int32_t)(carX - 10 - sPhase * 18.0f);
            int32_t smY = (int32_t)(carY + 12 - sPhase * 4.0f);
            float smR = 3.0f + sPhase * 3.0f;
            lv_area_t sm_area = { (int16_t)(smX - smR), (int16_t)(smY - smR), (int16_t)(smX + smR), (int16_t)(smY + smR) };
            lv_draw_rect(layer, &smoke_dsc, &sm_area);
        }

        // Cono de Luz / Faro Delantero (Proyección translúcida)
        lv_draw_line_dsc_t beam_dsc;
        lv_draw_line_dsc_init(&beam_dsc);
        beam_dsc.color = lv_color_hex(0xffea00);
        beam_dsc.width = 10;
        beam_dsc.opa = LV_OPA_30;
        beam_dsc.p1.x = carX + 75; beam_dsc.p1.y = carY + 14;
        beam_dsc.p2.x = carX + 160; beam_dsc.p2.y = carY + 24;
        lv_draw_line(layer, &beam_dsc);

        // Chasis y Silueta del Vocho / Beetle Retro (100% Vectorial)
        lv_draw_rect_dsc_t body_dsc;
        lv_draw_rect_dsc_init(&body_dsc);

        // 1. Capó Trasero Curvo del Vocho
        body_dsc.bg_color = lv_color_hex(0xff9f1c);
        body_dsc.bg_opa = LV_OPA_COVER;
        body_dsc.radius = 10;
        lv_area_t rear_hood = { (int16_t)(carX - 6), (int16_t)(carY + 2), (int16_t)(carX + 26), (int16_t)(carY + 22) };
        lv_draw_rect(layer, &body_dsc, &rear_hood);

        // 2. Capó Delantero Inclinado Redondeado
        lv_area_t front_hood = { (int16_t)(carX + 48), (int16_t)(carY + 4), (int16_t)(carX + 76), (int16_t)(carY + 22) };
        lv_draw_rect(layer, &body_dsc, &front_hood);

        // 3. Techo Curvo Redondeado Emblemático (Dome Body)
        body_dsc.radius = 18;
        lv_area_t roof_area = { (int16_t)(carX + 14), (int16_t)(carY - 14), (int16_t)(carX + 60), (int16_t)(carY + 14) };
        lv_draw_rect(layer, &body_dsc, &roof_area);

        // 4. Guardabarros Abombados (Fenders de Rueda Tradicionales)
        body_dsc.bg_color = lv_color_hex(0xe056fd); // Tono de contraste cómic
        body_dsc.radius = 8;
        // Guardabarros Trasero
        lv_area_t f1_area = { (int16_t)(carX + 2), (int16_t)(carY + 12), (int16_t)(carX + 26), (int16_t)(carY + 24) };
        lv_draw_rect(layer, &body_dsc, &f1_area);
        // Guardabarros Delantero
        lv_area_t f2_area = { (int16_t)(carX + 46), (int16_t)(carY + 12), (int16_t)(carX + 70), (int16_t)(carY + 24) };
        lv_draw_rect(layer, &body_dsc, &f2_area);

        // 5. Ventanas Curvas del Vocho
        body_dsc.bg_color = lv_color_hex(0xa8ded0);
        body_dsc.radius = 5;
        // Ventana Trasera
        lv_area_t win1 = { (int16_t)(carX + 22), (int16_t)(carY - 9), (int16_t)(carX + 35), (int16_t)(carY + 4) };
        lv_draw_rect(layer, &body_dsc, &win1);
        // Ventana Delantera
        lv_area_t win2 = { (int16_t)(carX + 39), (int16_t)(carY - 9), (int16_t)(carX + 54), (int16_t)(carY + 4) };
        lv_draw_rect(layer, &body_dsc, &win2);

        // 6. Faro Delantero Redondo Cromado del Vocho
        body_dsc.bg_color = lv_color_hex(0xffea00);
        body_dsc.radius = LV_RADIUS_CIRCLE;
        lv_area_t light_area = { (int16_t)(carX + 73), (int16_t)(carY + 6), (int16_t)(carX + 81), (int16_t)(carY + 14) };
        lv_draw_rect(layer, &body_dsc, &light_area);

        // 7. Parachoques Cromados (Metal)
        body_dsc.bg_color = lv_color_hex(0xe0e0e0);
        body_dsc.radius = 2;
        // Parachoques Delantero
        lv_area_t bump_front = { (int16_t)(carX + 75), (int16_t)(carY + 18), (int16_t)(carX + 83), (int16_t)(carY + 23) };
        lv_draw_rect(layer, &body_dsc, &bump_front);
        // Parachoques Trasero
        lv_area_t bump_rear = { (int16_t)(carX - 10), (int16_t)(carY + 18), (int16_t)(carX - 2), (int16_t)(carY + 23) };
        lv_draw_rect(layer, &body_dsc, &bump_rear);

        // 8. Ruedas Giratorias con Tapacubos de Platillo Cromado
        lv_draw_rect_dsc_t wheel_dsc;
        lv_draw_rect_dsc_init(&wheel_dsc);
        wheel_dsc.bg_color = lv_color_hex(0x111111);
        wheel_dsc.bg_opa = LV_OPA_COVER;
        wheel_dsc.radius = LV_RADIUS_CIRCLE;

        int32_t w1X = carX + 14;
        int32_t w2X = carX + 58;
        int32_t wY = carY + 22;
        int wheelR = 10;

        // Rueda Trasera
        lv_area_t w1_area = { (int16_t)(w1X - wheelR), (int16_t)(wY - wheelR), (int16_t)(w1X + wheelR), (int16_t)(wY + wheelR) };
        lv_draw_rect(layer, &wheel_dsc, &w1_area);

        // Rueda Delantera
        lv_area_t w2_area = { (int16_t)(w2X - wheelR), (int16_t)(wY - wheelR), (int16_t)(w2X + wheelR), (int16_t)(wY + wheelR) };
        lv_draw_rect(layer, &wheel_dsc, &w2_area);

        // Tapacubos cromados centrales
        wheel_dsc.bg_color = lv_color_hex(0xdddddd);
        int capR = 4;
        lv_area_t c1_area = { (int16_t)(w1X - capR), (int16_t)(wY - capR), (int16_t)(w1X + capR), (int16_t)(wY + capR) };
        lv_draw_rect(layer, &wheel_dsc, &c1_area);
        lv_area_t c2_area = { (int16_t)(w2X - capR), (int16_t)(wY - capR), (int16_t)(w2X + capR), (int16_t)(wY + capR) };
        lv_draw_rect(layer, &wheel_dsc, &c2_area);

        // Radios giratorios
        lv_draw_line_dsc_t spoke_dsc;
        lv_draw_line_dsc_init(&spoke_dsc);
        spoke_dsc.color = lv_color_hex(0x666666);
        spoke_dsc.width = 2;

        float angle = m_tick * 0.35f;
        float cosA = cosf(angle) * 7.0f;
        float sinA = sinf(angle) * 7.0f;

        spoke_dsc.p1.x = (int32_t)(w1X - cosA); spoke_dsc.p1.y = (int32_t)(wY - sinA);
        spoke_dsc.p2.x = (int32_t)(w1X + cosA); spoke_dsc.p2.y = (int32_t)(wY + sinA);
        lv_draw_line(layer, &spoke_dsc);

        spoke_dsc.p1.x = (int32_t)(w2X - cosA); spoke_dsc.p1.y = (int32_t)(wY - sinA);
        spoke_dsc.p2.x = (int32_t)(w2X + cosA); spoke_dsc.p2.y = (int32_t)(wY + sinA);
        lv_draw_line(layer, &spoke_dsc);

    } else if (m_style == Style::Fireflies) {
        // Estilo: Luciérnagas en el Bosque Vectorial
        // 1. Silueta del Bosque y Pinos en la parte inferior
        lv_draw_rect_dsc_t tree_dsc;
        int forestY = (int)(height * 0.72f);
        
        // Suelo oscuro del bosque
        lv_draw_rect_dsc_init(&tree_dsc);
        tree_dsc.bg_color = lv_color_hex(0x05130b);
        tree_dsc.bg_opa = LV_OPA_COVER;
        lv_area_t ground_area = { 0, (int16_t)forestY, (int16_t)width, (int16_t)height };
        lv_draw_rect(layer, &tree_dsc, &ground_area);

        // Siluetas de pinos (dibujados con rectángulos/triángulos vectoriales a distintas alturas)
        static const int treeX[] = { 15, 65, 130, 195, 260, 330, 400, 445 };
        static const int treeH[] = { 110, 150, 95, 170, 125, 160, 105, 140 };

        for (int i = 0; i < 8; i++) {
            int tx = treeX[i];
            int th = treeH[i];
            int ty = forestY;

            // Tronco
            tree_dsc.bg_color = lv_color_hex(0x080f0a);
            tree_dsc.radius = 2;
            lv_area_t trunk_area = { (int16_t)(tx + th/6 - 3), (int16_t)(ty - 20), (int16_t)(tx + th/6 + 3), (int16_t)ty };
            lv_draw_rect(layer, &tree_dsc, &trunk_area);

            // Capas de copas de pino
            tree_dsc.bg_color = (i % 2 == 0) ? lv_color_hex(0x0a1e12) : lv_color_hex(0x06150c);
            tree_dsc.radius = 12;

            for (int layerIdx = 0; layerIdx < 3; layerIdx++) {
                int layerW = (th / 3) + (2 - layerIdx) * 16;
                int layerY = ty - 20 - (layerIdx + 1) * (th / 4);
                lv_area_t p_area = { (int16_t)(tx + th/6 - layerW/2), (int16_t)layerY, (int16_t)(tx + th/6 + layerW/2), (int16_t)(layerY + 30) };
                lv_draw_rect(layer, &tree_dsc, &p_area);
            }
        }

        // 2. Partículas de Luciérnagas Vectoriales en Movimiento y Pulso de Luz
        lv_draw_rect_dsc_t ff_dsc;
        for (int i = 0; i < PARTICLE_COUNT; i++) {
            lv_draw_rect_dsc_init(&ff_dsc);
            ff_dsc.radius = LV_RADIUS_CIRCLE;

            float pulse = sinf(m_particles[i].phase) * 0.45f + 0.55f;
            float currentRadius = (m_particles[i].radius * 0.6f + 2.0f);

            // Aura exterior translúcida (Verde Neón Luciérnaga)
            ff_dsc.bg_color = lv_color_hex(0xd4ff00);
            ff_dsc.bg_opa = (lv_opa_t)(m_particles[i].alpha * pulse * 100.0f);
            float auraR = currentRadius + 5.0f;
            lv_area_t aura_area = {
                (int16_t)(m_particles[i].x - auraR),
                (int16_t)(m_particles[i].y - auraR),
                (int16_t)(m_particles[i].x + auraR),
                (int16_t)(m_particles[i].y + auraR)
            };
            lv_draw_rect(layer, &ff_dsc, &aura_area);

            // Núcleo brillante (Amarillo Cálido)
            ff_dsc.bg_color = lv_color_hex(0xffff99);
            ff_dsc.bg_opa = (lv_opa_t)(m_particles[i].alpha * pulse * 255.0f);
            lv_area_t core_area = {
                (int16_t)(m_particles[i].x - currentRadius),
                (int16_t)(m_particles[i].y - currentRadius),
                (int16_t)(m_particles[i].x + currentRadius),
                (int16_t)(m_particles[i].y + currentRadius)
            };
            lv_draw_rect(layer, &ff_dsc, &core_area);
        }
    } else if (m_style == Style::TouchSwarm) {
        // Estilo: Partículas Magnéticas Interactivas al Tacto
        // Detectar si el toque está activo
        lv_indev_t* indev = lv_indev_get_next(NULL);
        bool isPressed = false;
        lv_point_t touchPoint = {0, 0};
        if (indev && lv_indev_get_state(indev) == LV_INDEV_STATE_PRESSED) {
            isPressed = true;
            lv_indev_get_point(indev, &touchPoint);
        }

        // Anillos táctiles magnéticos alrededor del toque
        if (isPressed) {
            lv_draw_rect_dsc_t t_dsc;
            lv_draw_rect_dsc_init(&t_dsc);
            t_dsc.radius = LV_RADIUS_CIRCLE;
            t_dsc.bg_color = lv_color_hex(0x00f5d4);
            t_dsc.bg_opa = LV_OPA_30;
            float rPulse = sinf(m_tick * 0.2f) * 6.0f + 25.0f;
            lv_area_t t_area = { (int16_t)(touchPoint.x - rPulse), (int16_t)(touchPoint.y - rPulse), (int16_t)(touchPoint.x + rPulse), (int16_t)(touchPoint.y + rPulse) };
            lv_draw_rect(layer, &t_dsc, &t_area);

            // Líneas de atracción hacia las partículas más cercanas
            lv_draw_line_dsc_t l_dsc;
            lv_draw_line_dsc_init(&l_dsc);
            l_dsc.color = lv_color_hex(0xff007f);
            l_dsc.width = 1;
            for (int i = 0; i < PARTICLE_COUNT; i++) {
                float dx = touchPoint.x - m_particles[i].x;
                float dy = touchPoint.y - m_particles[i].y;
                if (dx * dx + dy * dy < 180.0f * 180.0f) {
                    l_dsc.opa = (lv_opa_t)(120);
                    l_dsc.p1.x = touchPoint.x; l_dsc.p1.y = touchPoint.y;
                    l_dsc.p2.x = (int32_t)m_particles[i].x; l_dsc.p2.y = (int32_t)m_particles[i].y;
                    lv_draw_line(layer, &l_dsc);
                }
            }
        }

        // Partículas Neón Interactivas
        lv_draw_rect_dsc_t p_dsc;
        for (int i = 0; i < PARTICLE_COUNT; i++) {
            lv_draw_rect_dsc_init(&p_dsc);
            p_dsc.radius = LV_RADIUS_CIRCLE;
            p_dsc.bg_color = (i % 2 == 0) ? lv_color_hex(0x00f5d4) : lv_color_hex(0xff007f);
            p_dsc.bg_opa = LV_OPA_COVER;
            
            float pr = m_particles[i].radius * 0.8f + 2.0f;
            lv_area_t p_area = { (int16_t)(m_particles[i].x - pr), (int16_t)(m_particles[i].y - pr), (int16_t)(m_particles[i].x + pr), (int16_t)(m_particles[i].y + pr) };
            lv_draw_rect(layer, &p_dsc, &p_area);
        }
    } else if (m_style == Style::Synthwave80s) {
        // Estilo: Horizonte Synthwave 3D / Cyberpunk Grid
        int horizonY = (int)(height * 0.58f);

        // 1. Sol Retro Neón
        lv_draw_rect_dsc_t sun_dsc;
        lv_draw_rect_dsc_init(&sun_dsc);
        sun_dsc.radius = LV_RADIUS_CIRCLE;
        sun_dsc.bg_color = lv_color_hex(0xff007f);
        sun_dsc.bg_opa = LV_OPA_COVER;

        int32_t sunX = width / 2;
        int32_t sunY = horizonY - 45;
        int sunR = 55;
        lv_area_t sun_area = { (int16_t)(sunX - sunR), (int16_t)(sunY - sunR), (int16_t)(sunX + sunR), (int16_t)(sunY + sunR) };
        lv_draw_rect(layer, &sun_dsc, &sun_area);

        // Franjas horizontales de sombra en el Sol
        lv_draw_rect_dsc_t slit_dsc;
        lv_draw_rect_dsc_init(&slit_dsc);
        slit_dsc.bg_color = lv_color_hex(0x0a0d1a);
        slit_dsc.bg_opa = LV_OPA_COVER;
        for (int s = 0; s < 5; s++) {
            int32_t sy = sunY + s * 9 + 4;
            lv_area_t slit_area = { (int16_t)(sunX - sunR - 4), (int16_t)sy, (int16_t)(sunX + sunR + 4), (int16_t)(sy + 2 + s) };
            lv_draw_rect(layer, &slit_dsc, &slit_area);
        }

        // 2. Montañas Wireframe al Horizonte
        lv_draw_line_dsc_t m_dsc;
        lv_draw_line_dsc_init(&m_dsc);
        m_dsc.color = lv_color_hex(0x9d4edd);
        m_dsc.width = 2;
        static const int pks[] = { 0, 40, 80, 140, 200, 240, 310, 380, 440, 480 };
        static const int pkh[] = { 0, 35, 10, 50,  20,  60,  15,  45,  10,  0 };
        for (int i = 0; i < 9; i++) {
            m_dsc.p1.x = pks[i]; m_dsc.p1.y = horizonY - pkh[i];
            m_dsc.p2.x = pks[i+1]; m_dsc.p2.y = horizonY - pkh[i+1];
            lv_draw_line(layer, &m_dsc);
        }

        // 3. Malla 3D Perspectiva en Movimiento (Cyan Neón)
        lv_draw_line_dsc_t grid_dsc;
        lv_draw_line_dsc_init(&grid_dsc);
        grid_dsc.color = lv_color_hex(0x00f5d4);
        grid_dsc.width = 1;
        grid_dsc.opa = LV_OPA_80;

        // Líneas longitudinales convergentes al centro
        int vpX = width / 2;
        for (int x = -100; x <= width + 100; x += 40) {
            grid_dsc.p1.x = vpX; grid_dsc.p1.y = horizonY;
            grid_dsc.p2.x = x; grid_dsc.p2.y = height;
            lv_draw_line(layer, &grid_dsc);
        }

        // Líneas transversales avanzando hacia abajo
        float gProgress = fmodf(m_tick * 1.6f, 30.0f);
        for (int i = 0; i < 8; i++) {
            float distRatio = (i * 30.0f + gProgress) / 240.0f;
            if (distRatio > 1.0f) continue;
            float lineY = horizonY + distRatio * distRatio * (height - horizonY);
            grid_dsc.p1.x = 0; grid_dsc.p1.y = (int32_t)lineY;
            grid_dsc.p2.x = width; grid_dsc.p2.y = (int32_t)lineY;
            lv_draw_line(layer, &grid_dsc);
        }
    } else {
        // Estilo: Constelación Neón (Partículas + Nodos Flotantes)
        // 1. Dibujar Líneas de Constelación entre Nodos Cercanos (radio optimizado 95px)
        lv_draw_line_dsc_t conn_dsc;
        lv_draw_line_dsc_init(&conn_dsc);
        conn_dsc.color = lv_color_hex(0x4cc9f0);
        conn_dsc.width = 1;

        for (int i = 0; i < PARTICLE_COUNT; i++) {
            for (int j = i + 1; j < PARTICLE_COUNT; j++) {
                float dx = m_particles[i].x - m_particles[j].x;
                float dy = m_particles[i].y - m_particles[j].y;
                float distSq = dx * dx + dy * dy;

                if (distSq < 95.0f * 95.0f) {
                    float dist = sqrtf(distSq);
                    float alphaRatio = 1.0f - (dist / 95.0f);
                    conn_dsc.opa = (lv_opa_t)(alphaRatio * 85);

                    conn_dsc.p1.x = (int32_t)m_particles[i].x;
                    conn_dsc.p1.y = (int32_t)m_particles[i].y;
                    conn_dsc.p2.x = (int32_t)m_particles[j].x;
                    conn_dsc.p2.y = (int32_t)m_particles[j].y;
                    lv_draw_line(layer, &conn_dsc);
                }
            }
        }

        // 2. Dibujar Orbes / Partículas Vectoriales Flotantes (Círculos limpios sin pase de outline)
        lv_draw_rect_dsc_t orb_dsc;
        for (int i = 0; i < PARTICLE_COUNT; i++) {
            lv_draw_rect_dsc_init(&orb_dsc);
            
            float pulse = sinf(m_particles[i].phase) * 0.25f + 0.75f;
            float currentRadius = m_particles[i].radius * pulse;

            orb_dsc.radius = LV_RADIUS_CIRCLE; // Círculo perfecto
            orb_dsc.bg_color = (i % 2 == 0) ? lv_color_hex(0x00f5d4) : lv_color_hex(0x7209b7);
            orb_dsc.bg_opa = (lv_opa_t)(m_particles[i].alpha * pulse * 255.0f);
            
            lv_area_t orb_area = {
                (int16_t)(m_particles[i].x - currentRadius),
                (int16_t)(m_particles[i].y - currentRadius),
                (int16_t)(m_particles[i].x + currentRadius),
                (int16_t)(m_particles[i].y + currentRadius)
            };
            lv_draw_rect(layer, &orb_dsc, &orb_area);
        }
    }
}

} // namespace ui
} // namespace cbdos
