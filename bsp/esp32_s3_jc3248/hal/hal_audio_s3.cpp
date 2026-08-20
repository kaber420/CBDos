#include "cbdos/audio.hpp"

namespace cbdos {
namespace audio {

static uint8_t s_volume = 75;
static AudioStats s_stats = {
    .isPlaying = false,
    .codec = CodecType::None,
    .sampleRate = 44100,
    .channels = 2,
    .bitRate = 128000,
    .bufferPercent = 0
};

bool init() {
    s_volume = 75;
    return true;
}

bool playStream(const char* url) {
    if (!url) return false;
    s_stats.isPlaying = true;
    s_stats.codec = CodecType::MP3;
    return true;
}

bool playFile(const char* filepath) {
    if (!filepath) return false;
    s_stats.isPlaying = true;
    s_stats.codec = CodecType::WAV;
    return true;
}

void stop() {
    s_stats.isPlaying = false;
    s_stats.codec = CodecType::None;
}

void pause() {
    s_stats.isPlaying = false;
}

void resume() {
    s_stats.isPlaying = true;
}

void setVolume(uint8_t volumePercent) {
    if (volumePercent > 100) volumePercent = 100;
    s_volume = volumePercent;
}

uint8_t getVolume() {
    return s_volume;
}

void playTone(uint32_t freqHz, uint32_t durationMs) {
    (void)freqHz;
    (void)durationMs;
}

void playBeep() {
}

void seek(uint32_t seconds) {
    (void)seconds;
}

uint32_t getCurrentTimeSec() {
    return 0;
}

uint32_t getTotalTimeSec() {
    return 0;
}

bool writeAudio(const void* src, size_t size) {
    (void)src;
    (void)size;
    return true;
}

AudioStats getStats() {
    return s_stats;
}

} // namespace audio
} // namespace cbdos
