#pragma once

#include <cstdint>
#include <cstddef>
#include <esp_err.h>
#include <driver/i2s_std.h>
#include <driver/i2c_master.h>
#include "esp_codec_dev.h"

#define BOARD_AUDIO_I2S_PORT       I2S_NUM_0
#define BOARD_AUDIO_MCLK_GPIO      13
#define BOARD_AUDIO_BCLK_GPIO      12
#define BOARD_AUDIO_WS_GPIO        10
#define BOARD_AUDIO_DOUT_GPIO      9
#define BOARD_AUDIO_DIN_GPIO       48
#define BOARD_AUDIO_PA_GPIO        11
#define BOARD_AUDIO_CODEC_ADDR     0x30

class AudioHAL {
public:
    static AudioHAL& getInstance() {
        static AudioHAL instance;
        return instance;
    }

    esp_err_t init(uint32_t sampleRate = 44100);
    esp_err_t setSampleRate(uint32_t sampleRate);
    
    void setVolume(uint8_t volumePercent);
    uint8_t getVolume() const { return currentVolume; }
    
    void mute(bool enable);
    bool isMuted() const { return muted; }

    esp_err_t writeAudio(const void* src, size_t size, size_t* bytesWritten, uint32_t timeoutMs = 1000);
    
    void playTone(uint32_t freqHz, uint32_t durationMs);
    void playBeep();
    void playStartupChime();

    bool isInitialized() const { return initialized; }

private:
    AudioHAL();
    ~AudioHAL();

    AudioHAL(const AudioHAL&) = delete;
    AudioHAL& operator=(const AudioHAL&) = delete;

    esp_codec_dev_handle_t playDevHandle = nullptr;
    
    uint8_t currentVolume = 70;
    bool muted = false;
    bool initialized = false;
    uint32_t currentSampleRate = 44100;
};
