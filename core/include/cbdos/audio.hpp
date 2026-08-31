#pragma once
#include <cstdint>
#include <cstddef>

namespace cbdos {
namespace audio {

enum class CodecType {
    None,
    MP3,
    AAC,
    WAV
};

struct AudioStats {
    bool isPlaying;
    CodecType codec;
    uint32_t sampleRate;
    uint8_t channels;
    uint32_t bitRate;
    uint8_t bufferPercent;
};

// Grabación de Audio y Captura de Micrófono
struct RecordConfig {
    uint32_t sampleRate = 16000; // 16 kHz recomendado para voz o 44100 Hz
    uint8_t channels = 1;        // 1 = Mono, 2 = Estéreo
    uint8_t bitsPerSample = 16;  // 16-bit PCM
    uint8_t micGainDb = 24;      // Ganancia de entrada analógica de micrófono (0 a 30 dB)
};

// ────────────────────────────────────────────────────────────────
// Contratos HAL C++ Puros
// ────────────────────────────────────────────────────────────────

class IAudioSink {
public:
    virtual ~IAudioSink() = default;

    virtual bool init(uint32_t sampleRate = 44100, uint8_t channels = 2, uint8_t bitsPerSample = 16) = 0;
    virtual void deinit() = 0;
    virtual size_t write(const void* pcmData, size_t sizeBytes, uint32_t timeoutMs = 1000) = 0;
    virtual void setVolume(uint8_t volumePercent) = 0;
    virtual uint8_t getVolume() const = 0;
    virtual bool setSampleRate(uint32_t sampleRate) = 0;
    virtual void mute(bool enable) = 0;
    virtual bool isMuted() const = 0;
    virtual void playTone(uint32_t freqHz, uint32_t durationMs) = 0;
    virtual void playBeep() { playTone(1200, 80); }
};

class IAudioSource {
public:
    virtual ~IAudioSource() = default;

    virtual bool init(const RecordConfig& cfg) = 0;
    virtual void deinit() = 0;
    virtual size_t read(void* dest, size_t sizeBytes, uint32_t timeoutMs = 100) = 0;
    virtual void setMicGain(uint8_t gainDb) = 0;
    virtual float getPeakLevel() = 0;
};

void setAudioSink(IAudioSink* sink);
IAudioSink* getAudioSink();

void setAudioSource(IAudioSource* source);
IAudioSource* getAudioSource();

// ────────────────────────────────────────────────────────────────
// APIs públicas de CBDos (Consumidas por Vistas, Apps y Lua)
// ────────────────────────────────────────────────────────────────

bool init();
bool playStream(const char* url);
bool playFile(const char* filepath);
void stop();
void pause();
void resume();
void setVolume(uint8_t volumePercent);
uint8_t getVolume();
bool setSampleRate(uint32_t sampleRate);
void playTone(uint32_t freqHz = 1000, uint32_t durationMs = 100);
void playBeep();
void seek(uint32_t seconds);
uint32_t getCurrentTimeSec();
uint32_t getTotalTimeSec();
bool writeAudio(const void* src, size_t size);
AudioStats getStats();

// Grabación
bool recordStart(const char* targetFilePath, const RecordConfig& cfg = RecordConfig());
void recordPause();
void recordResume();
void recordStop();
bool isRecording();
bool isRecordPaused();
uint32_t getRecordDurationMs();
float getMicPeakLevel();
size_t readAudio(void* dest, size_t sizeBytes, uint32_t timeoutMs = 100);

} // namespace audio
} // namespace cbdos

