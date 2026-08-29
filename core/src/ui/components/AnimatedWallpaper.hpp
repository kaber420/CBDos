#ifndef ANIMATED_WALLPAPER_HPP
#define ANIMATED_WALLPAPER_HPP

#include <lvgl.h>
#include <vector>

namespace cbdos {
namespace ui {

struct Particle {
    float x;
    float y;
    float vx;
    float vy;
    float radius;
    float alpha;
    float pulseSpeed;
    float phase;
};

class AnimatedWallpaper {
public:
    static AnimatedWallpaper& getInstance() {
        static AnimatedWallpaper instance;
        return instance;
    }

    void init(lv_obj_t* parent);
    void start();
    void stop();
    bool isRunning() const { return m_running; }
    void destroy();

private:
    AnimatedWallpaper();
    ~AnimatedWallpaper();

    static void drawCallback(lv_event_t* e);
    static void timerCallback(lv_timer_t* timer);

    void draw(lv_layer_t* layer);
    void updatePhysics();

    lv_obj_t* m_canvasObj;
    lv_timer_t* m_timer;
    bool m_running;
    uint32_t m_tick;
    
    static const int PARTICLE_COUNT = 16;
    Particle m_particles[PARTICLE_COUNT];
};

} // namespace ui
} // namespace cbdos

#endif // ANIMATED_WALLPAPER_HPP
