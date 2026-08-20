#pragma once

#include "cbdos/audio.hpp"
#include <string>
#include <vector>
#include <cstdio>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

namespace cbdos {
namespace audio {

enum class AudioFormat {
    Unknown,
    MP3,
    WAV,
    AAC,
    FLAC
};

class AudioPlayer {
public:
    static AudioPlayer& getInstance();

    bool play(const char* filepath);
    void pause();
    void resume();
    void stop();
    void seek(uint32_t targetSec);

    bool isPlaying() const { return m_isPlaying; }
    bool isPaused() const { return m_isPaused; }
    
    uint32_t getCurrentTimeSec() const;
    uint32_t getTotalTimeSec() const { return m_totalTimeSec; }
    float getProgress() const;
    
    const std::string& getCurrentFilePath() const { return m_currentFile; }
    std::string getCurrentTrackTitle() const;
    std::string getFormatString() const;
    AudioFormat getFormat() const { return m_format; }
    
    AudioStats getStats() const;

private:
    AudioPlayer();
    ~AudioPlayer();
    AudioPlayer(const AudioPlayer&) = delete;
    AudioPlayer& operator=(const AudioPlayer&) = delete;

    static void playbackTask(void* param);
    void runPlayback();
    void runMp3Playback();
    void runWavPlayback();

    AudioFormat detectFormat(const char* filepath);

    FILE* m_file = nullptr;
    std::string m_currentFile;
    AudioFormat m_format = AudioFormat::Unknown;
    
    uint32_t m_fileSize = 0;
    uint32_t m_sampleRate = 44100;
    uint8_t m_channels = 2;
    uint32_t m_bitrate = 128000;
    uint32_t m_totalTimeSec = 0;
    volatile uint32_t m_currentSec = 0;
    volatile uint32_t m_bytesProcessed = 0;
    
    volatile bool m_isPlaying = false;
    volatile bool m_isPaused = false;
    volatile bool m_stopRequested = false;
    volatile int32_t m_seekRequestSec = -1;
    
    TaskHandle_t m_taskHandle = nullptr;
    SemaphoreHandle_t m_mutex = nullptr;
};

} // namespace audio
} // namespace cbdos
