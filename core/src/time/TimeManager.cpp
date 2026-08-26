#include "cbdos/time.hpp"
#include "cbdos/network.hpp"
#include "cbdos/system.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

namespace cbdos {
namespace time {

static ITimeProvider* s_provider = nullptr;
static TimeSource s_currentSource = TimeSource::None;
static long s_gmtOffsetSec = -21600;
static int s_daylightOffsetSec = 0;
static std::string s_ntpServer = "pool.ntp.org";

static bool s_autoSyncEnabled = true;
static uint8_t s_retryCount = 0;
static bool s_wasConnected = false;
static uint32_t s_lastSyncAttempt = 0;
static uint32_t s_lastSuccessfulSync = 0;

static constexpr uint32_t BASE_RETRY_INTERVAL_MS = 60000; // 1 minute base backoff
static constexpr uint32_t MAX_RETRY_INTERVAL_MS = 15 * 60 * 1000UL; // 15 mins max
static constexpr uint32_t SYNC_REFRESH_INTERVAL_MS = 3 * 3600 * 1000UL; // 3 hours
static std::function<void()> s_towerSyncRequestCb = nullptr;

static void applyPosixTimezone(long gmtOffsetSec, int daylightOffsetSec) {
    long totalOffset = gmtOffsetSec + daylightOffsetSec;
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

void init(ITimeProvider* provider) {
    s_provider = provider;
    s_currentSource = TimeSource::None;
    s_retryCount = 0;
    s_wasConnected = false;
    s_lastSyncAttempt = 0;
    s_lastSuccessfulSync = 0;
    
    applyPosixTimezone(s_gmtOffsetSec, s_daylightOffsetSec);

    if (s_provider) {
        s_provider->init(s_gmtOffsetSec, s_daylightOffsetSec, s_ntpServer.c_str());
    }
}

void setTimezone(long gmtOffsetSec, int daylightOffsetSec) {
    s_gmtOffsetSec = gmtOffsetSec;
    s_daylightOffsetSec = daylightOffsetSec;
    applyPosixTimezone(s_gmtOffsetSec, s_daylightOffsetSec);
    if (s_provider) {
        s_provider->setTimezone(s_gmtOffsetSec, s_daylightOffsetSec);
    }
}

void setNtpServer(const char* server) {
    if (server) {
        s_ntpServer = server;
        if (s_provider) {
            s_provider->setServer(s_ntpServer.c_str());
        }
    }
}

void setNtpEnabled(bool enabled) {
    setAutoSyncEnabled(enabled); // Legacy wrapper
}

void setAutoSyncEnabled(bool enabled) {
    s_autoSyncEnabled = enabled;
    s_retryCount = 0;
    if (!s_autoSyncEnabled && s_provider) {
        s_provider->stopNtp();
    }
}

bool isAutoSyncEnabled() {
    return s_autoSyncEnabled;
}

void syncNtp() {
    if (s_provider && cbdos::network::isConnected()) {
        cbdos::system::log(cbdos::system::LogLevel::Info, "TimeManager", "Sincronizando NTP por Wi-Fi...");
        s_provider->syncNtp();
        s_lastSyncAttempt = cbdos::system::getTimeMs();
    }
}

void notifySynced(TimeSource source) {
    s_currentSource = source;
    s_lastSuccessfulSync = cbdos::system::getTimeMs();
    s_retryCount = 0;
    cbdos::system::log(cbdos::system::LogLevel::Info, "TimeManager", "Hora sincronizada correctamente. Origen: %d", (int)source);
}

void setEpoch(time_t epoch, TimeSource source) {
    if (!s_autoSyncEnabled) return;

#ifdef _WIN32
    // Windows implementation not strictly needed for this OS port, ignored for now
#else
    struct timeval tv;
    tv.tv_sec = epoch;
    tv.tv_usec = 0;
    settimeofday(&tv, nullptr);
#endif
    
    notifySynced(source);
}

void setTowerSyncRequestCallback(std::function<void()> cb) {
    s_towerSyncRequestCb = cb;
}

void update() {
    if (!s_autoSyncEnabled) return;

    uint32_t now = cbdos::system::getTimeMs();
    bool connected = cbdos::network::isConnected();
    
    if (connected && !s_wasConnected) {
        s_retryCount = 0; // Reset backoff al conectarse a red
    }
    s_wasConnected = connected;

    if (isSynced()) {
        // Refresco de 3 horas para Wi-Fi SNTP (en Radio se recibe pasivamente por broadcast cada 60s)
        if (now - s_lastSuccessfulSync >= SYNC_REFRESH_INTERVAL_MS) {
            if (connected && s_provider) {
                syncNtp();
            }
        }
        return;
    }

    // --- Ciclo Wi-Fi SNTP (Backoff Exponencial) ---
    // Si hay Wi-Fi conectado, intentamos sincronizar con pool.ntp.org.
    // Si no hay Wi-Fi, el sistema espera pasivamente el micro-broadcast de la Torre (0 TX).
    if (connected && s_provider) {
        uint32_t currentRetryInterval = BASE_RETRY_INTERVAL_MS;
        if (s_retryCount < 4) { // 1m, 2m, 4m, 8m
            currentRetryInterval = BASE_RETRY_INTERVAL_MS * (1UL << s_retryCount);
        } else {
            currentRetryInterval = MAX_RETRY_INTERVAL_MS;
        }
        
        if (now - s_lastSyncAttempt >= currentRetryInterval) {
            if (s_retryCount < 255) s_retryCount++;
            s_lastSyncAttempt = now;
            syncNtp();
        }
    }
}

TimeSource getSource() {
    return s_currentSource;
}

bool isSynced() {
    time_t now_epoch = getEpoch();
    if (now_epoch > 1700000000) { 
        if (s_currentSource == TimeSource::None) {
            s_currentSource = TimeSource::Local;
        }
        return true;
    }
    s_currentSource = TimeSource::None;
    return false;
}

time_t getEpoch() {
    return ::time(nullptr);
}

void getFormattedTime(char* buf, size_t len, const char* format) {
    if (!buf || len == 0) return;

    if (!isSynced()) {
        std::snprintf(buf, len, "--:--");
        return;
    }

    time_t now = getEpoch();
    struct tm timeinfo;

#ifdef _WIN32
    struct tm* t = localtime(&now);
    if (t) timeinfo = *t;
#else
    if (!localtime_r(&now, &timeinfo)) {
        std::snprintf(buf, len, "--:--");
        return;
    }
#endif

    const char* fmt = (format && strlen(format) > 0) ? format : "%H:%M";
    strftime(buf, len, fmt, &timeinfo);
}

void getFormattedDate(char* buf, size_t len, const char* format) {
    if (!buf || len == 0) return;

    if (!isSynced()) {
        std::snprintf(buf, len, "--/--/----");
        return;
    }

    time_t now = getEpoch();
    struct tm timeinfo;

#ifdef _WIN32
    struct tm* t = localtime(&now);
    if (t) timeinfo = *t;
#else
    if (!localtime_r(&now, &timeinfo)) {
        std::snprintf(buf, len, "--/--/----");
        return;
    }
#endif

    const char* fmt = (format && strlen(format) > 0) ? format : "%d/%m/%Y";
    strftime(buf, len, fmt, &timeinfo);
}

} // namespace time
} // namespace cbdos
