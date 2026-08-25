#include "cbdos/media/AviParser.hpp"
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

} // namespace media
} // namespace cbdos
