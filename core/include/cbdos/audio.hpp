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

// Grabación de Audio y Captura de Micrófono
struct RecordConfig {
    uint32_t sampleRate = 16000; // 16 kHz recomendado para voz o 44100 Hz
    uint8_t channels = 1;        // 1 = Mono, 2 = Estéreo
    uint8_t bitsPerSample = 16;  // 16-bit PCM
    uint8_t micGainDb = 24;      // Ganancia de entrada analógica de micrófono (0 a 30 dB)
};

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
