#ifndef TIME_SERVICE_H
#define TIME_SERVICE_H

#include <stdint.h>
#include <time.h>
#include <string>

#ifdef ARDUINO
#include <Arduino.h>
#endif

class TimeService {
public:
    static TimeService& getInstance() {
        static TimeService instance;
        return instance;
    }

    void init(long gmtOffsetSec = -21600, int daylightOffsetSec = 0, const char* ntpServer = "pool.ntp.org", bool enabled = true);
    void sync();
    void update(); // Debe llamarse periódicamente desde networkTask o loop

    bool isSynced() const;
    void getFormattedTime(char* buf, size_t len, const char* format = "%H:%M");
    void getFormattedDate(char* buf, size_t len, const char* format = "%d/%m/%Y");
    time_t getEpochTime() const;

    void setTimezone(long gmtOffsetSec, int daylightOffsetSec);
    long getGmtOffsetSec() const { return m_gmtOffsetSec; }
    int getDaylightOffsetSec() const { return m_daylightOffsetSec; }
    const std::string& getNtpServer() const { return m_ntpServer; }
    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled);

private:
    TimeService();
    ~TimeService() = default;

    long m_gmtOffsetSec;
    int m_daylightOffsetSec;
    std::string m_ntpServer;
    bool m_enabled;
    uint8_t m_retryCount;
    bool m_wasConnected;
    uint32_t m_lastSyncAttempt;
    uint32_t m_lastSuccessfulSync;
    bool m_sntpConfigured;

    static constexpr uint32_t BASE_RETRY_INTERVAL_MS = 30000;              // 30s intervalo base
    static constexpr uint32_t MAX_RETRY_INTERVAL_MS = 15 * 60 * 1000UL;    // 15 min tope máximo de espera
    static constexpr uint32_t SYNC_REFRESH_INTERVAL_MS = 3 * 3600 * 1000UL; // 3 horas de refresco regular
};

#endif // TIME_SERVICE_H
