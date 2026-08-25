#pragma once

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

namespace cbdos {
namespace media {

enum class VideoCodecType {
    Unknown,
    MJPEG,
    H264
};

enum class AudioCodecType {
    None,
    PCM_WAV,
    MP3
};

struct AviStreamInfo {
    uint32_t width{0};
    uint32_t height{0};
    float fps{0.0f};
    uint32_t microSecPerFrame{0};
    uint32_t totalFrames{0};
    VideoCodecType videoCodec{VideoCodecType::Unknown};
    
    bool hasAudio{false};
    AudioCodecType audioCodec{AudioCodecType::None};
    uint32_t audioSampleRate{0};
    uint16_t audioChannels{0};
    uint16_t audioBitsPerSample{0};
    uint32_t audioBlockAlign{0};
};

struct AviChunk {
    uint32_t fourcc{0};
    size_t size{0};
    long fileOffset{0};
    bool isVideo{false};
    bool isAudio{false};
};

class AviParser {
public:
    AviParser();
    ~AviParser();

    bool open(const std::string& filepath);
    void close();

    bool isOpened() const { return m_file != nullptr; }
    const AviStreamInfo& getInfo() const { return m_info; }

    bool readNextChunkHeader(AviChunk& chunk);
    size_t readChunkData(const AviChunk& chunk, uint8_t* buffer, size_t bufferSize);
    bool skipChunk(const AviChunk& chunk);

    // Decodifica el siguiente frame de video (MJPEG) y extrae audio intercalado
    bool decodeNextVideoFrame(uint8_t* outRgb565, uint32_t outWidth, uint32_t outHeight,
                              uint8_t* outAudioBuf = nullptr, size_t maxAudioSize = 0, size_t* outAudioBytes = nullptr);

    bool seekToFrame(uint32_t frameIndex);
    bool rewindToMovi();

    uint32_t getCurrentFrameIndex() const { return m_currentFrameIndex; }

private:
    FILE* m_file{nullptr};
    std::string m_filepath;
    AviStreamInfo m_info;
    
    long m_moviStartOffset{0};
    long m_moviEndOffset{0};
    uint32_t m_currentFrameIndex{0};

    bool parseRiffHeaders();
    bool parseAvih(size_t size);
    bool parseStrl(size_t listEndOffset);
    bool parseStrh(size_t size, uint32_t& streamType, uint32_t& handler);
    bool parseStrf(size_t size, uint32_t streamType);
};

} // namespace media
} // namespace cbdos
