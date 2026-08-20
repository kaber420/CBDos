#include "cbdos/audio.hpp"
#include "AudioHAL.h"
#include "AudioPlayer.hpp"

namespace cbdos {
namespace audio {

bool init() {
    AudioHAL::getInstance().init(44100);
    return true;
}

bool playStream(const char* url) {
    return AudioPlayer::getInstance().playStream(url);
}

bool playFile(const char* filepath) {
    return AudioPlayer::getInstance().play(filepath);
}

void stop() {
    AudioPlayer::getInstance().stop();
}

void pause() {
    AudioPlayer::getInstance().pause();
}

void resume() {
    AudioPlayer::getInstance().resume();
}

void seek(uint32_t seconds) {
    AudioPlayer::getInstance().seek(seconds);
}

uint32_t getCurrentTimeSec() {
    return AudioPlayer::getInstance().getCurrentTimeSec();
}

uint32_t getTotalTimeSec() {
    return AudioPlayer::getInstance().getTotalTimeSec();
}

bool writeAudio(const void* src, size_t size) {
    size_t written = 0;
    return AudioHAL::getInstance().writeAudio(src, size, &written) == ESP_OK;
}

void setVolume(uint8_t volumePercent) {
    AudioHAL::getInstance().setVolume(volumePercent);
}

uint8_t getVolume() {
    return AudioHAL::getInstance().getVolume();
}

bool setSampleRate(uint32_t sampleRate) {
    return AudioHAL::getInstance().setSampleRate(sampleRate) == ESP_OK;
}

void playTone(uint32_t freqHz, uint32_t durationMs) {
    AudioHAL::getInstance().playTone(freqHz, durationMs);
}

void playBeep() {
    AudioHAL::getInstance().playBeep();
}

AudioStats getStats() {
    return AudioPlayer::getInstance().getStats();
}

} // namespace audio
} // namespace cbdos


