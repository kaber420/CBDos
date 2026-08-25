#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

namespace cbdos {
namespace video {

enum class VideoState {
    Stopped,
    Playing,
    Paused,
    Completed,
    Error
};

struct VideoStats {
    VideoState state{VideoState::Stopped};
    std::string currentFile;
    uint32_t width{0};
    uint32_t height{0};
    float fps{0.0f};
    uint32_t currentFrame{0};
    uint32_t totalFrames{0};
    uint32_t currentTimeSec{0};
    uint32_t totalTimeSec{0};
    bool hasAudio{false};
    uint8_t volume{80};
};

bool init();
bool play(const char* filepath);
void pause();
void resume();
void stop();
void togglePlayPause();
bool isPlaying();
void setVolume(uint8_t volumePercent);
uint8_t getVolume();
VideoStats getStats();

} // namespace video
} // namespace cbdos
