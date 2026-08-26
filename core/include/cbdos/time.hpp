#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <functional>
#include <time.h>

namespace cbdos {
namespace time {

enum class TimeSource {
    None,
    Local,
    SNTP,
    RTC,
    Tower  // Hora recibida desde el beacon del Gateway (C3/SBC)
};

class ITimeProvider {
public:
    virtual ~ITimeProvider() = default;
    // El proveedor debe llamar a cbdos::time::notifySynced() cuando obtenga la hora exitosamente.
    virtual void init(long gmtOffsetSec, int daylightOffsetSec, const char* ntpServer) = 0;
    virtual void setTimezone(long gmtOffsetSec, int daylightOffsetSec) = 0;
    virtual void setServer(const char* ntpServer) = 0;
    virtual void syncNtp() = 0;
    virtual void stopNtp() = 0;
};

// Funciones para el ciclo de vida del sistema
void init(ITimeProvider* provider = nullptr);
void update(); // Se llama periódicamente (loop o task) para aplicar backoffs y reintentos.
void notifySynced(TimeSource source); // Llamado por el ITimeProvider u otros orígenes.

// API Pública de Configuración
void setTimezone(long gmtOffsetSec, int daylightOffsetSec);
void setNtpServer(const char* server);
void setNtpEnabled(bool enabled);
void setAutoSyncEnabled(bool enabled);
bool isAutoSyncEnabled();
void syncNtp(); // Fuerza un intento de sincronización SNTP.
void setEpoch(time_t epoch, TimeSource source); // Inyecta la hora desde cualquier fuente
void setTowerSyncRequestCallback(std::function<void()> cb); // Inyectado por el BSP al arranque

// API Pública de Lectura
TimeSource getSource();
bool isSynced();
time_t getEpoch();
void getFormattedTime(char* buf, size_t len, const char* format = "%H:%M");
void getFormattedDate(char* buf, size_t len, const char* format = "%d/%m/%Y");

} // namespace time
} // namespace cbdos
