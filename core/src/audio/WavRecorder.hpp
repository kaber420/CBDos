#pragma once

#include "cbdos/audio.hpp"
#include <string>
#include <cstdio>
#include <cstdint>
#include "cbdos/rtos.hpp"

namespace cbdos {
namespace audio {

class WavRecorder {
public:
    static WavRecorder& getInstance();

    bool start(const char* filepath, const RecordConfig& cfg = RecordConfig());
    void pause();
    void resume();
    void stop();

    bool isRecording() const { return m_isRecording; }
    bool isPaused() const { return m_isPaused; }
    uint32_t getDurationMs() const { return m_durationMs; }
    float getPeakLevel() const { return m_peakLevel; }
    const std::string& getFilePath() const { return m_filePath; }

private:
    WavRecorder();
    ~WavRecorder();
    WavRecorder(const WavRecorder&) = delete;
    WavRecorder& operator=(const WavRecorder&) = delete;

    static void recordingTask(void* param);
    void runRecording();
    bool writeWavHeader(FILE* f, const RecordConfig& cfg, uint32_t dataBytes);

    RecordConfig m_config;
    std::string m_filePath;
    FILE* m_file = nullptr;

    volatile bool m_isRecording = false;
    volatile bool m_isPaused = false;
    volatile bool m_stopRequested = false;
    volatile uint32_t m_durationMs = 0;
    volatile float m_peakLevel = 0.0f;
    volatile uint32_t m_totalDataBytesWritten = 0;

    cbdos::rtos::TaskHandle m_taskHandle = nullptr;
    cbdos::rtos::MutexHandle m_mutex = nullptr;
};

} // namespace audio
} // namespace cbdos
