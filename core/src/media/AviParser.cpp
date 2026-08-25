#include "cbdos/media/AviParser.hpp"
#include "src/libs/tjpgd/tjpgd.h"
#include <cstring>
#include <algorithm>

#define FOURCC(a, b, c, d) \
    ((uint32_t)(uint8_t)(a) | ((uint32_t)(uint8_t)(b) << 8) | \
     ((uint32_t)(uint8_t)(c) << 16) | ((uint32_t)(uint8_t)(d) << 24))

namespace cbdos {
namespace media {

static const uint32_t FCC_RIFF = FOURCC('R', 'I', 'F', 'F');
static const uint32_t FCC_AVI  = FOURCC('A', 'V', 'I', ' ');
static const uint32_t FCC_LIST = FOURCC('L', 'I', 'S', 'T');
static const uint32_t FCC_HDRL = FOURCC('h', 'd', 'r', 'l');
static const uint32_t FCC_AVIH = FOURCC('a', 'v', 'i', 'h');
static const uint32_t FCC_STRL = FOURCC('s', 't', 'r', 'l');
static const uint32_t FCC_STRH = FOURCC('s', 't', 'r', 'h');
static const uint32_t FCC_STRF = FOURCC('s', 't', 'r', 'f');
static const uint32_t FCC_VIDS = FOURCC('v', 'i', 'd', 's');
static const uint32_t FCC_AUDS = FOURCC('a', 'u', 'd', 's');
static const uint32_t FCC_MJPG = FOURCC('M', 'J', 'P', 'G');
static const uint32_t FCC_mjpg = FOURCC('m', 'j', 'p', 'g');
static const uint32_t FCC_JPEG = FOURCC('J', 'P', 'E', 'G');
static const uint32_t FCC_jpeg = FOURCC('j', 'p', 'e', 'g');
static const uint32_t FCC_MOVI = FOURCC('m', 'o', 'v', 'i');

#pragma pack(push, 1)
struct MainAVIHeaderRaw {
    uint32_t dwMicroSecPerFrame;
    uint32_t dwMaxBytesPerSec;
    uint32_t dwPaddingGranularity;
    uint32_t dwFlags;
    uint32_t dwTotalFrames;
    uint32_t dwInitialFrames;
    uint32_t dwStreams;
    uint32_t dwSuggestedBufferSize;
    uint32_t dwWidth;
    uint32_t dwHeight;
    uint32_t dwReserved[4];
};

struct AVIStreamHeaderRaw {
    uint32_t fccType;
    uint32_t fccHandler;
    uint32_t dwFlags;
    uint16_t wPriority;
    uint16_t wLanguage;
    uint32_t dwInitialFrames;
    uint32_t dwScale;
    uint32_t dwRate;
    uint32_t dwStart;
    uint32_t dwLength;
    uint32_t dwSuggestedBufferSize;
    uint32_t dwQuality;
    uint32_t dwSampleSize;
    struct {
        int16_t left;
        int16_t top;
        int16_t right;
        int16_t bottom;
    } rcFrame;
};

struct BitmapInfoHeaderRaw {
    uint32_t biSize;
    int32_t  biWidth;
    int32_t  biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t  biXPelsPerMeter;
    int32_t  biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
};

struct WaveFormatExRaw {
    uint16_t wFormatTag;
    uint16_t nChannels;
    uint32_t nSamplesPerSec;
    uint32_t nAvgBytesPerSec;
    uint16_t nBlockAlign;
    uint16_t wBitsPerSample;
    uint16_t cbSize;
};
#pragma pack(pop)

AviParser::AviParser() = default;

AviParser::~AviParser() {
    close();
}

bool AviParser::open(const std::string& filepath) {
    close();
    m_filepath = filepath;
    m_file = fopen(filepath.c_str(), "rb");
    if (!m_file) {
        return false;
    }
    setvbuf(m_file, nullptr, _IOFBF, 64 * 1024);

    if (!parseRiffHeaders()) {
        close();
        return false;
    }

    return true;
}

void AviParser::close() {
    if (m_file) {
        fclose(m_file);
        m_file = nullptr;
    }
    m_info = AviStreamInfo{};
    m_moviStartOffset = 0;
    m_moviEndOffset = 0;
    m_currentFrameIndex = 0;
}

bool AviParser::parseRiffHeaders() {
    if (!m_file) return false;

    uint32_t riffFourCC = 0;
    uint32_t riffSize = 0;
    uint32_t aviFourCC = 0;

    if (fread(&riffFourCC, 1, 4, m_file) != 4 ||
        fread(&riffSize, 1, 4, m_file) != 4 ||
        fread(&aviFourCC, 1, 4, m_file) != 4) {
        return false;
    }

    if (riffFourCC != FCC_RIFF || aviFourCC != FCC_AVI) {
        return false;
    }

    // Traverse chunks until 'movi' list is found
    bool foundMovi = false;
    while (!feof(m_file) && !foundMovi) {
        uint32_t chunkType = 0;
        uint32_t chunkSize = 0;
        if (fread(&chunkType, 1, 4, m_file) != 4 || fread(&chunkSize, 1, 4, m_file) != 4) {
            break;
        }

        if (chunkType == FCC_LIST) {
            uint32_t listType = 0;
            if (fread(&listType, 1, 4, m_file) != 4) break;

            long listEndOffset = ftell(m_file) - 4 + chunkSize;

            if (listType == FCC_HDRL) {
                // Parse headers inside hdrl
                while (ftell(m_file) < listEndOffset) {
                    uint32_t subType = 0;
                    uint32_t subSize = 0;
                    if (fread(&subType, 1, 4, m_file) != 4 || fread(&subSize, 1, 4, m_file) != 4) break;

                    if (subType == FCC_AVIH) {
                        parseAvih(subSize);
                    } else if (subType == FCC_LIST) {
                        uint32_t subListType = 0;
                        if (fread(&subListType, 1, 4, m_file) != 4) break;
                        long subListEnd = ftell(m_file) - 4 + subSize;
                        if (subListType == FCC_STRL) {
                            parseStrl(subListEnd);
                        } else {
                            fseek(m_file, subListEnd, SEEK_SET);
                        }
                    } else {
                        long pad = (subSize % 2 != 0) ? 1 : 0;
                        fseek(m_file, subSize + pad, SEEK_CUR);
                    }
                }
            } else if (listType == FCC_MOVI) {
                m_moviStartOffset = ftell(m_file);
                m_moviEndOffset = listEndOffset;
                foundMovi = true;
                break;
            } else {
                fseek(m_file, listEndOffset, SEEK_SET);
            }
        } else {
            long pad = (chunkSize % 2 != 0) ? 1 : 0;
            fseek(m_file, chunkSize + pad, SEEK_CUR);
        }
    }

    if (foundMovi) {
        rewindToMovi();
        return true;
    }

    return false;
}

bool AviParser::parseAvih(size_t size) {
    MainAVIHeaderRaw avih{};
    size_t toRead = std::min(size, sizeof(avih));
    if (fread(&avih, 1, toRead, m_file) != toRead) return false;

    if (size > toRead) {
        fseek(m_file, size - toRead, SEEK_CUR);
    }
    if (size % 2 != 0) fseek(m_file, 1, SEEK_CUR);

    m_info.microSecPerFrame = avih.dwMicroSecPerFrame;
    if (avih.dwMicroSecPerFrame > 0) {
        m_info.fps = 1000000.0f / (float)avih.dwMicroSecPerFrame;
    }
    m_info.totalFrames = avih.dwTotalFrames;
    m_info.width = avih.dwWidth;
    m_info.height = avih.dwHeight;

    return true;
}

bool AviParser::parseStrl(size_t listEndOffset) {
    uint32_t streamType = 0;
    uint32_t handler = 0;

    while (ftell(m_file) < (long)listEndOffset) {
        uint32_t chunkType = 0;
        uint32_t chunkSize = 0;
        if (fread(&chunkType, 1, 4, m_file) != 4 || fread(&chunkSize, 1, 4, m_file) != 4) break;

        if (chunkType == FCC_STRH) {
            parseStrh(chunkSize, streamType, handler);
        } else if (chunkType == FCC_STRF) {
            parseStrf(chunkSize, streamType);
        } else {
            long pad = (chunkSize % 2 != 0) ? 1 : 0;
            fseek(m_file, chunkSize + pad, SEEK_CUR);
        }
    }

    return true;
}

bool AviParser::parseStrh(size_t size, uint32_t& streamType, uint32_t& handler) {
    AVIStreamHeaderRaw strh{};
    size_t toRead = std::min(size, sizeof(strh));
    if (fread(&strh, 1, toRead, m_file) != toRead) return false;

    if (size > toRead) fseek(m_file, size - toRead, SEEK_CUR);
    if (size % 2 != 0) fseek(m_file, 1, SEEK_CUR);

    streamType = strh.fccType;
    handler = strh.fccHandler;

    if (streamType == FCC_VIDS) {
        if (strh.dwScale > 0 && strh.dwRate > 0) {
            m_info.fps = (float)strh.dwRate / (float)strh.dwScale;
        }
        if (strh.dwLength > 0 && m_info.totalFrames == 0) {
            m_info.totalFrames = strh.dwLength;
        }
    }

    return true;
}

bool AviParser::parseStrf(size_t size, uint32_t streamType) {
    if (streamType == FCC_VIDS) {
        BitmapInfoHeaderRaw bih{};
        size_t toRead = std::min(size, sizeof(bih));
        if (fread(&bih, 1, toRead, m_file) != toRead) return false;

        if (bih.biWidth > 0) m_info.width = bih.biWidth;
        if (bih.biHeight > 0) m_info.height = bih.biHeight;

        if (bih.biCompression == FCC_MJPG || bih.biCompression == FCC_mjpg ||
            bih.biCompression == FCC_JPEG || bih.biCompression == FCC_jpeg) {
            m_info.videoCodec = VideoCodecType::MJPEG;
        } else {
            m_info.videoCodec = VideoCodecType::Unknown;
        }

        if (size > toRead) fseek(m_file, size - toRead, SEEK_CUR);
    } else if (streamType == FCC_AUDS) {
        WaveFormatExRaw wfx{};
        size_t toRead = std::min(size, sizeof(wfx));
        if (fread(&wfx, 1, toRead, m_file) != toRead) return false;

        m_info.hasAudio = true;
        m_info.audioChannels = wfx.nChannels;
        m_info.audioSampleRate = wfx.nSamplesPerSec;
        m_info.audioBitsPerSample = wfx.wBitsPerSample;
        m_info.audioBlockAlign = wfx.nBlockAlign;

        if (wfx.wFormatTag == 1) {
            m_info.audioCodec = AudioCodecType::PCM_WAV;
        } else if (wfx.wFormatTag == 0x55) {
            m_info.audioCodec = AudioCodecType::MP3;
        }

        if (size > toRead) fseek(m_file, size - toRead, SEEK_CUR);
    } else {
        fseek(m_file, size, SEEK_CUR);
    }

    if (size % 2 != 0) fseek(m_file, 1, SEEK_CUR);
    return true;
}

bool AviParser::rewindToMovi() {
    if (!m_file || m_moviStartOffset <= 0) return false;
    fseek(m_file, m_moviStartOffset, SEEK_SET);
    m_currentFrameIndex = 0;
    return true;
}

bool AviParser::readNextChunkHeader(AviChunk& chunk) {
    if (!m_file) return false;

    while (!feof(m_file)) {
        long currentPos = ftell(m_file);
        if (m_moviEndOffset > 0 && currentPos >= m_moviEndOffset) {
            return false;
        }

        uint32_t fourcc = 0;
        uint32_t size = 0;
        if (fread(&fourcc, 1, 4, m_file) != 4 || fread(&size, 1, 4, m_file) != 4) {
            return false;
        }

        chunk.fourcc = fourcc;
        chunk.size = size;
        chunk.fileOffset = ftell(m_file);

        // Check if chunk is video: '00dc', '00db', '01dc'
        char* fccStr = (char*)&fourcc;
        if ((fccStr[0] == '0' && (fccStr[2] == 'd' || fccStr[2] == 'D') && (fccStr[3] == 'c' || fccStr[3] == 'b' || fccStr[3] == 'C' || fccStr[3] == 'B'))) {
            chunk.isVideo = true;
            chunk.isAudio = false;
            m_currentFrameIndex++;
            return true;
        }

        // Check if chunk is audio: '01wb', '00wb'
        if ((fccStr[2] == 'w' || fccStr[2] == 'W') && (fccStr[3] == 'b' || fccStr[3] == 'B')) {
            chunk.isVideo = false;
            chunk.isAudio = true;
            return true;
        }

        // Skip other chunks like 'JUNK' or rec lists
        long pad = (size % 2 != 0) ? 1 : 0;
        fseek(m_file, size + pad, SEEK_CUR);
    }

    return false;
}

size_t AviParser::readChunkData(const AviChunk& chunk, uint8_t* buffer, size_t bufferSize) {
    if (!m_file || !buffer || bufferSize == 0) return 0;

    fseek(m_file, chunk.fileOffset, SEEK_SET);
    size_t toRead = std::min(chunk.size, bufferSize);
    size_t read = fread(buffer, 1, toRead, m_file);

    // Skip to next chunk boundary (align to 2 bytes)
    long pad = (chunk.size % 2 != 0) ? 1 : 0;
    fseek(m_file, chunk.fileOffset + chunk.size + pad, SEEK_SET);

    return read;
}

bool AviParser::skipChunk(const AviChunk& chunk) {
    if (!m_file) return false;
    long pad = (chunk.size % 2 != 0) ? 1 : 0;
    return fseek(m_file, chunk.fileOffset + chunk.size + pad, SEEK_SET) == 0;
}

bool AviParser::seekToFrame(uint32_t frameIndex) {
    if (!m_file || m_moviStartOffset <= 0) return false;

    if (frameIndex < m_currentFrameIndex) {
        if (!rewindToMovi()) return false;
    }

    while (m_currentFrameIndex < frameIndex) {
        AviChunk chunk;
        if (!readNextChunkHeader(chunk)) {
            return false;
        }
        skipChunk(chunk);
    }
    return true;
}

struct AviJpegState {
    const uint8_t* src;
    size_t size;
    size_t offset;
    uint16_t* dst;
    uint32_t dstWidth;
    uint32_t dstHeight;
    uint32_t offsetX;
    uint32_t offsetY;
};

static size_t avi_jd_input(JDEC* jd, uint8_t* buff, size_t nbyte) {
    auto* st = static_cast<AviJpegState*>(jd->device);
    if (!st || st->offset >= st->size) return 0;
    size_t toRead = std::min(nbyte, st->size - st->offset);
    if (buff) {
        memcpy(buff, st->src + st->offset, toRead);
    }
    st->offset += toRead;
    return toRead;
}

static int avi_jd_output(JDEC* jd, void* bitmap, JRECT* rect) {
    auto* st = static_cast<AviJpegState*>(jd->device);
    if (!st || !st->dst || !bitmap || !rect) return 0;

    const uint8_t* srcRgb888 = static_cast<const uint8_t*>(bitmap);
    uint32_t blockW = rect->right - rect->left + 1;
    uint32_t blockH = rect->bottom - rect->top + 1;

    for (uint32_t y = 0; y < blockH; y++) {
        uint32_t dstY = st->offsetY + rect->top + y;
        if (dstY >= st->dstHeight) break;

        uint32_t dstX = st->offsetX + rect->left;
        if (dstX >= st->dstWidth) continue;

        uint16_t* dstRow = st->dst + (dstY * st->dstWidth) + dstX;
        const uint8_t* srcRow = srcRgb888 + (y * blockW * 3);
        uint32_t copyW = std::min(blockW, (st->dstWidth > dstX) ? (st->dstWidth - dstX) : 0);

        for (uint32_t x = 0; x < copyW; x++) {
            uint8_t r = srcRow[x * 3 + 0];
            uint8_t g = srcRow[x * 3 + 1];
            uint8_t b = srcRow[x * 3 + 2];
            dstRow[x] = (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
        }
    }
    return 1;
}

bool AviParser::decodeNextVideoFrame(uint8_t* outRgb565, uint32_t outWidth, uint32_t outHeight,
                                     uint8_t* outAudioBuf, size_t maxAudioSize, size_t* outAudioBytes) {
    if (outAudioBytes) *outAudioBytes = 0;
    if (!m_file || !outRgb565 || outWidth == 0 || outHeight == 0) return false;

    AviChunk chunk;
    while (readNextChunkHeader(chunk)) {
        if (chunk.isAudio) {
            if (outAudioBuf && maxAudioSize > 0) {
                size_t toRead = std::min(chunk.size, maxAudioSize);
                size_t read = readChunkData(chunk, outAudioBuf, toRead);
                if (outAudioBytes) *outAudioBytes = read;
            } else {
                skipChunk(chunk);
            }
        } else if (chunk.isVideo) {
            static std::vector<uint8_t> s_jpegBuf;
            if (s_jpegBuf.size() < chunk.size) {
                s_jpegBuf.resize(chunk.size + 2048);
            }

            size_t bytesRead = readChunkData(chunk, s_jpegBuf.data(), s_jpegBuf.size());
            if (bytesRead == 0) return false;

            AviJpegState state;
            state.src = s_jpegBuf.data();
            state.size = bytesRead;
            state.offset = 0;
            state.dst = (uint16_t*)outRgb565;
            state.dstWidth = outWidth;
            state.dstHeight = outHeight;

            uint32_t renderW = outWidth;
            uint32_t renderH = (m_info.width > 0) ? (outWidth * m_info.height) / m_info.width : outHeight;
            if (renderH > outHeight && m_info.height > 0) {
                renderH = outHeight;
                renderW = (outHeight * m_info.width) / m_info.height;
            }
            state.offsetX = (outWidth > renderW) ? (outWidth - renderW) / 2 : 0;
            state.offsetY = (outHeight > renderH) ? (outHeight - renderH) / 2 : 0;

            static uint8_t s_pool[4096];
            JDEC jdec;
            JRESULT res = jd_prepare(&jdec, avi_jd_input, s_pool, sizeof(s_pool), &state);
            if (res != JDR_OK) return false;

            res = jd_decomp(&jdec, avi_jd_output, 0);
            return (res == JDR_OK);
        } else {
            skipChunk(chunk);
        }
    }
    return false;
}

} // namespace media
} // namespace cbdos

