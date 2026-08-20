#include "WavPlayer.hpp"
#include <esp_log.h>
#include <cstring>

static const char* TAG = "WavPlayer";

namespace cbdos {
namespace audio {

WavPlayer& WavPlayer::getInstance() {
    static WavPlayer instance;
    return instance;
}

WavPlayer::WavPlayer() {
    m_mutex = xSemaphoreCreateMutex();
}

WavPlayer::~WavPlayer() {
    stop();
    if (m_mutex) {
        vSemaphoreDelete(m_mutex);
        m_mutex = nullptr;
    }
}

std::string WavPlayer::getCurrentTrackTitle() const {
    if (m_currentFile.empty()) return "Ninguna cancion seleccionada";
    size_t lastSlash = m_currentFile.find_last_of("/\\");
    std::string name = (lastSlash != std::string::npos) ? m_currentFile.substr(lastSlash + 1) : m_currentFile;
    size_t lastDot = name.find_last_of('.');
    if (lastDot != std::string::npos) {
        name = name.substr(0, lastDot);
    }
    return name;
}

uint32_t WavPlayer::getCurrentTimeSec() const {
    if (!m_isPlaying || m_header.byteRate == 0) return 0;
    return m_bytesRead / m_header.byteRate;
}

float WavPlayer::getProgress() const {
    if (!m_isPlaying || m_header.dataSize == 0) return 0.0f;
    float prog = (float)m_bytesRead / (float)m_header.dataSize;
    if (prog < 0.0f) prog = 0.0f;
    if (prog > 1.0f) prog = 1.0f;
    return prog;
}

void WavPlayer::pause() {
    m_isPaused = true;
    ESP_LOGI(TAG, "Audio en pausa");
}

void WavPlayer::resume() {
    m_isPaused = false;
    ESP_LOGI(TAG, "Audio reanudado");
}

void WavPlayer::seek(uint32_t targetSec) {
    if (targetSec > m_totalTimeSec) targetSec = m_totalTimeSec;
    m_seekRequestSec = (int32_t)targetSec;
    ESP_LOGI(TAG, "Seek solicitado a %lu segundos", targetSec);
}

void WavPlayer::stop() {
    m_stopRequested = true;
    m_isPaused = false;
    if (m_taskHandle) {
        int timeout = 50;
        while (m_isPlaying && timeout-- > 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
    m_isPlaying = false;
}

bool WavPlayer::play(const char* filepath) {
    if (!filepath) return false;

    stop();

    if (xSemaphoreTake(m_mutex, pdMS_TO_TICKS(500)) != pdTRUE) {
        return false;
    }

    FILE* f = fopen(filepath, "rb");
    if (!f) {
        ESP_LOGE(TAG, "No se pudo abrir el archivo de audio: %s", filepath);
        xSemaphoreGive(m_mutex);
        return false;
    }

    // Leer encabezado inicial RIFF
    char riffHeader[12];
    if (fread(riffHeader, 1, 12, f) != 12 || memcmp(riffHeader, "RIFF", 4) != 0 || memcmp(riffHeader + 8, "WAVE", 4) != 0) {
        ESP_LOGE(TAG, "Encabezado RIFF/WAVE no valido en: %s", filepath);
        fclose(f);
        xSemaphoreGive(m_mutex);
        return false;
    }

    // Buscar chunk "fmt " y chunk "data"
    bool fmtFound = false;
    bool dataFound = false;
    uint32_t dataOffset = 12;

    while (!dataFound && !feof(f)) {
        char chunkId[4];
        uint32_t chunkSize = 0;
        if (fread(chunkId, 1, 4, f) != 4) break;
        if (fread(&chunkSize, 4, 1, f) != 1) break;
        dataOffset += 8;

        if (memcmp(chunkId, "fmt ", 4) == 0) {
            m_header.fmtSize = chunkSize;
            fread(&m_header.audioFormat, 2, 1, f);
            fread(&m_header.numChannels, 2, 1, f);
            fread(&m_header.sampleRate, 4, 1, f);
            fread(&m_header.byteRate, 4, 1, f);
            fread(&m_header.blockAlign, 2, 1, f);
            fread(&m_header.bitsPerSample, 2, 1, f);
            
            // Saltar cualquier byte extra del chunk fmt
            if (chunkSize > 16) {
                fseek(f, chunkSize - 16, SEEK_CUR);
            }
            dataOffset += chunkSize;
            fmtFound = true;
        } else if (memcmp(chunkId, "data", 4) == 0) {
            m_header.dataSize = chunkSize;
            m_dataStartOffset = dataOffset;
            dataFound = true;
            break;
        } else {
            // Saltar chunk desconocido (ej. LIST, ID3, JUNK)
            fseek(f, chunkSize, SEEK_CUR);
            dataOffset += chunkSize;
        }
    }

    if (!fmtFound || !dataFound) {
        ESP_LOGE(TAG, "Formato WAV corrupto o sin chunk data: %s", filepath);
        fclose(f);
        xSemaphoreGive(m_mutex);
        return false;
    }

    m_file = f;
    m_currentFile = filepath;
    m_bytesRead = 0;
    m_totalTimeSec = (m_header.byteRate > 0) ? (m_header.dataSize / m_header.byteRate) : 0;
    m_isPlaying = true;
    m_isPaused = false;
    m_stopRequested = false;
    m_seekRequestSec = -1;

    ESP_LOGI(TAG, "Iniciando reproduccion: '%s' (%lu Hz, %d canales, %d-bit, duracion: %lu seg)",
             filepath, m_header.sampleRate, m_header.numChannels, m_header.bitsPerSample, m_totalTimeSec);

    xTaskCreatePinnedToCore(playbackTask, "wav_player_task", 4096, this, 4, &m_taskHandle, 0);

    xSemaphoreGive(m_mutex);
    return true;
}

void WavPlayer::playbackTask(void* param) {
    WavPlayer* player = static_cast<WavPlayer*>(param);
    player->runPlayback();
    vTaskDelete(NULL);
}

void WavPlayer::runPlayback() {
    const size_t CHUNK_SIZE = 2048;
    uint8_t buffer[CHUNK_SIZE];

    while (!m_stopRequested && m_file) {
        if (m_isPaused) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        if (m_seekRequestSec >= 0) {
            uint32_t targetByte = m_seekRequestSec * m_header.byteRate;
            if (targetByte > m_header.dataSize) targetByte = m_header.dataSize;
            targetByte = (targetByte / m_header.blockAlign) * m_header.blockAlign; // Alinear bloque
            fseek(m_file, m_dataStartOffset + targetByte, SEEK_SET);
            m_bytesRead = targetByte;
            m_seekRequestSec = -1;
        }

        size_t bytesToRead = CHUNK_SIZE;
        if (m_bytesRead + bytesToRead > m_header.dataSize) {
            bytesToRead = m_header.dataSize - m_bytesRead;
        }

        if (bytesToRead == 0) {
            // Fin de la pista alcanzado
            break;
        }

        size_t nRead = fread(buffer, 1, bytesToRead, m_file);
        if (nRead == 0) {
            break;
        }

        // Transmitir al driver de audio I2S
        cbdos::audio::writeAudio(buffer, nRead);
        m_bytesRead += nRead;
    }

    if (m_file) {
        fclose(m_file);
        m_file = nullptr;
    }

    m_isPlaying = false;
    m_isPaused = false;
    m_taskHandle = nullptr;
    ESP_LOGI(TAG, "Reproduccion finalizada");
}

AudioStats WavPlayer::getStats() const {
    AudioStats stats;
    stats.isPlaying = m_isPlaying && !m_isPaused;
    stats.codec = m_isPlaying ? CodecType::WAV : CodecType::None;
    stats.sampleRate = m_header.sampleRate;
    stats.channels = (uint8_t)m_header.numChannels;
    stats.bitRate = m_header.byteRate * 8;
    stats.bufferPercent = 100;
    return stats;
}

} // namespace audio
} // namespace cbdos
