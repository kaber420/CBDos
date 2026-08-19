#pragma once
#include <cstdint>
#include <cstddef>

namespace cbdos {
namespace display {

struct DisplayCaps {
    uint16_t width;
    uint16_t height;
    bool hasHardware2D;     // true si cuenta con PPA / DMA2D / GPU 2D
    uint8_t targetFps;      // 60 en P4, 30 en S3
    bool isTouchSupported;
};

bool init();
DisplayCaps getCapabilities();
void setBrightness(uint8_t percent);
uint8_t getBrightness();
void* getFramebuffer(int index);

} // namespace display
} // namespace cbdos
