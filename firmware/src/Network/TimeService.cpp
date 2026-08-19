#include "TimeService.h"
#include <cstdio>
#include <cstring>

#ifdef ARDUINO
#include <WiFi.h>
#include <esp_sntp.h>
#endif

TimeService::TimeService()
    : m_gmtOffsetSec(-21600),
      m_daylightOffsetSec(0),
      m_ntpServer("pool.ntp.org"),
      m_lastSyncAttempt(0),
      m_lastSuccessfulSync(0),
      m_sntpConfigured(false) {
}

void TimeService::init(long gmtOffsetSec, int daylightOffsetSec, const char* ntpServer) {
    m_gmtOffsetSec = gmtOffsetSec;
    m_daylightOffsetSec = daylightOffsetSec;
    if (ntpServer && strlen(ntpServer) > 0) {
        m_ntpServer = ntpServer;
    }
    m_sntpConfigured = false;
    m_lastSyncAttempt = 0;
}

void TimeService::sync() {
#ifdef ARDUINO
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[TimeService] Iniciando sincronizacion NTP con %s (Offset: %ld, DST: %d)...\n",
                      m_ntpServer.c_str(), m_gmtOffsetSec, m_daylightOffsetSec);
        configTime(m_gmtOffsetSec, m_daylightOffsetSec, m_ntpServer.c_str(), "time.nist.gov", "time.google.com");
        m_sntpConfigured = true;
        m_lastSyncAttempt = millis();
    }
#else
    m_lastSyncAttempt = 0;
#endif
}

void TimeService::update() {
#ifdef ARDUINO
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    uint32_t now = millis();

    if (!m_sntpConfigured) {
        sync();
        return;
    }

    if (isSynced()) {
        m_lastSuccessfulSync = now;
        // Refresco de fondo cada 3 horas
        if (now - m_lastSyncAttempt >= SYNC_REFRESH_INTERVAL_MS) {
            Serial.println("[TimeService] Resincronizacion periodica NTP (3 horas transcurridas)...");
            sync();
        }
    } else {
        // Reintento si no ha sincronizado tras 30 segundos
        if (now - m_lastSyncAttempt >= SYNC_RETRY_INTERVAL_MS) {
            Serial.println("[TimeService] Reintentando sincronizacion NTP...");
            sync();
        }
    }
#endif
}

bool TimeService::isSynced() const {
    time_t now = time(nullptr);
    // 1700000000 = aprox. Noviembre 2023. Cualquier fecha posterior indica que el reloj ya fue ajustado por NTP.
    return (now > 1700000000);
}

void TimeService::getFormattedTime(char* buf, size_t len, const char* format) {
    if (!buf || len == 0) return;

    if (!isSynced()) {
        std::snprintf(buf, len, "--:--");
        return;
    }

    time_t now = time(nullptr);
    struct tm timeinfo;
#ifdef ARDUINO
    if (!localtime_r(&now, &timeinfo)) {
        std::snprintf(buf, len, "--:--");
        return;
    }
#else
    struct tm* t = localtime(&now);
    if (!t) {
        std::snprintf(buf, len, "--:--");
        return;
    }
    timeinfo = *t;
#endif

    const char* fmt = (format && strlen(format) > 0) ? format : "%H:%M";
    strftime(buf, len, fmt, &timeinfo);
}

void TimeService::getFormattedDate(char* buf, size_t len, const char* format) {
    if (!buf || len == 0) return;

    if (!isSynced()) {
        std::snprintf(buf, len, "--/--/----");
        return;
    }

    time_t now = time(nullptr);
    struct tm timeinfo;
#ifdef ARDUINO
    if (!localtime_r(&now, &timeinfo)) {
        std::snprintf(buf, len, "--/--/----");
        return;
    }
#else
    struct tm* t = localtime(&now);
    if (!t) {
        std::snprintf(buf, len, "--/--/----");
        return;
    }
    timeinfo = *t;
#endif

    const char* fmt = (format && strlen(format) > 0) ? format : "%d/%m/%Y";
    strftime(buf, len, fmt, &timeinfo);
}

time_t TimeService::getEpochTime() const {
    return time(nullptr);
}

void TimeService::setTimezone(long gmtOffsetSec, int daylightOffsetSec) {
    m_gmtOffsetSec = gmtOffsetSec;
    m_daylightOffsetSec = daylightOffsetSec;
#ifdef ARDUINO
    if (m_sntpConfigured) {
        configTime(m_gmtOffsetSec, m_daylightOffsetSec, m_ntpServer.c_str(), "time.nist.gov", "time.google.com");
    }
#endif
}
