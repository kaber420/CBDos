#pragma once

#include "cbdos/audio.hpp"
#include <string>
#include <vector>
#include <cstdio>
#include "cbdos/rtos.hpp"

namespace cbdos {
namespace audio {

#pragma pack(push, 1)
struct WavHeader {
    char riff[4];           // "RIFF"
    uint32_t fileSize;      // Total file size - 8
    char wave[4];           // "WAVE"
    char fmt[4];            // "fmt "
    uint32_t fmtSize;       // Format chunk size (16 for PCM)
    uint16_t audioFormat;   // 1 = PCM
    uint16_t numChannels;   // 1 = Mono, 2 = Stereo
    uint32_t sampleRate;    // 44100, 48000, etc.
    uint32_t byteRate;      // sampleRate * numChannels * bitsPerSample / 8
    uint16_t blockAlign;    // numChannels * bitsPerSample / 8
    uint16_t bitsPerSample; // 16
    char data[4];           // "data"
    uint32_t dataSize;      // Size of audio PCM data
};
#pragma pack(pop)

class WavPlayer {
public:
    static WavPlayer& getInstance();

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
    
    AudioStats getStats() const;

private:
    WavPlayer();
    ~WavPlayer();
    WavPlayer(const WavPlayer&) = delete;
    WavPlayer& operator=(const WavPlayer&) = delete;

    static void playbackTask(void* param);
    void runPlayback();

    FILE* m_file = nullptr;
    std::string m_currentFile;
    
    WavHeader m_header = {};
    uint32_t m_dataStartOffset = 44;
    uint32_t m_totalTimeSec = 0;
    volatile uint32_t m_bytesRead = 0;
    
    volatile bool m_isPlaying = false;
    volatile bool m_isPaused = false;
    volatile bool m_stopRequested = false;
    volatile int32_t m_seekRequestSec = -1;
    
    cbdos::rtos::TaskHandle m_taskHandle = nullptr;
    cbdos::rtos::MutexHandle m_mutex = nullptr;
};

} // namespace audio
} // namespace cbdos
