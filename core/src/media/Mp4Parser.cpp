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
#include "aacdec.h"
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

    if (m_info.hasAudio && m_audioTrack >= 0) {
        initAacDecoder();
    }

    m_currentVideoSample = 0;
    m_currentAudioSample = 0;
    return true;
}

void Mp4Parser::close() {
    cleanupH264Decoder();
    cleanupAacDecoder();
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

    if (frameBytes == 0 || offset == 0) {
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

        if (status == H264BSD_PIC_RDY) {
            pic = h264bsdNextOutputPicture(storage, &picId, &isIdr, &numErr);
            if (pic) break;
        } else if (status == H264BSD_HDRS_RDY) {
            continue;
        } else if (status == H264BSD_ERROR ||
                   status == H264BSD_PARAM_SET_ERROR ||
                   status == H264BSD_MEMALLOC_ERROR) {
            break;
        }
    }

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

    return false;
}



bool Mp4Parser::initAacDecoder() {
    cleanupAacDecoder();
    if (m_audioTrack < 0 || !m_info.hasAudio) return true;
    m_aacDecoder = AACInitDecoder();
    return m_aacDecoder != nullptr;
}

void Mp4Parser::cleanupAacDecoder() {
    if (m_aacDecoder) {
        AACFreeDecoder((HAACDecoder)m_aacDecoder);
        m_aacDecoder = nullptr;
    }
    if (m_aacRawBuf) {
        free(m_aacRawBuf);
        m_aacRawBuf = nullptr;
        m_aacRawBufSize = 0;
    }
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

bool Mp4Parser::decodeNextAudioPcm(int16_t* outPcm, size_t maxSamples, size_t& outSamples) {
    outSamples = 0;
    if (!m_file || !m_demuxer || m_audioTrack < 0 || !m_aacDecoder || !outPcm || maxSamples == 0) return false;

    if (m_currentAudioSample >= m_info.totalAudioSamples) return false;

    if (!m_aacRawBuf) {
        m_aacRawBufSize = 8192;
        m_aacRawBuf = (uint8_t*)malloc(m_aacRawBufSize);
        if (!m_aacRawBuf) return false;
    }

    size_t adtsBytes = 0;
    if (!readNextAudioFrame(m_aacRawBuf, m_aacRawBufSize, adtsBytes) || adtsBytes == 0) {
        return false;
    }

    unsigned char* inPtr = m_aacRawBuf;
    int bytesLeft = (int)adtsBytes;

    int err = AACDecode((HAACDecoder)m_aacDecoder, &inPtr, &bytesLeft, (short*)outPcm);
    if (err != 0) {
        return false;
    }

    AACFrameInfo frameInfo;
    AACGetLastFrameInfo((HAACDecoder)m_aacDecoder, &frameInfo);
    outSamples = frameInfo.outputSamps;
    return (outSamples > 0);
}

// Tablas precalculadas de coeficientes YUV a RGB
static int16_t s_r_v[256];
static int16_t s_g_u[256];
static int16_t s_g_v[256];
static int16_t s_b_u[256];
static uint8_t s_clamp[1024];
static bool s_lut_inited = false;

static void initYuvLut() {
    if (s_lut_inited) return;
    for (int i = 0; i < 256; i++) {
        int val = i - 128;
        s_r_v[i] = (int16_t)((1436 * val) >> 10);
        s_g_u[i] = (int16_t)((352 * val) >> 10);
        s_g_v[i] = (int16_t)((731 * val) >> 10);
        s_b_u[i] = (int16_t)((1815 * val) >> 10);
    }
    for (int i = 0; i < 1024; i++) {
        int v = i - 256;
        if (v < 0) s_clamp[i] = 0;
        else if (v > 255) s_clamp[i] = 255;
        else s_clamp[i] = (uint8_t)v;
    }
    s_lut_inited = true;
}

// Conversor ultrarrápido de YUV420 a RGB565 (optimizado con LUTs y escritura de 32 bits)
void Mp4Parser::yuv420ToRgb565(const uint8_t* y, const uint8_t* u, const uint8_t* v,
                               uint32_t srcWidth, uint32_t srcHeight,
                               uint8_t* dstRgb565, uint32_t dstWidth, uint32_t dstHeight) {
    if (!y || !u || !v || !dstRgb565 || srcWidth == 0 || srcHeight == 0 || dstWidth == 0 || dstHeight == 0) return;

    initYuvLut();
    const uint8_t* const clamp = s_clamp + 256;
    uint16_t* dst = (uint16_t*)dstRgb565;

    // Calcular dimensiones de escalado proporcional (Letterbox)
    uint32_t renderW = dstWidth;
    uint32_t renderH = (dstWidth * srcHeight) / srcWidth;

    if (renderH > dstHeight) {
        renderH = dstHeight;
        renderW = (dstHeight * srcWidth) / srcHeight;
    }
    renderW &= ~1; // asegurar ancho par para escrituras de 32 bits

    uint32_t offsetX = (dstWidth > renderW) ? (dstWidth - renderW) / 2 : 0;
    uint32_t offsetY = (dstHeight > renderH) ? (dstHeight - renderH) / 2 : 0;

    // Vía rápida: 1 a 1 sin reescalado
    if (srcWidth == renderW && srcHeight == renderH) {
        for (uint32_t dy = 0; dy < renderH; dy++) {
            const uint8_t* yRow = y + (dy * srcWidth);
            const uint8_t* uRow = u + ((dy >> 1) * (srcWidth >> 1));
            const uint8_t* vRow = v + ((dy >> 1) * (srcWidth >> 1));
            uint32_t* outRow32 = (uint32_t*)(dst + ((offsetY + dy) * dstWidth) + offsetX);

            for (uint32_t dx = 0; dx < renderW; dx += 2) {
                uint8_t u_val = uRow[dx >> 1];
                uint8_t v_val = vRow[dx >> 1];
                int rv = s_r_v[v_val];
                int guv = -(s_g_u[u_val] + s_g_v[v_val]);
                int bu = s_b_u[u_val];

                uint8_t y0 = yRow[dx];
                uint8_t r0 = clamp[y0 + rv];
                uint8_t g0 = clamp[y0 + guv];
                uint8_t b0 = clamp[y0 + bu];
                uint32_t p0 = ((r0 & 0xF8) << 8) | ((g0 & 0xFC) << 3) | (b0 >> 3);

                uint8_t y1 = yRow[dx + 1];
                uint8_t r1 = clamp[y1 + rv];
                uint8_t g1 = clamp[y1 + guv];
                uint8_t b1 = clamp[y1 + bu];
                uint32_t p1 = ((r1 & 0xF8) << 8) | ((g1 & 0xFC) << 3) | (b1 >> 3);

                outRow32[dx >> 1] = p0 | (p1 << 16);
            }
        }
        return;
    }

    // Vía general con escalado
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
            uint8_t u_val = uRow[sx >> 1];
            uint8_t v_val = vRow[sx >> 1];

            uint8_t Y = yRow[sx];
            uint8_t R = clamp[Y + s_r_v[v_val]];
            uint8_t G = clamp[Y - (s_g_u[u_val] + s_g_v[v_val])];
            uint8_t B = clamp[Y + s_b_u[u_val]];

            outRow[dx] = (uint16_t)(((R & 0xF8) << 8) | ((G & 0xFC) << 3) | (B >> 3));
        }
    }
}

} // namespace media
} // namespace cbdos

