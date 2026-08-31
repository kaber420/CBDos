#include "cbdos/audio.hpp"
#include "../audio/AudioPlayer.hpp"
#include "cbdos/log.hpp"

namespace cbdos {
namespace audio {

static IAudioSink* s_audioSink = nullptr;
static IAudioSource* s_audioSource = nullptr;

void setAudioSink(IAudioSink* sink) {
    s_audioSink = sink;
}

IAudioSink* getAudioSink() {
    return s_audioSink;
}

void setAudioSource(IAudioSource* source) {
    s_audioSource = source;
}

IAudioSource* getAudioSource() {
    return s_audioSource;
}

bool init() {
    bool ok = true;
    if (s_audioSink) {
        ok = s_audioSink->init();
    }
    return ok;
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

AudioStats getStats() {
    return AudioPlayer::getInstance().getStats();
}

void setVolume(uint8_t volumePercent) {
    if (s_audioSink) {
        s_audioSink->setVolume(volumePercent);
    }
}

uint8_t getVolume() {
    if (s_audioSink) {
        return s_audioSink->getVolume();
    }
    return 70;
}

bool setSampleRate(uint32_t sampleRate) {
    if (s_audioSink) {
        return s_audioSink->setSampleRate(sampleRate);
    }
    return false;
}

void playTone(uint32_t freqHz, uint32_t durationMs) {
    if (s_audioSink) {
        s_audioSink->playTone(freqHz, durationMs);
    }
}

void playBeep() {
    if (s_audioSink) {
        s_audioSink->playBeep();
    }
}

bool writeAudio(const void* src, size_t size) {
    if (s_audioSink) {
        return s_audioSink->write(src, size) > 0;
    }
    return false;
}

size_t readAudio(void* dest, size_t sizeBytes, uint32_t timeoutMs) {
    if (s_audioSource) {
        return s_audioSource->read(dest, sizeBytes, timeoutMs);
    }
    return 0;
}

} // namespace audio
} // namespace cbdos
