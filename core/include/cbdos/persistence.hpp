#pragma once

#include <cstdint>
#include <string>

namespace cbdos {
namespace persistence {

class IPersistenceBackend {
public:
    virtual ~IPersistenceBackend() = default;

    virtual bool begin(const char* nameSpace, bool readOnly = false) = 0;
    virtual void end() = 0;

    virtual bool setUChar(const char* key, uint8_t value) = 0;
    virtual uint8_t getUChar(const char* key, uint8_t defaultVal = 0) = 0;

    virtual bool setUShort(const char* key, uint16_t value) = 0;
    virtual uint16_t getUShort(const char* key, uint16_t defaultVal = 0) = 0;

    virtual bool setUInt(const char* key, uint32_t value) = 0;
    virtual uint32_t getUInt(const char* key, uint32_t defaultVal = 0) = 0;

    virtual bool setInt(const char* key, int32_t value) = 0;
    virtual int32_t getInt(const char* key, int32_t defaultVal = 0) = 0;

    virtual bool setFloat(const char* key, float value) = 0;
    virtual float getFloat(const char* key, float defaultVal = 0.0f) = 0;

    virtual bool setBool(const char* key, bool value) = 0;
    virtual bool getBool(const char* key, bool defaultVal = false) = 0;

    virtual bool setString(const char* key, const std::string& value) = 0;
    virtual std::string getString(const char* key, const std::string& defaultVal = "") = 0;

    virtual bool remove(const char* key) = 0;
    virtual bool clear() = 0;
};

void setBackend(IPersistenceBackend* backend);
IPersistenceBackend* getBackend();

} // namespace persistence
} // namespace cbdos
