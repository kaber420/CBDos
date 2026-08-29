#ifndef POWER_MANAGER_HPP
#define POWER_MANAGER_HPP

#include <cstdint>
#include <lvgl.h>

namespace cbdos {
namespace system {

enum class PowerState {
    Active,
    Dimmed,
    ScreenOff,
    LightSleep,
    DeepSleep
};

class PowerManager {
public:
    static PowerManager& getInstance() {
        static PowerManager instance;
        return instance;
    }

    void init();
    void update();
    void notifyActivity();

    PowerState getState() const { return m_state; }
    uint32_t getIdleTimeoutSec() const { return m_idleTimeoutSec; }
    void setIdleTimeoutSec(uint32_t sec);

    void turnOffScreen();
    void enterLightSleep();
    void enterDeepSleep();
    void wakeUp();
    void restart();

private:
    PowerManager();
    ~PowerManager() = default;

    static void touchInputEventCb(lv_event_t* e);

    PowerState m_state;
    uint32_t m_idleTimeoutSec;
    uint32_t m_lastActivityMs;
    uint8_t m_savedBrightness;
    bool m_initialized;
};

} // namespace system
} // namespace cbdos

#endif // POWER_MANAGER_HPP
