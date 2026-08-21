#include "cbdos/audio.hpp"
#include "AudioPlayer.hpp"
#include <Arduino.h>
#include <driver/i2s.h>
#include <cmath>

namespace cbdos {
namespace audio {

static bool s_i2s_installed = false;
static uint32_t s_currentSampleRate = 44100;
static uint8_t s_volume = 75;

bool init() {
    if (s_i2s_installed) return true;

    i2s_config_t cfg = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate          = 44100,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count        = 16,
        .dma_buf_len          = 1024,
        .use_apll             = false,
        .tx_desc_auto_clear   = true,
        .fixed_mclk           = 0
    };

    i2s_pin_config_t pins = {
        .bck_io_num   = 42,
        .ws_io_num    = 2,
        .data_out_num = 41,
        .data_in_num  = I2S_PIN_NO_CHANGE
    };

    if (i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL) != ESP_OK) {
        Serial.println("[AudioHAL-S3] Error instalando driver I2S");
        return false;
    }
    if (i2s_set_pin(I2S_NUM_0, &pins) != ESP_OK) {
        Serial.println("[AudioHAL-S3] Error configurando pines I2S");
        return false;
    }
    i2s_zero_dma_buffer(I2S_NUM_0);
    s_i2s_installed = true;
    Serial.println("[AudioHAL-S3] I2S listo en GPIOs BCK=42, WS=2, DOUT=41 a 44100 Hz");
    return true;
}

bool playStream(const char* url) {
    if (!s_i2s_installed) init();
    return AudioPlayer::getInstance().playStream(url);
}

bool playFile(const char* filepath) {
    if (!s_i2s_installed) init();
    return AudioPlayer::getInstance().play(filepath);
}

void stop() {
    AudioPlayer::getInstance().stop();
    if (s_i2s_installed) {
        i2s_zero_dma_buffer(I2S_NUM_0);
    }
}

void pause() {
    AudioPlayer::getInstance().pause();
}

void resume() {
    AudioPlayer::getInstance().resume();
}

void setVolume(uint8_t volumePercent) {
    if (volumePercent > 100) volumePercent = 100;
    s_volume = volumePercent;
}

uint8_t getVolume() {
    return s_volume;
}

bool setSampleRate(uint32_t sampleRate) {
    if (!s_i2s_installed) init();
    if (s_currentSampleRate == sampleRate) return true;
    s_currentSampleRate = sampleRate;
    return i2s_set_sample_rates(I2S_NUM_0, sampleRate) == ESP_OK;
}

void playTone(uint32_t freqHz, uint32_t durationMs) {
    if (!s_i2s_installed) init();
    if (freqHz == 0 || durationMs == 0) return;

    const uint32_t sampleRate = s_currentSampleRate ? s_currentSampleRate : 44100;
    uint32_t totalSamples = (sampleRate * durationMs) / 1000;
    const size_t CHUNK = 256;
    int16_t buffer[CHUNK * 2]; // Estéreo

    float phase = 0.0f;
    float phaseInc = (2.0f * M_PI * freqHz) / (float)sampleRate;
    int32_t volFactor = (int32_t)s_volume;
    float amplitude = (32767.0f * volFactor) / 100.0f;

    size_t written = 0;
    for (uint32_t i = 0; i < totalSamples; i += CHUNK) {
        size_t samplesThisChunk = (totalSamples - i > CHUNK) ? CHUNK : (totalSamples - i);
        for (size_t s = 0; s < samplesThisChunk; ++s) {
            int16_t val = (int16_t)(sinf(phase) * amplitude);
            buffer[s * 2]     = val;
            buffer[s * 2 + 1] = val;
            phase += phaseInc;
            if (phase >= 2.0f * M_PI) phase -= 2.0f * M_PI;
        }
        i2s_write(I2S_NUM_0, buffer, samplesThisChunk * 2 * sizeof(int16_t), &written, portMAX_DELAY);
    }
}

void playBeep() {
    playTone(1200, 80);
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

bool writeAudio(const void* src, size_t size) {
    if (!s_i2s_installed) {
        if (!init()) return false;
    }
    if (!src || size == 0) return true;

    size_t bytesWritten = 0;
    if (s_volume >= 100) {
        return i2s_write(I2S_NUM_0, src, size, &bytesWritten, portMAX_DELAY) == ESP_OK;
    }

    if (s_volume == 0) {
        uint8_t zeroBuf[256] = {0};
        size_t left = size;
        while (left > 0) {
            size_t chunk = left > sizeof(zeroBuf) ? sizeof(zeroBuf) : left;
            i2s_write(I2S_NUM_0, zeroBuf, chunk, &bytesWritten, portMAX_DELAY);
            left -= chunk;
        }
        return true;
    }

    // Escalado de volumen por software para 16-bit PCM estéreo
    int16_t scaledBuf[256];
    const int16_t* inSamples = (const int16_t*)src;
    size_t sampleCount = size / sizeof(int16_t);
    size_t offset = 0;
    int32_t volFactor = (int32_t)s_volume;

    while (offset < sampleCount) {
        size_t chunk = (sampleCount - offset > 256) ? 256 : (sampleCount - offset);
        for (size_t i = 0; i < chunk; ++i) {
            scaledBuf[i] = (int16_t)(((int32_t)inSamples[offset + i] * volFactor) / 100);
        }
        i2s_write(I2S_NUM_0, scaledBuf, chunk * sizeof(int16_t), &bytesWritten, portMAX_DELAY);
        offset += chunk;
    }

    return true;
}

AudioStats getStats() {
    return AudioPlayer::getInstance().getStats();
}

} // namespace audio
} // namespace cbdos
