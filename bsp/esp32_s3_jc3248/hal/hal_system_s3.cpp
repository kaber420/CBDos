#include "cbdos/system.hpp"
#include <Arduino.h>

namespace cbdos {
namespace system {

uint32_t getTimeMs() {
    return millis();
}

uint64_t getTimeUs() {
    return micros();
}

void sleepMs(uint32_t ms) {
    delay(ms);
}

void sleepUs(uint32_t us) {
    delayMicroseconds(us);
}

void yieldTask() {
    yield();
}

size_t getFreeHeap() {
    return ESP.getFreeHeap();
}

size_t getTotalHeap() {
    return ESP.getHeapSize();
}

size_t getFreePsram() {
    return ESP.getFreePsram();
}

size_t getTotalPsram() {
    return ESP.getPsramSize();
}

float getCpuTemperature() {
    return temperatureRead();
}

void restart() {
    ESP.restart();
}

void log(LogLevel level, const char* tag, const char* format, ...) {
    const char* lvlStr = "INFO";
    switch (level) {
        case LogLevel::Debug: lvlStr = "DEBUG"; break;
        case LogLevel::Info:  lvlStr = "INFO";  break;
        case LogLevel::Warn:  lvlStr = "WARN";  break;
        case LogLevel::Error: lvlStr = "ERROR"; break;
    }
    
    char buf[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    
    Serial.printf("[%s] [%s] %s\n", lvlStr, tag ? tag : "CBDos", buf);
}

} // namespace system
} // namespace cbdos
