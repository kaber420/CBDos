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
AudioStats getStats();

} // namespace audio
} // namespace cbdos
