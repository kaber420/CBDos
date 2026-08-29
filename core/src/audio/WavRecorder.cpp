#include "WavRecorder.hpp"
#include "cbdos/log.hpp"
#include "cbdos/memory.hpp"
#include <cstring>
#include <cmath>

static const char* TAG = "WavRecorder";

namespace cbdos {
namespace audio {

WavRecorder& WavRecorder::getInstance() {
    static WavRecorder instance;
    return instance;
}

WavRecorder::WavRecorder() {
    m_mutex = xSemaphoreCreateMutex();
}

WavRecorder::~WavRecorder() {
    stop();
    if (m_mutex) {
        vSemaphoreDelete(m_mutex);
        m_mutex = nullptr;
    }
}

bool WavRecorder::writeWavHeader(FILE* f, const RecordConfig& cfg, uint32_t dataBytes) {
    if (!f) return false;

    uint32_t totalChunkSize = 36 + dataBytes;
    uint32_t byteRate = cfg.sampleRate * cfg.channels * (cfg.bitsPerSample / 8);
    uint16_t blockAlign = cfg.channels * (cfg.bitsPerSample / 8);
    uint16_t audioFormat = 1; // PCM lineal

    uint8_t header[44];
    // RIFF Chunk Descriptor
    memcpy(&header[0], "RIFF", 4);
    header[4] = (uint8_t)(totalChunkSize & 0xFF);
    header[5] = (uint8_t)((totalChunkSize >> 8) & 0xFF);
    header[6] = (uint8_t)((totalChunkSize >> 16) & 0xFF);
    header[7] = (uint8_t)((totalChunkSize >> 24) & 0xFF);
    memcpy(&header[8], "WAVE", 4);

    // "fmt " sub-chunk
    memcpy(&header[12], "fmt ", 4);
    uint32_t subchunk1Size = 16;
    header[16] = (uint8_t)(subchunk1Size & 0xFF);
    header[17] = (uint8_t)((subchunk1Size >> 8) & 0xFF);
    header[18] = (uint8_t)((subchunk1Size >> 16) & 0xFF);
    header[19] = (uint8_t)((subchunk1Size >> 24) & 0xFF);
    header[20] = (uint8_t)(audioFormat & 0xFF);
    header[21] = (uint8_t)((audioFormat >> 8) & 0xFF);
    header[22] = (uint8_t)(cfg.channels & 0xFF);
    header[23] = (uint8_t)((cfg.channels >> 8) & 0xFF);

    header[24] = (uint8_t)(cfg.sampleRate & 0xFF);
    header[25] = (uint8_t)((cfg.sampleRate >> 8) & 0xFF);
    header[26] = (uint8_t)((cfg.sampleRate >> 16) & 0xFF);
    header[27] = (uint8_t)((cfg.sampleRate >> 24) & 0xFF);

    header[28] = (uint8_t)(byteRate & 0xFF);
    header[29] = (uint8_t)((byteRate >> 8) & 0xFF);
    header[30] = (uint8_t)((byteRate >> 16) & 0xFF);
    header[31] = (uint8_t)((byteRate >> 24) & 0xFF);

    header[32] = (uint8_t)(blockAlign & 0xFF);
    header[33] = (uint8_t)((blockAlign >> 8) & 0xFF);
    header[34] = (uint8_t)(cfg.bitsPerSample & 0xFF);
    header[35] = (uint8_t)((cfg.bitsPerSample >> 8) & 0xFF);

    // "data" sub-chunk
    memcpy(&header[36], "data", 4);
    header[40] = (uint8_t)(dataBytes & 0xFF);
    header[41] = (uint8_t)((dataBytes >> 8) & 0xFF);
    header[42] = (uint8_t)((dataBytes >> 16) & 0xFF);
    header[43] = (uint8_t)((dataBytes >> 24) & 0xFF);

    fseek(f, 0, SEEK_SET);
    size_t written = fwrite(header, 1, 44, f);
    return written == 44;
}

bool WavRecorder::start(const char* filepath, const RecordConfig& cfg) {
    if (!filepath || m_isRecording) return false;

    if (xSemaphoreTake(m_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return false;
    }

    m_filePath = filepath;
    m_config = cfg;
    m_stopRequested = false;
    m_isPaused = false;
    m_durationMs = 0;
    m_peakLevel = 0.0f;
    m_totalDataBytesWritten = 0;

    m_file = fopen(m_filePath.c_str(), "wb");
    if (!m_file) {
        ESP_LOGE(TAG, "Error fopen wb en '%s'. Verificando si /sdcard esta montado...", m_filePath.c_str());
        xSemaphoreGive(m_mutex);
        return false;
    }

    // Escribir cabecera provisional de 44 bytes
    if (!writeWavHeader(m_file, m_config, 0)) {
        ESP_LOGE(TAG, "Error escribiendo header WAV en '%s'", m_filePath.c_str());
        fclose(m_file);
        m_file = nullptr;
        xSemaphoreGive(m_mutex);
        return false;
    }
    fflush(m_file);

    m_isRecording = true;

    BaseType_t res = xTaskCreatePinnedToCore(
        recordingTask,
        "wav_rec_task",
        8192,
        this,
        5,
        &m_taskHandle,
        1
    );

    if (res != pdPASS) {
        ESP_LOGE(TAG, "Fallo al crear tarea de grabación");
        fclose(m_file);
        m_file = nullptr;
        m_isRecording = false;
        xSemaphoreGive(m_mutex);
        return false;
    }

    xSemaphoreGive(m_mutex);
    ESP_LOGI(TAG, "Grabación iniciada en: %s (%lu Hz, %d ch, %d bits)",
             m_filePath.c_str(), m_config.sampleRate, m_config.channels, m_config.bitsPerSample);
    return true;
}

void WavRecorder::pause() {
    m_isPaused = true;
}

void WavRecorder::resume() {
    m_isPaused = false;
}

void WavRecorder::stop() {
    if (!m_isRecording) return;

    m_stopRequested = true;

    // Esperar a que la tarea finalice
    int timeout = 50;
    while (m_isRecording && timeout-- > 0) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void WavRecorder::recordingTask(void* param) {
    auto* self = static_cast<WavRecorder*>(param);
    self->runRecording();
    self->m_isRecording = false;
    self->m_taskHandle = nullptr;
    vTaskDelete(nullptr);
}

void WavRecorder::runRecording() {
    const size_t CHUNK_SAMPLES = 512;
    const size_t bytesPerSample = (m_config.bitsPerSample / 8);
    const size_t chunkBytes = CHUNK_SAMPLES * m_config.channels * bytesPerSample;

    // Búfer DMA / PSRAM para leer del micrófono
    uint8_t* pcmBuffer = (uint8_t*)malloc(chunkBytes);
    if (!pcmBuffer) {
        ESP_LOGE(TAG, "No hay memoria para buffer de grabación");
        if (m_file) {
            fclose(m_file);
            m_file = nullptr;
        }
        return;
    }

    const uint32_t bytesPerSecond = m_config.sampleRate * m_config.channels * bytesPerSample;

    while (!m_stopRequested) {
        if (m_isPaused) {
            m_peakLevel = 0.0f;
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        // Leer datos crudos desde el HAL (I2S / Códec ES8311)
        size_t bytesRead = readAudio(pcmBuffer, chunkBytes, 100);
        if (bytesRead > 0 && m_file) {
            // Calcular nivel de pico para el vúmetro UI
            int16_t* samples = (int16_t*)pcmBuffer;
            size_t numSamples = bytesRead / sizeof(int16_t);
            int16_t maxSample = 0;
            for (size_t i = 0; i < numSamples; i++) {
                int16_t s = std::abs(samples[i]);
                if (s > maxSample) maxSample = s;
            }
            float level = (float)maxSample / 32768.0f;
            m_peakLevel = (m_peakLevel * 0.3f) + (level * 0.7f); // Suavizado exponencial

            // Escribir datos PCM al archivo en MicroSD
            fwrite(pcmBuffer, 1, bytesRead, m_file);
            m_totalDataBytesWritten += bytesRead;

            if (bytesPerSecond > 0) {
                m_durationMs = (uint32_t)(((uint64_t)m_totalDataBytesWritten * 1000) / bytesPerSecond);
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    // Finalizar archivo WAV con el tamaño exacto en el header
    if (m_file) {
        writeWavHeader(m_file, m_config, m_totalDataBytesWritten);
        fflush(m_file);
        fclose(m_file);
        m_file = nullptr;
        ESP_LOGI(TAG, "Grabación finalizada con éxito: %lu bytes (%lu ms)",
                 m_totalDataBytesWritten, m_durationMs);
    }

    free(pcmBuffer);
    m_peakLevel = 0.0f;
}

// Implementación de las funciones de alto nivel en cbdos::audio
bool recordStart(const char* targetFilePath, const RecordConfig& cfg) {
    return WavRecorder::getInstance().start(targetFilePath, cfg);
}

void recordPause() {
    WavRecorder::getInstance().pause();
}

void recordResume() {
    WavRecorder::getInstance().resume();
}

void recordStop() {
    WavRecorder::getInstance().stop();
}

bool isRecording() {
    return WavRecorder::getInstance().isRecording();
}

bool isRecordPaused() {
    return WavRecorder::getInstance().isPaused();
}

uint32_t getRecordDurationMs() {
    return WavRecorder::getInstance().getDurationMs();
}

float getMicPeakLevel() {
    return WavRecorder::getInstance().getPeakLevel();
}

} // namespace audio
} // namespace cbdos
