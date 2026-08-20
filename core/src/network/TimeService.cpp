#include "TimeService.h"
#include "cbdos/network.hpp"
#include "cbdos/system.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>

#ifdef ARDUINO
#include <WiFi.h>
#include <esp_sntp.h>
#elif defined(ESP_PLATFORM)
#include <esp_sntp.h>
#include <esp_log.h>
#endif

static void applyPosixTimezone(long gmtOffsetSec, int daylightOffsetSec) {
    long totalOffset = gmtOffsetSec + daylightOffsetSec;
    // En formato POSIX TZ, el signo está invertido respecto a ISO:
    // UTC-6 (México) -> "UTC+6"
    long posixHours = -(totalOffset / 3600);
    long posixMins = std::abs((totalOffset % 3600) / 60);
    char tzBuf[32];
    if (posixMins != 0) {
        std::snprintf(tzBuf, sizeof(tzBuf), "UTC%+ld:%02ld", posixHours, posixMins);
    } else {
        std::snprintf(tzBuf, sizeof(tzBuf), "UTC%+ld", posixHours);
    }
    setenv("TZ", tzBuf, 1);
    tzset();
}

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
    applyPosixTimezone(m_gmtOffsetSec, m_daylightOffsetSec);
}

void TimeService::sync() {
    applyPosixTimezone(m_gmtOffsetSec, m_daylightOffsetSec);

#ifdef ARDUINO
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[TimeService] Sincronizando NTP con %s (Offset: %ld, DST: %d)...\n",
                      m_ntpServer.c_str(), m_gmtOffsetSec, m_daylightOffsetSec);
        configTime(m_gmtOffsetSec, m_daylightOffsetSec, m_ntpServer.c_str(), "time.nist.gov", "time.google.com");
        m_sntpConfigured = true;
        m_lastSyncAttempt = millis();
    }
#elif defined(ESP_PLATFORM)
    if (cbdos::network::isConnected()) {
        ESP_LOGI("TimeService", "Iniciando sincronizacion SNTP con %s (Offset: %ld, DST: %d)...",
                 m_ntpServer.c_str(), m_gmtOffsetSec, m_daylightOffsetSec);
        if (esp_sntp_enabled()) {
            esp_sntp_stop();
        }
        esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, m_ntpServer.c_str());
        esp_sntp_setservername(1, "time.nist.gov");
        esp_sntp_setservername(2, "time.google.com");
        esp_sntp_init();
        m_sntpConfigured = true;
        m_lastSyncAttempt = cbdos::system::getTimeMs();
    }
#endif
}

void TimeService::update() {
    bool connected = cbdos::network::isConnected();
#ifdef ARDUINO
    connected = connected || (WiFi.status() == WL_CONNECTED);
#endif
    if (!connected) return;

    uint32_t now = cbdos::system::getTimeMs();

    if (!m_sntpConfigured) {
        sync();
        return;
    }

    if (isSynced()) {
        m_lastSuccessfulSync = now;
        // Refresco de fondo cada 3 horas
        if (now - m_lastSyncAttempt >= SYNC_REFRESH_INTERVAL_MS) {
            sync();
        }
    } else {
        // Reintento si no ha sincronizado tras 30 segundos
        if (now - m_lastSyncAttempt >= SYNC_RETRY_INTERVAL_MS) {
            sync();
        }
    }
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
    applyPosixTimezone(m_gmtOffsetSec, m_daylightOffsetSec);

#ifdef ARDUINO
    if (m_sntpConfigured) {
        configTime(m_gmtOffsetSec, m_daylightOffsetSec, m_ntpServer.c_str(), "time.nist.gov", "time.google.com");
    }
#elif defined(ESP_PLATFORM)
    if (m_sntpConfigured && cbdos::network::isConnected()) {
        sync();
    }
#endif
}
