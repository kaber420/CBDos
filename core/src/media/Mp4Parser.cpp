#include "cbdos/media/Mp4Parser.hpp"
#include <cstring>
#include <algorithm>
#include <vector>
#include <esp_log.h>

#if defined(ESP_PLATFORM)
#include <esp_heap_caps.h>
#define ALLOC_MEM(sz) heap_caps_malloc((sz), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#else
#define ALLOC_MEM(sz) malloc(sz)
#endif

#define MINIMP4_IMPLEMENTATION
#include "minimp4.h"

extern "C" {
#include "libh264/h264bsd_decoder.h"
#include "libh264/h264bsd_storage.h"
}

namespace cbdos {
namespace media {

static const char* TAG = "Mp4Parser";

struct Mp4ReaderState {
    FILE* file{nullptr};
    int64_t cached_offset{-1};
    size_t cached_size{0};
    uint8_t buffer[32768]; // Buffer de cache de 32KB
};

static Mp4ReaderState s_readerState;

static int mp4_read_callback(int64_t offset, void *buffer, size_t size, void *token) {
    Mp4ReaderState* state = (Mp4ReaderState*)token;
    if (!state || !state->file || !buffer || size == 0) return -1;

    // Si los datos solicitados caen dentro del bloque cacheado:
    if (state->cached_offset >= 0 &&
        offset >= state->cached_offset &&
        (offset + (int64_t)size) <= (state->cached_offset + (int64_t)state->cached_size)) {
        size_t bufPos = (size_t)(offset - state->cached_offset);
        memcpy(buffer, state->buffer + bufPos, size);
        return 0;
    }

    // Para lecturas que caben en el buffer de caché (<= 32KB):
    if (size <= sizeof(state->buffer)) {
        if (fseek(state->file, (long)offset, SEEK_SET) != 0) {
            state->cached_offset = -1;
            state->cached_size = 0;
            return -1;
        }
        size_t n = fread(state->buffer, 1, sizeof(state->buffer), state->file);
        if (n < size) {
            state->cached_offset = -1;
            state->cached_size = 0;
            return -1;
        }
        state->cached_offset = offset;
        state->cached_size = n;
        memcpy(buffer, state->buffer, size);
        return 0;
    }

    // Para lecturas más grandes que el buffer de caché (> 32KB):
    state->cached_offset = -1;
    state->cached_size = 0;
    if (fseek(state->file, (long)offset, SEEK_SET) != 0) return -1;
    size_t n = fread(buffer, 1, size, state->file);
    return (n == size) ? 0 : -1;
}

Mp4Parser::Mp4Parser() = default;

Mp4Parser::~Mp4Parser() {
    close();
}

bool Mp4Parser::open(const std::string& filepath) {
    close();
    m_filepath = filepath;
    m_file = fopen(filepath.c_str(), "rb");
    if (!m_file) {
        ESP_LOGE(TAG, "No se pudo abrir archivo MP4: %s", filepath.c_str());
        return false;
    }

    fseek(m_file, 0, SEEK_END);
    int64_t fileSize = ftell(m_file);
    fseek(m_file, 0, SEEK_SET);

    ESP_LOGI(TAG, "Iniciando analisis de MP4 (tamano: %lld bytes): %s", (long long)fileSize, filepath.c_str());

    s_readerState.file = m_file;
    s_readerState.cached_offset = -1;
    s_readerState.cached_size = 0;

    MP4D_demux_t* demux = (MP4D_demux_t*)ALLOC_MEM(sizeof(MP4D_demux_t));
    if (!demux) demux = (MP4D_demux_t*)malloc(sizeof(MP4D_demux_t));
    if (!demux) {
        close();
        return false;
    }
    memset(demux, 0, sizeof(MP4D_demux_t));

    if (!MP4D_open(demux, mp4_read_callback, &s_readerState, fileSize)) {
        ESP_LOGE(TAG, "Error en MP4D_open para %s", filepath.c_str());
        free(demux);
        close();
        return false;
    }

    m_demuxer = demux;
    m_videoTrack = -1;
    m_audioTrack = -1;

    for (unsigned i = 0; i < demux->track_count; i++) {
        const auto& track = demux->track[i];
        if (track.handler_type == MP4D_HANDLER_TYPE_VIDE ||
            track.object_type_indication == MP4_OBJECT_TYPE_AVC) {
            m_videoTrack = i;
            m_info.width = track.SampleDescription.video.width;
            m_info.height = track.SampleDescription.video.height;
            m_info.totalVideoSamples = track.sample_count;
            if (track.timescale > 0 && track.duration_lo > 0) {
                m_info.durationSec = track.duration_lo / track.timescale;
                if (m_info.durationSec > 0 && m_info.totalVideoSamples > 0) {
                    m_info.fps = (float)m_info.totalVideoSamples / (float)m_info.durationSec;
                }
            }
        } else if (track.handler_type == MP4D_HANDLER_TYPE_SOUN ||
                   track.object_type_indication == MP4_OBJECT_TYPE_AUDIO_ISO_IEC_14496_3 ||
                   track.object_type_indication == MP4_OBJECT_TYPE_AUDIO_ISO_IEC_13818_7_LC_PROFILE) {
            m_audioTrack = i;
            m_info.hasAudio = true;
            m_info.audioSampleRate = track.SampleDescription.audio.samplerate_hz ? track.SampleDescription.audio.samplerate_hz : 44100;
            m_info.audioChannels = track.SampleDescription.audio.channelcount ? track.SampleDescription.audio.channelcount : 2;
            m_info.totalAudioSamples = track.sample_count;
            m_info.audioBitrate = track.avg_bitrate_bps ? track.avg_bitrate_bps : 96000;
        }
    }

    if (m_videoTrack < 0) {
        ESP_LOGE(TAG, "No se encontro pista de video H.264 compatible");
        close();
        return false;
    }

    if (!initH264Decoder()) {
        close();
        return false;
    }

    m_currentVideoSample = 0;
    m_currentAudioSample = 0;
    return true;
}

void Mp4Parser::close() {
    cleanupH264Decoder();
    if (m_demuxer) {
        MP4D_demux_t* demux = (MP4D_demux_t*)m_demuxer;
        MP4D_close(demux);
        free(demux);
        m_demuxer = nullptr;
    }
    if (m_file) {
        fclose(m_file);
        m_file = nullptr;
    }
    if (m_nalBuffer) {
        free(m_nalBuffer);
        m_nalBuffer = nullptr;
        m_nalBufferSize = 0;
    }
    s_readerState.file = nullptr;
    s_readerState.cached_offset = -1;
    s_readerState.cached_size = 0;

    m_info = Mp4StreamInfo{};
    m_videoTrack = -1;
    m_audioTrack = -1;
    m_currentVideoSample = 0;
    m_currentAudioSample = 0;
}

bool Mp4Parser::initH264Decoder() {
    cleanupH264Decoder();

    storage_t* storage = h264bsdAlloc();
    if (!storage) return false;
    memset(storage, 0, sizeof(storage_t));

    if (h264bsdInit(storage, 1) != 0) {
        h264bsdFree(storage);
        return false;
    }

    m_h264Decoder = storage;
    MP4D_demux_t* demux = (MP4D_demux_t*)m_demuxer;

    // Inyectar SPS y PPS desde los metadatos del MP4 al decodificador
    int spsCount = 0;
    int spsBytes = 0;
    const void* sps = nullptr;
    while ((sps = MP4D_read_sps(demux, m_videoTrack, spsCount, &spsBytes)) != nullptr) {
        uint8_t startCode[4] = {0, 0, 0, 1};
        std::vector<uint8_t> spsNal(4 + spsBytes);
        memcpy(spsNal.data(), startCode, 4);
        memcpy(spsNal.data() + 4, sps, spsBytes);
        u32 readBytes = 0;
        h264bsdDecode(storage, spsNal.data(), spsNal.size(), 0, &readBytes);
        spsCount++;
    }

    int ppsCount = 0;
    int ppsBytes = 0;
    const void* pps = nullptr;
    while ((pps = MP4D_read_pps(demux, m_videoTrack, ppsCount, &ppsBytes)) != nullptr) {
        uint8_t startCode[4] = {0, 0, 0, 1};
        std::vector<uint8_t> ppsNal(4 + ppsBytes);
        memcpy(ppsNal.data(), startCode, 4);
        memcpy(ppsNal.data() + 4, pps, ppsBytes);
        u32 readBytes = 0;
        h264bsdDecode(storage, ppsNal.data(), ppsNal.size(), 0, &readBytes);
        ppsCount++;
    }

    return true;
}

void Mp4Parser::cleanupH264Decoder() {
    if (m_h264Decoder) {
        storage_t* storage = (storage_t*)m_h264Decoder;
        h264bsdShutdown(storage);
        h264bsdFree(storage);
        m_h264Decoder = nullptr;
    }
}

bool Mp4Parser::seekToSample(uint32_t sampleIndex) {
    if (sampleIndex >= m_info.totalVideoSamples) return false;
    m_currentVideoSample = sampleIndex;
    if (m_info.totalAudioSamples > 0 && m_info.totalVideoSamples > 0) {
        m_currentAudioSample = (uint32_t)(((uint64_t)sampleIndex * m_info.totalAudioSamples) / m_info.totalVideoSamples);
    }
    return true;
}

bool Mp4Parser::decodeNextVideoFrame(uint8_t* outRgb565, uint32_t outWidth, uint32_t outHeight) {
    static uint32_t s_callCount = 0;
    bool verbose = (s_callCount < 20 || (s_callCount % 100 == 0));
    s_callCount++;
    if (!m_file || !m_demuxer || !m_h264Decoder || !outRgb565 || outWidth == 0 || outHeight == 0) return false;
    MP4D_demux_t* demux = (MP4D_demux_t*)m_demuxer;
    storage_t* storage = (storage_t*)m_h264Decoder;

    // 1. Si ya hay imagen pendiente en el DPB (puede pasar si el tick anterior la dejó)
    u32 picId = 0, isIdr = 0, numErr = 0;
    u8* pic = h264bsdNextOutputPicture(storage, &picId, &isIdr, &numErr);
    if (pic) {
        u32 picW = h264bsdPicWidth(storage) * 16;
        u32 picH = h264bsdPicHeight(storage) * 16;
        if (picW > 0 && picH > 0) {
            const u8* y = pic;
            const u8* u = y + (picW * picH);
            const u8* v = u + (picW * picH / 4);
            yuv420ToRgb565(y, u, v, picW, picH, outRgb565, outWidth, outHeight);
            return true;
        }
    }

    // 2. Procesar UNA muestra por tick (no bloquear el hilo LVGL)
    if (m_currentVideoSample >= m_info.totalVideoSamples) {
        return false;
    }

    unsigned int frameBytes = 0;
    unsigned int timestamp = 0;
    unsigned int duration = 0;
    MP4D_file_offset_t offset = MP4D_frame_offset(demux, m_videoTrack, m_currentVideoSample, &frameBytes, &timestamp, &duration);
    m_currentVideoSample++;

    if (verbose) ESP_LOGI(TAG, "[%lu] sample=%lu frameBytes=%u offset=%llu",
        (unsigned long)s_callCount, (unsigned long)(m_currentVideoSample-1),
        frameBytes, (unsigned long long)offset);

    if (frameBytes == 0 || offset == 0) {
        ESP_LOGW(TAG, "[%lu] sample=%lu SKIP (empty)", (unsigned long)s_callCount, (unsigned long)(m_currentVideoSample-1));
        return false;
    }

    // Ampliar buffer si hace falta
    if (frameBytes > m_nalBufferSize) {
        if (m_nalBuffer) free(m_nalBuffer);
        m_nalBufferSize = frameBytes + 8192;
        m_nalBuffer = (uint8_t*)ALLOC_MEM(m_nalBufferSize);
        if (!m_nalBuffer) m_nalBuffer = (uint8_t*)malloc(m_nalBufferSize);
        if (!m_nalBuffer) { m_nalBufferSize = 0; return false; }
    }

    if (fseek(m_file, offset, SEEK_SET) != 0) return false;
    if (fread(m_nalBuffer, 1, frameBytes, m_file) != frameBytes) return false;

    // Convertir prefijos de longitud MP4 → Annex-B start codes (00 00 00 01)
    size_t pos = 0;
    while (pos + 4 <= frameBytes) {
        uint32_t nalLen = ((uint32_t)m_nalBuffer[pos]     << 24) |
                          ((uint32_t)m_nalBuffer[pos + 1] << 16) |
                          ((uint32_t)m_nalBuffer[pos + 2] << 8)  |
                          ((uint32_t)m_nalBuffer[pos + 3]);
        m_nalBuffer[pos]     = 0;
        m_nalBuffer[pos + 1] = 0;
        m_nalBuffer[pos + 2] = 0;
        m_nalBuffer[pos + 3] = 1;
        if (nalLen == 0 || pos + 4 + nalLen > frameBytes) break;
        pos += 4 + nalLen;
    }

    // Decodificar todos los NAL units de esta muestra.
    // PROTOCOLO libh264: cuando readBytes==0 + H264BSD_HDRS_RDY → pendingActivation en 2 pasos:
    //   el decoder almacena el puntero en prevBufPointer y espera ser llamado de NUEVO
    //   con EL MISMO bytePtr para completar la activación. NO avanzar bytePtr en ese caso.
    u8* bytePtr = m_nalBuffer;
    u32 bytesLeft = (u32)frameBytes;
    pic = nullptr;
    int safetyCount = 0; // evitar loop infinito

    while (bytesLeft > 0 && safetyCount < 512) {
        safetyCount++;
        u32 readBytes = 0;
        u32 status = h264bsdDecode(storage, bytePtr, bytesLeft, m_currentVideoSample, &readBytes);

        if (readBytes > 0) {
            // Caso normal: el decoder consumió bytes, avanzar puntero
            bytePtr   += readBytes;
            bytesLeft  = (readBytes >= bytesLeft) ? 0 : (bytesLeft - readBytes);
        }
        // Si readBytes == 0: el decoder está en estado pendingActivation (2do paso de init).
        // DEBE llamarse de nuevo con el MISMO bytePtr — no avanzar nada.

        if (verbose) ESP_LOGI(TAG, "  h264bsdDecode st=%lu rb=%lu bytesLeft=%lu",
            (unsigned long)status, (unsigned long)readBytes, (unsigned long)bytesLeft);

        if (status == H264BSD_PIC_RDY) {
            pic = h264bsdNextOutputPicture(storage, &picId, &isIdr, &numErr);
            if (verbose) ESP_LOGI(TAG, "  PIC_RDY pic=%s", pic ? "OK" : "NULL");
            if (pic) break;
        } else if (status == H264BSD_HDRS_RDY) {
            if (verbose) ESP_LOGI(TAG, "  HDRS_RDY → retry same ptr");
            continue;
        } else if (status == H264BSD_ERROR ||
                   status == H264BSD_PARAM_SET_ERROR ||
                   status == H264BSD_MEMALLOC_ERROR) {
            ESP_LOGW(TAG, "  DECODE_ERR=%lu sample=%lu", (unsigned long)status, (unsigned long)m_currentVideoSample);
            break;
        }
    }

    if (pic) {
        u32 picW = h264bsdPicWidth(storage) * 16;
        u32 picH = h264bsdPicHeight(storage) * 16;
        if (verbose) ESP_LOGI(TAG, "  → FRAME OK %lux%lu", (unsigned long)picW, (unsigned long)picH);
        if (picW > 0 && picH > 0) {
            const u8* y = pic;
            const u8* u = y + (picW * picH);
            const u8* v = u + (picW * picH / 4);
            yuv420ToRgb565(y, u, v, picW, picH, outRgb565, outWidth, outHeight);
            return true;
        }
    } else {
        if (verbose) ESP_LOGW(TAG, "  → NO FRAME (call=%lu sample=%lu)", (unsigned long)s_callCount, (unsigned long)m_currentVideoSample);
    }

    return false;
}



bool Mp4Parser::readNextAudioFrame(uint8_t* outBuffer, size_t maxBufferSize, size_t& outFrameSize) {
    outFrameSize = 0;
    if (!m_file || !m_demuxer || m_audioTrack < 0 || !outBuffer) return false;
    MP4D_demux_t* demux = (MP4D_demux_t*)m_demuxer;

    if (m_currentAudioSample >= m_info.totalAudioSamples) {
        return false;
    }

    unsigned int frameBytes = 0;
    unsigned int timestamp = 0;
    unsigned int duration = 0;
    MP4D_file_offset_t offset = MP4D_frame_offset(demux, m_audioTrack, m_currentAudioSample, &frameBytes, &timestamp, &duration);

    if (frameBytes == 0 || offset == 0 || (frameBytes + 7) > maxBufferSize) {
        m_currentAudioSample++;
        return false;
    }

    // Construir cabecera ADTS de 7 bytes para que el decodificador AAC lo procese directamente
    uint8_t profile = 1; // AAC LC
    uint8_t freqIdx = 4; // 44100 Hz
    if (m_info.audioSampleRate == 48000) freqIdx = 3;
    else if (m_info.audioSampleRate == 32000) freqIdx = 5;
    else if (m_info.audioSampleRate == 24000) freqIdx = 6;
    else if (m_info.audioSampleRate == 22050) freqIdx = 7;
    else if (m_info.audioSampleRate == 16000) freqIdx = 8;

    uint8_t chanCfg = (uint8_t)m_info.audioChannels;
    size_t adtsLen = frameBytes + 7;

    outBuffer[0] = 0xFF;
    outBuffer[1] = 0xF9; // MPEG-2 AAC, no CRC
    outBuffer[2] = ((profile & 3) << 6) | ((freqIdx & 0xF) << 2) | ((chanCfg >> 2) & 1);
    outBuffer[3] = ((chanCfg & 3) << 6) | ((adtsLen >> 11) & 3);
    outBuffer[4] = (adtsLen >> 3) & 0xFF;
    outBuffer[5] = ((adtsLen & 7) << 5) | 0x1F;
    outBuffer[6] = 0xFC;

    fseek(m_file, offset, SEEK_SET);
    if (fread(outBuffer + 7, 1, frameBytes, m_file) != frameBytes) {
        m_currentAudioSample++;
        return false;
    }

    outFrameSize = adtsLen;
    m_currentAudioSample++;
    return true;
}

// Conversor de planos YUV420 a RGB565 con conservación de aspect-ratio (Letterbox)
void Mp4Parser::yuv420ToRgb565(const uint8_t* y, const uint8_t* u, const uint8_t* v,
                               uint32_t srcWidth, uint32_t srcHeight,
                               uint8_t* dstRgb565, uint32_t dstWidth, uint32_t dstHeight) {
    if (!y || !u || !v || !dstRgb565 || srcWidth == 0 || srcHeight == 0 || dstWidth == 0 || dstHeight == 0) return;

    uint16_t* dst = (uint16_t*)dstRgb565;

    // Calcular dimensiones de escalado proporcional (mantener aspect ratio)
    uint32_t renderW = dstWidth;
    uint32_t renderH = (dstWidth * srcHeight) / srcWidth;

    if (renderH > dstHeight) {
        renderH = dstHeight;
        renderW = (dstHeight * srcWidth) / srcHeight;
    }

    uint32_t offsetX = (dstWidth > renderW) ? (dstWidth - renderW) / 2 : 0;
    uint32_t offsetY = (dstHeight > renderH) ? (dstHeight - renderH) / 2 : 0;

    // Limpiar barras negras superior e inferior si el marco cambió
    if (offsetY > 0) {
        memset(dst, 0, offsetY * dstWidth * sizeof(uint16_t));
        uint32_t bottomStart = offsetY + renderH;
        if (bottomStart < dstHeight) {
            memset(dst + (bottomStart * dstWidth), 0, (dstHeight - bottomStart) * dstWidth * sizeof(uint16_t));
        }
    }

    // Precalcular tabla horizontal X para evitar divisiones en el loop interno
    static std::vector<uint32_t> s_xMap;
    if (s_xMap.size() < renderW) {
        s_xMap.resize(renderW);
    }
    for (uint32_t dx = 0; dx < renderW; dx++) {
        s_xMap[dx] = (dx * srcWidth) / renderW;
    }

    for (uint32_t dy = 0; dy < renderH; dy++) {
        uint32_t sy = (dy * srcHeight) / renderH;
        const uint8_t* yRow = y + (sy * srcWidth);
        const uint8_t* uRow = u + ((sy >> 1) * (srcWidth >> 1));
        const uint8_t* vRow = v + ((sy >> 1) * (srcWidth >> 1));

        uint16_t* outRow = dst + ((offsetY + dy) * dstWidth) + offsetX;

        for (uint32_t dx = 0; dx < renderW; dx++) {
            uint32_t sx = s_xMap[dx];
            int Y = yRow[sx];
            int U = uRow[sx >> 1] - 128;
            int V = vRow[sx >> 1] - 128;

            int R = Y + ((1436 * V) >> 10);
            int G = Y - ((352 * U + 731 * V) >> 10);
            int B = Y + ((1815 * U) >> 10);

            if ((uint32_t)R > 255) R = (R < 0) ? 0 : 255;
            if ((uint32_t)G > 255) G = (G < 0) ? 0 : 255;
            if ((uint32_t)B > 255) B = (B < 0) ? 0 : 255;

            outRow[dx] = (uint16_t)(((R & 0xF8) << 8) | ((G & 0xFC) << 3) | (B >> 3));
        }
    }
}

} // namespace media
} // namespace cbdos
