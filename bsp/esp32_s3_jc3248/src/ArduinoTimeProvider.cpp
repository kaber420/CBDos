#include "cbdos/time.hpp"
#include <Arduino.h>

namespace cbdos {
namespace bsp {

class ArduinoTimeProvider : public cbdos::time::ITimeProvider {
private:
    std::string m_server;
    long m_gmtOffset = 0;
    int m_dstOffset = 0;

public:
    ArduinoTimeProvider() {
        m_server = "pool.ntp.org";
    }

    void init(long gmtOffsetSec, int daylightOffsetSec, const char* ntpServer) override {
        if (ntpServer) m_server = ntpServer;
        m_gmtOffset = gmtOffsetSec;
        m_dstOffset = daylightOffsetSec;
        
        // El ESP32 en Arduino core notifica automáticamente cuando se sincroniza 
        // a través del callback sntp_set_time_sync_notification_cb si lo configuramos,
        // pero podemos simplificarlo usando un tick o asumiendo que configTime lanza la peticion.
        // Por simplicidad, Arduino se maneja internamente.
        // Haremos que syncNtp llame a configTime.
    }
    
    void setTimezone(long gmtOffsetSec, int daylightOffsetSec) override {
        m_gmtOffset = gmtOffsetSec;
        m_dstOffset = daylightOffsetSec;
    }
    
    void setServer(const char* ntpServer) override {
        if (ntpServer) m_server = ntpServer;
    }
    
    void syncNtp() override {
        // En Arduino, configTime inicializa la sincronización SNTP de fondo.
        configTime(m_gmtOffset, m_dstOffset, m_server.c_str(), "time.nist.gov", "time.google.com");
    }
    
    void stopNtp() override {
        // Arduino no expone un stop directo fácil en configTime, 
        // pero podemos setear el server a NULL.
        configTime(0, 0, nullptr);
    }
};

static ArduinoTimeProvider s_timeProvider;

cbdos::time::ITimeProvider* getArduinoTimeProvider() {
    return &s_timeProvider;
}

} // namespace bsp
} // namespace cbdos
