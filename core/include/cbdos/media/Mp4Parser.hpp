#pragma once

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

namespace cbdos {
namespace media {

struct Mp4StreamInfo {
    uint32_t width{0};
    uint32_t height{0};
    float fps{30.0f};
    uint32_t totalVideoSamples{0};
    uint32_t durationSec{0};

    bool hasAudio{false};
    uint32_t audioSampleRate{44100};
    uint16_t audioChannels{2};
    uint32_t audioBitrate{96000};
    uint32_t totalAudioSamples{0};
};

class Mp4Parser {
public:
    Mp4Parser();
    ~Mp4Parser();

    bool open(const std::string& filepath);
    void close();

    bool isOpened() const { return m_file != nullptr; }
    const Mp4StreamInfo& getInfo() const { return m_info; }

    uint32_t getCurrentVideoSample() const { return m_currentVideoSample; }
    uint32_t getCurrentAudioSample() const { return m_currentAudioSample; }

    // Decodifica el siguiente fotograma H.264 directamente a búfer RGB565
    bool decodeNextVideoFrame(uint8_t* outRgb565, uint32_t outWidth, uint32_t outHeight);

    // Lee la siguiente trama de audio AAC (con cabecera ADTS lista para Helix AAC)
    bool readNextAudioFrame(uint8_t* outBuffer, size_t maxBufferSize, size_t& outFrameSize);

    // Decodifica la siguiente trama de audio AAC a PCM estéreo 16 bits (para enviar a I2S)
    bool decodeNextAudioPcm(int16_t* outPcm, size_t maxSamples, size_t& outSamples);

    bool seekToSample(uint32_t sampleIndex);

private:
    FILE* m_file{nullptr};
    std::string m_filepath;
    Mp4StreamInfo m_info;

    void* m_demuxer{nullptr};
    int m_videoTrack{-1};
    int m_audioTrack{-1};
    uint32_t m_currentVideoSample{0};
    uint32_t m_currentAudioSample{0};

    void* m_h264Decoder{nullptr}; // h264bsd storage
    uint8_t* m_nalBuffer{nullptr};
    size_t m_nalBufferSize{0};

    void* m_aacDecoder{nullptr};  // Helix HAACDecoder
    uint8_t* m_aacRawBuf{nullptr};
    size_t m_aacRawBufSize{0};

    bool initH264Decoder();
    void cleanupH264Decoder();
    bool initAacDecoder();
    void cleanupAacDecoder();
    void yuv420ToRgb565(const uint8_t* y, const uint8_t* u, const uint8_t* v,
                        uint32_t srcWidth, uint32_t srcHeight,
                        uint8_t* dstRgb565, uint32_t dstWidth, uint32_t dstHeight);
};

} // namespace media
} // namespace cbdos
