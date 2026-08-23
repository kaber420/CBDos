#include "cbdos/persistence.hpp"
#include <Arduino.h>
#include <Preferences.h>

namespace cbdos {
namespace bsp {

class ArduinoPersistenceBackend : public persistence::IPersistenceBackend {
public:
    ArduinoPersistenceBackend() : m_isOpen(false) {}

    ~ArduinoPersistenceBackend() override {
        end();
    }

    bool begin(const char* nameSpace, bool readOnly = false) override {
        if (m_isOpen) {
            end();
        }
        m_isOpen = m_prefs.begin(nameSpace, readOnly);
        return m_isOpen;
    }

    void end() override {
        if (m_isOpen) {
            m_prefs.end();
            m_isOpen = false;
        }
    }

    bool setUChar(const char* key, uint8_t value) override {
        if (!m_isOpen) return false;
        return (m_prefs.putUChar(key, value) > 0);
    }

    uint8_t getUChar(const char* key, uint8_t defaultVal) override {
        if (!m_isOpen) return defaultVal;
        return m_prefs.getUChar(key, defaultVal);
    }

    bool setUShort(const char* key, uint16_t value) override {
        if (!m_isOpen) return false;
        return (m_prefs.putUShort(key, value) > 0);
    }

    uint16_t getUShort(const char* key, uint16_t defaultVal) override {
        if (!m_isOpen) return defaultVal;
        return m_prefs.getUShort(key, defaultVal);
    }

    bool setUInt(const char* key, uint32_t value) override {
        if (!m_isOpen) return false;
        return (m_prefs.putUInt(key, value) > 0);
    }

    uint32_t getUInt(const char* key, uint32_t defaultVal) override {
        if (!m_isOpen) return defaultVal;
        return m_prefs.getUInt(key, defaultVal);
    }

    bool setInt(const char* key, int32_t value) override {
        if (!m_isOpen) return false;
        return (m_prefs.putInt(key, value) > 0);
    }

    int32_t getInt(const char* key, int32_t defaultVal) override {
        if (!m_isOpen) return defaultVal;
        return m_prefs.getInt(key, defaultVal);
    }

    bool setFloat(const char* key, float value) override {
        if (!m_isOpen) return false;
        return (m_prefs.putFloat(key, value) > 0);
    }

    float getFloat(const char* key, float defaultVal) override {
        if (!m_isOpen) return defaultVal;
        return m_prefs.getFloat(key, defaultVal);
    }

    bool setBool(const char* key, bool value) override {
        if (!m_isOpen) return false;
        return (m_prefs.putBool(key, value) > 0);
    }

    bool getBool(const char* key, bool defaultVal) override {
        if (!m_isOpen) return defaultVal;
        return m_prefs.getBool(key, defaultVal);
    }

    bool setString(const char* key, const std::string& value) override {
        if (!m_isOpen) return false;
        return (m_prefs.putString(key, value.c_str()) > 0);
    }

    std::string getString(const char* key, const std::string& defaultVal) override {
        if (!m_isOpen) return defaultVal;
        String val = m_prefs.getString(key, defaultVal.c_str());
        return std::string(val.c_str());
    }

    bool remove(const char* key) override {
        if (!m_isOpen) return false;
        return m_prefs.remove(key);
    }

    bool clear() override {
        if (!m_isOpen) return false;
        return m_prefs.clear();
    }

private:
    Preferences m_prefs;
    bool m_isOpen;
};

static ArduinoPersistenceBackend s_s3PersistenceBackend;

void initPersistenceBackend() {
    persistence::setBackend(&s_s3PersistenceBackend);
    Serial.println("[NVS_S3] Arduino Preferences Persistence Backend inicializado e inyectado.");
}

} // namespace bsp
} // namespace cbdos
