#include "NativeAudioDriver.h"
#include <driver/i2s.h>
#include <SD.h>
#include <WiFiClientSecure.h>
#include <esp_task_wdt.h>
#include "libhelix-mp3/mp3dec.h"
#include "libhelix-aac/aacdec.h"
#include "../UI/UIManager.h"
#include "LVFS_Driver.h"

// ─── Parámetros de DMA ───────────────────────────────────────────────
// 16 buffers × 1024 samples × 2 bytes = 32KB de DMA total
// Cobertura ≈ 185ms a 44.1kHz estéreo — suficiente para que i2s_write
// nunca bloquee más de unos milisegundos.
#define AUDIO_DMA_BUF_COUNT  16
#define AUDIO_DMA_BUF_LEN    1024

// Buffer de lectura SD en PSRAM: 16KB minimiza los accesos al bus SPI
#define READ_BUF_SIZE        16384

// pcmBuf: Soporte para MP3 (2304 samples) y AAC/HE-AAC (hasta 4096 samples)
#define PCM_BUF_SAMPLES      4096

static HMP3Decoder hMP3Decoder = nullptr;
static HAACDecoder hAACDecoder = nullptr;

// Helper: Calcular el tamaño total de la cabecera ID3v2 (incluyendo carátulas grandes)
// y avanzar el puntero del archivo FÍSICAMENTE con f.seek() hasta el primer frame de audio MP3 real.
static uint32_t getID3v2Size(File& f) {
    lv_fs_spi_lock();
    f.seek(0);
    uint8_t header[10];
    int readBytes = f.read(header, 10);
    lv_fs_spi_unlock();

    if (readBytes < 10) return 0;

    if (header[0] == 'I' && header[1] == 'D' && header[2] == '3') {
        uint32_t id3Size = ((header[6] & 0x7F) << 21) |
                           ((header[7] & 0x7F) << 14) |
                           ((header[8] & 0x7F) << 7)  |
                            (header[9] & 0x7F);
        return id3Size + 10; // +10 bytes del header ID3v2
    }
    return 0;
}

static int probeMP3SampleRate(File& f, HMP3Decoder dec, uint32_t id3Offset) {
    const int PROBE_SIZE = 4096;
    uint8_t* probeBuf = (uint8_t*)malloc(PROBE_SIZE);
    if (!probeBuf) {
        lv_fs_spi_lock(); f.seek(id3Offset); lv_fs_spi_unlock();
        return 44100;
    }

    lv_fs_spi_lock();
    f.seek(id3Offset);
    int bytesRead = f.read(probeBuf, PROBE_SIZE);
    // Volver al offset de inicio de audio MP3
    f.seek(id3Offset);
    lv_fs_spi_unlock();

    if (bytesRead <= 0) { free(probeBuf); return 44100; }

    uint8_t* ptr = probeBuf;
    int left = bytesRead;

    int offset = MP3FindSyncWord(ptr, left);
    if (offset < 0) { free(probeBuf); return 44100; }
    ptr += offset; left -= offset;

    int16_t* tmpBuf = (int16_t*)malloc(PCM_BUF_SAMPLES * sizeof(int16_t));
    int sampRate = 44100;
    if (tmpBuf) {
        int err = MP3Decode(dec, &ptr, &left, tmpBuf, 0);
        if (err == ERR_MP3_NONE) {
            MP3FrameInfo info;
            MP3GetLastFrameInfo(dec, &info);
            if (info.samprate > 0) sampRate = info.samprate;
            Serial.printf("[Audio] Probe: %d Hz, %d ch, %d kbps\n",
                          info.samprate, info.nChans, info.bitrate / 1000);
        }
        free(tmpBuf);
    }
    free(probeBuf);
    return sampRate;
}

// ─────────────────────────────────────────────────────────────────────
bool NativeAudioDriver::begin(int bclk, int lrck, int dout, int sampleRate) {
    if (initialized) return true;
    _bclk = bclk; _lrck = lrck; _dout = dout;

    if (!hMP3Decoder) {
        hMP3Decoder = MP3InitDecoder();
        if (!hMP3Decoder) {
            Serial.println("[Audio] ERROR: MP3InitDecoder fallo");
            return false;
        }
    }

    if (!hAACDecoder) {
        hAACDecoder = AACInitDecoder();
        if (!hAACDecoder) {
            Serial.println("[Audio] ERROR: AACInitDecoder fallo");
            return false;
        }
    }

    initialized = true;
    Serial.printf("[Audio] Decodificadores Helix (MP3 + AAC) listos. Pins I2S: BCLK=%d LRCK=%d DOUT=%d\n",
                  bclk, lrck, dout);
    return true;
}

// Instala el driver I2S con la sample rate real del archivo
static bool installI2S(int bclk, int lrck, int dout, int sampleRate) {
    // Si ya está instalado, desinstalarlo primero para reconfigurarlo
    i2s_driver_uninstall(I2S_NUM_0);

    i2s_config_t cfg = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate          = (uint32_t)sampleRate,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count        = AUDIO_DMA_BUF_COUNT,
        .dma_buf_len          = AUDIO_DMA_BUF_LEN,
        .use_apll             = false,
        .tx_desc_auto_clear   = true,
        .fixed_mclk           = 0
    };

    i2s_pin_config_t pins = {
        .bck_io_num   = bclk,
        .ws_io_num    = lrck,
        .data_out_num = dout,
        .data_in_num  = I2S_PIN_NO_CHANGE
    };

    esp_err_t err = i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("[Audio] i2s_driver_install error: 0x%x\n", err);
        return false;
    }
    err = i2s_set_pin(I2S_NUM_0, &pins);
    if (err != ESP_OK) {
        Serial.printf("[Audio] i2s_set_pin error: 0x%x\n", err);
        return false;
    }
    Serial.printf("[Audio] I2S instalado a %d Hz, DMA %dx%d = %d bytes\n",
                  sampleRate,
                  AUDIO_DMA_BUF_COUNT, AUDIO_DMA_BUF_LEN,
                  AUDIO_DMA_BUF_COUNT * AUDIO_DMA_BUF_LEN * 2);
    return true;
}

// ─────────────────────────────────────────────────────────────────────
#include <WiFi.h>
#include <WiFiClient.h>

void NativeAudioDriver::playMP3(const char* filePath) {
    if (!initialized) begin();
    stop();

    currentFilePath = String(filePath);
    _isStream = false;
    playing = true;

    xTaskCreatePinnedToCore(
        audioTask,
        "AudioTask",
        16384,
        this,
        2,
        &audioTaskHandle,
        0  // Core 0 — LVGL corre en Core 1
    );
}

void NativeAudioDriver::playStream(const char* url) {
    if (!initialized) begin();
    stop();

    currentFilePath = String(url);
    _isStream = true;
    playing = true;

    xTaskCreatePinnedToCore(
        streamAudioTask,
        "StreamTask",
        20480,
        this,
        2,
        &audioTaskHandle,
        0  // Core 0
    );
}

void NativeAudioDriver::stop() {
    playing = false;
    if (audioTaskHandle) {
        vTaskDelay(pdMS_TO_TICKS(100));
        audioTaskHandle = nullptr;
    }
}

// ─────────────────────────────────────────────────────────────────────
void NativeAudioDriver::audioTask(void* param) {
    NativeAudioDriver* driver = (NativeAudioDriver*)param;

    // Registrar en el watchdog para poder alimentarlo manualmente
    esp_task_wdt_add(NULL);

    // ── Resolver archivo por índice de directorio ────────────────────
    // Formato del path: "sdidx:/carpeta:N"  (sin usar el nombre del archivo)
    // Esto hace el sistema 100% inmune a nombres con caracteres no-ASCII.
    String descriptor = driver->currentFilePath;

    File f;

    if (descriptor.startsWith("sdidx:")) {
        // Parsear: "sdidx:/dir:index"
        // Ejemplo: "sdidx:/musica:3"  o  "sdidx:/:0"
        String inner = descriptor.substring(6); // quitar "sdidx:"
        int sep = inner.lastIndexOf(':');
        String dirPath  = inner.substring(0, sep);   // "/musica" o "/"
        int    fileIdx  = inner.substring(sep + 1).toInt();

        Serial.printf("[Audio] Abriendo por indice: dir=%s idx=%d\n",
                      dirPath.c_str(), fileIdx);

        lv_fs_spi_lock();
        File dir = SD.open(dirPath);
        if (!dir) {
            lv_fs_spi_unlock();
            Serial.printf("[Audio] No se pudo abrir directorio: %s\n", dirPath.c_str());
            driver->playing = false;
            esp_task_wdt_delete(NULL);
            vTaskDelete(NULL);
            return;
        }

        // Iterar hasta el archivo en posición fileIdx
        int current = 0;
        File candidate = dir.openNextFile();
        while (candidate) {
            String cName = candidate.name();
            String cLow  = cName; cLow.toLowerCase();
            bool isMedia = !candidate.isDirectory() &&
                           (cLow.endsWith(".mp3") || cLow.endsWith(".wav") ||
                            cLow.endsWith(".jpg") || cLow.endsWith(".png") || cLow.endsWith(".bmp") || cLow.endsWith(".gif"));
            if (isMedia) {
                if (current == fileIdx) {
                    f = candidate;   // Encontrado — no cerrar
                    break;
                }
                current++;
            }
            candidate.close();
            candidate = dir.openNextFile();
        }
        dir.close();
        lv_fs_spi_unlock();

    } else {
        // Fallback legacy: ruta directa (solo archivos con nombres ASCII)
        String path = descriptor;
        if (path.startsWith("A:/")) path = path.substring(2);
        if (!path.startsWith("/"))  path = "/" + path;
        lv_fs_spi_lock();
        f = SD.open(path);
        lv_fs_spi_unlock();
        if (!f) {
            Serial.printf("[Audio] ERROR: SD.open fallo para ruta: '%s'\n", path.c_str());
        }
    }

    if (!f) {
        Serial.printf("[Audio] Fallo reproduccion: %s\n", descriptor.c_str());
        driver->playing = false;
        esp_task_wdt_delete(NULL);
        vTaskDelete(NULL);
        return;
    }

    // ── 1. Calcular offset ID3v2 y probar sample rate real ─────────
    uint32_t id3Offset = getID3v2Size(f);
    int sampRate = probeMP3SampleRate(f, hMP3Decoder, id3Offset);

    // ── 2. Instalar I2S a la frecuencia exacta del archivo ───────────
    if (!installI2S(driver->_bclk, driver->_lrck, driver->_dout, sampRate)) {
        f.close();
        driver->playing = false;
        esp_task_wdt_delete(NULL);
        vTaskDelete(NULL);
        return;
    }

    Serial.printf("[Audio] Reproduciendo: %s\n", descriptor.c_str());

    // ── 3. Alojar buffers en PSRAM ───────────────────────────────────
    uint8_t* readBuf = (uint8_t*)ps_malloc(READ_BUF_SIZE);
    int16_t* pcmBuf  = (int16_t*)ps_malloc(PCM_BUF_SAMPLES * sizeof(int16_t));

    if (!readBuf || !pcmBuf) {
        Serial.println("[Audio] ps_malloc fallo");
        if (readBuf) free(readBuf);
        if (pcmBuf)  free(pcmBuf);
        f.close();
        i2s_driver_uninstall(I2S_NUM_0);
        driver->playing = false;
        esp_task_wdt_delete(NULL);
        vTaskDelete(NULL);
        return;
    }

    uint8_t* readPtr = readBuf;
    int bytesLeft    = 0;
    size_t  written  = 0;
    bool eofReached  = false;

    // ── 4. Loop de decodificación y reproducción ─────────────────────
    while (driver->playing && (!eofReached || bytesLeft > 0)) {
        esp_task_wdt_reset(); // Alimentar watchdog en cada frame

        // Rellenar readBuf desde SD cuando quede menos de la mitad
        if (bytesLeft < READ_BUF_SIZE / 2 && !eofReached) {
            memmove(readBuf, readPtr, bytesLeft);
            lv_fs_spi_lock();
            int got = f.read(readBuf + bytesLeft, READ_BUF_SIZE - bytesLeft);
            lv_fs_spi_unlock();
            if (got > 0) {
                bytesLeft += got;
            } else {
                eofReached = true;
            }
            readPtr = readBuf;
        }

        // Buscar sync word MP3
        int offset = MP3FindSyncWord(readPtr, bytesLeft);
        if (offset < 0) { bytesLeft = 0; continue; }
        readPtr   += offset;
        bytesLeft -= offset;

        // Decodificar frame
        int err = MP3Decode(hMP3Decoder, &readPtr, &bytesLeft, pcmBuf, 0);
        if (err == ERR_MP3_NONE) {
            MP3FrameInfo info;
            MP3GetLastFrameInfo(hMP3Decoder, &info);

            // Escribir PCM al I2S. 200ms de timeout — nunca portMAX_DELAY
            int pcmBytes = info.outputSamps * sizeof(int16_t);
            i2s_write(I2S_NUM_0, (const char*)pcmBuf, pcmBytes, &written,
                      pdMS_TO_TICKS(200));
        }
        // Ignorar errores de underflow de datos (ERR_MP3_INDATA_UNDERFLOW)
        // El próximo ciclo rellenará el buffer

        // Ceder CPU brevemente para que el IDLE task del Core 0
        // pueda correr y alimentar el Watchdog del sistema
        taskYIELD();
    }

    // ── 5. Limpieza ──────────────────────────────────────────────────
    i2s_zero_dma_buffer(I2S_NUM_0);
    free(readBuf);
    free(pcmBuf);
    lv_fs_spi_lock();
    f.close();
    lv_fs_spi_unlock();
    driver->playing = false;
    Serial.println("[Audio] Reproduccion finalizada");
    esp_task_wdt_delete(NULL);
    vTaskDelete(NULL);
}

// ─────────────────────────────────────────────────────────────────────
// Streaming de Radio Online por HTTP MP3 (Icecast / Shoutcast)
// ─────────────────────────────────────────────────────────────────────
#define STREAM_BUF_SIZE 49152 // 48KB en PSRAM para absorber jitter de red

void NativeAudioDriver::streamAudioTask(void* param) {
    NativeAudioDriver* driver = (NativeAudioDriver*)param;
    esp_task_wdt_add(NULL);

    String targetUrl = driver->currentFilePath;
    Serial.printf("[AudioStream] Iniciando stream: %s\n", targetUrl.c_str());

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[AudioStream] ERROR: WiFi no conectado");
        driver->playing = false;
        esp_task_wdt_delete(NULL);
        vTaskDelete(NULL);
        return;
    }

    WiFiClient* client = nullptr;
    WiFiClientSecure* secClient = nullptr;
    WiFiClient* plainClient = nullptr;
    bool isAacHeader = false;

    // Bucle de resolución de Redirecciones HTTP (301, 302, 307) y Playlists (.pls, .m3u)
    int maxRedirects = 5;
    bool streamConnected = false;

    for (int r = 0; r < maxRedirects && driver->playing; r++) {
        esp_task_wdt_reset();
        String host = "";
        int port = 80;
        String path = "/";
        bool isHttps = false;

        String currentUrl = targetUrl;
        if (currentUrl.startsWith("http://")) {
            currentUrl = currentUrl.substring(7);
            isHttps = false;
        } else if (currentUrl.startsWith("https://")) {
            currentUrl = currentUrl.substring(8);
            port = 443;
            isHttps = true;
        }

        int slashIdx = currentUrl.indexOf('/');
        if (slashIdx > 0) {
            host = currentUrl.substring(0, slashIdx);
            path = currentUrl.substring(slashIdx);
        } else {
            host = currentUrl;
            path = "/";
        }

        int colonIdx = host.indexOf(':');
        if (colonIdx > 0) {
            port = host.substring(colonIdx + 1).toInt();
            host = host.substring(0, colonIdx);
        }

        Serial.printf("[AudioStream] Conectando a %s:%d %s (HTTPS: %s)\n",
                      host.c_str(), port, path.c_str(), isHttps ? "SI" : "NO");

        if (client) {
            client->stop();
            delete client;
            client = nullptr;
        }

        if (isHttps || port == 443) {
            secClient = new WiFiClientSecure();
            secClient->setInsecure();
            secClient->setTimeout(5000);
            client = secClient;
        } else {
            plainClient = new WiFiClient();
            plainClient->setTimeout(5000);
            client = plainClient;
        }

        if (!client->connect(host.c_str(), port)) {
            Serial.printf("[AudioStream] ERROR: No se pudo conectar a %s:%d\n", host.c_str(), port);
            break;
        }

        // Enviar petición HTTP
        client->printf("GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: CBDos-Radio/1.0\r\nAccept: */*\r\nIcy-MetaData: 0\r\nConnection: close\r\n\r\n",
                       path.c_str(), host.c_str());

        // Leer cabeceras HTTP
        bool inHeader = true;
        bool isRedirect = false;
        bool isPlaylist = false;
        String redirectUrl = "";
        uint32_t headerTimeout = millis();

        while (client->connected() && inHeader && (millis() - headerTimeout < 6000)) {
            esp_task_wdt_reset();
            if (client->available()) {
                String line = client->readStringUntil('\n');
                line.trim();

                if (line.startsWith("HTTP/1.") || line.startsWith("ICY ")) {
                    int sp1 = line.indexOf(' ');
                    if (sp1 > 0) {
                        int code = line.substring(sp1 + 1, sp1 + 4).toInt();
                        if (code == 301 || code == 302 || code == 303 || code == 307 || code == 308) {
                            isRedirect = true;
                        }
                    }
                } else if (line.startsWith("Location:") || line.startsWith("location:")) {
                    redirectUrl = line.substring(9);
                    redirectUrl.trim();
                    isRedirect = true;
                } else if (line.startsWith("Content-Type:") || line.startsWith("content-type:")) {
                    String cType = line.substring(13);
                    cType.toLowerCase();
                    if (cType.indexOf("aac") >= 0 || cType.indexOf("mp4") >= 0 || cType.indexOf("m4a") >= 0) {
                        isAacHeader = true;
                    }
                    if (cType.indexOf("scpls") >= 0 || cType.indexOf("mpegurl") >= 0 || cType.indexOf("playlist") >= 0) {
                        isPlaylist = true;
                    }
                }

                if (line.length() == 0) {
                    inHeader = false;
                    break;
                }
            } else {
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        }

        if (inHeader) {
            Serial.println("[AudioStream] Timeout leyendo cabeceras HTTP");
            break;
        }

        // Si fue una redirección HTTP (Location: ...)
        if (isRedirect && redirectUrl.length() > 0) {
            Serial.printf("[AudioStream] Redireccion HTTP detectada -> %s\n", redirectUrl.c_str());
            targetUrl = redirectUrl;
            continue;
        }

        // Si fue una respuesta con playlist en el body (.pls o .m3u)
        if (isPlaylist || targetUrl.endsWith(".pls") || targetUrl.endsWith(".m3u")) {
            Serial.println("[AudioStream] Parseando archivo de playlist...");
            String playlistStreamUrl = "";
            uint32_t plStart = millis();
            while (client->connected() && (millis() - plStart < 3000) && playlistStreamUrl.length() == 0) {
                esp_task_wdt_reset();
                if (client->available()) {
                    String plLine = client->readStringUntil('\n');
                    plLine.trim();
                    if (plLine.startsWith("File1=") || plLine.startsWith("file1=")) {
                        playlistStreamUrl = plLine.substring(6);
                    } else if (plLine.startsWith("File2=") || plLine.startsWith("file2=")) {
                        if (playlistStreamUrl.length() == 0) playlistStreamUrl = plLine.substring(6);
                    } else if (plLine.startsWith("http://") || plLine.startsWith("https://")) {
                        playlistStreamUrl = plLine;
                    }
                } else {
                    vTaskDelay(pdMS_TO_TICKS(10));
                }
            }
            if (playlistStreamUrl.length() > 0) {
                playlistStreamUrl.trim();
                Serial.printf("[AudioStream] Stream real extraido de playlist -> %s\n", playlistStreamUrl.c_str());
                targetUrl = playlistStreamUrl;
                continue;
            }
        }

        // Si llegamos aquí, la conexión directa al audio stream está establecida
        streamConnected = true;
        break;
    }

    if (!streamConnected || !client || !client->connected()) {
        Serial.println("[AudioStream] ERROR: No se pudo establecer conexion de stream final");
        if (client) {
            client->stop();
            delete client;
        }
        driver->playing = false;
        esp_task_wdt_delete(NULL);
        vTaskDelete(NULL);
        return;
    }

    Serial.println("[AudioStream] Cabeceras recibidas. Pre-buferizando stream...");

    uint8_t* streamBuf = (uint8_t*)ps_malloc(STREAM_BUF_SIZE);
    int16_t* pcmBuf    = (int16_t*)ps_malloc(PCM_BUF_SAMPLES * sizeof(int16_t));

    if (!streamBuf || !pcmBuf) {
        Serial.println("[AudioStream] ERROR: ps_malloc fallo");
        if (streamBuf) free(streamBuf);
        if (pcmBuf) free(pcmBuf);
        client->stop();
        delete client;
        driver->playing = false;
        esp_task_wdt_delete(NULL);
        vTaskDelete(NULL);
        return;
    }

    uint8_t* readPtr = streamBuf;
    int bytesLeft = 0;
    size_t written = 0;

    // Pre-buferizar 16KB para arranque suave
    uint32_t bufferStart = millis();
    while (driver->playing && client->connected() && bytesLeft < 16384 && (millis() - bufferStart < 8000)) {
        esp_task_wdt_reset();
        int avail = client->available();
        if (avail > 0) {
            int toRead = min(avail, (int)(STREAM_BUF_SIZE - bytesLeft));
            int got = client->read(streamBuf + bytesLeft, toRead);
            if (got > 0) bytesLeft += got;
        } else {
            vTaskDelay(pdMS_TO_TICKS(15));
        }
    }

    // Detectar codec y frecuencia de muestreo del stream
    enum StreamCodec {
        CODEC_AUTO = 0,
        CODEC_MP3,
        CODEC_AAC
    };
    StreamCodec codec = CODEC_AUTO;
    int sampRate = 44100;

    // 1. Probar AAC si la cabecera lo sugirió o si encontramos SyncWord de AAC
    if (isAacHeader || AACFindSyncWord(streamBuf, bytesLeft) >= 0) {
        int aacOffset = AACFindSyncWord(streamBuf, bytesLeft);
        if (aacOffset >= 0) {
            uint8_t* tmpPtr = streamBuf + aacOffset;
            int tmpLeft = bytesLeft - aacOffset;
            if (hAACDecoder) {
                int err = AACDecode(hAACDecoder, &tmpPtr, &tmpLeft, pcmBuf);
                if (err == ERR_AAC_NONE) {
                    AACFrameInfo aInfo;
                    AACGetLastFrameInfo(hAACDecoder, &aInfo);
                    if (aInfo.sampRateOut > 0) sampRate = aInfo.sampRateOut;
                    codec = CODEC_AAC;
                    Serial.printf("[AudioStream] Detectado stream AAC/AAC+ a %d Hz, %d canales\n", sampRate, aInfo.nChans);
                }
            }
        }
    }

    // 2. Si no es AAC, probar MP3
    if (codec == CODEC_AUTO) {
        int mp3Offset = MP3FindSyncWord(streamBuf, bytesLeft);
        if (mp3Offset >= 0) {
            uint8_t* tmpPtr = streamBuf + mp3Offset;
            int tmpLeft = bytesLeft - mp3Offset;
            if (hMP3Decoder) {
                int err = MP3Decode(hMP3Decoder, &tmpPtr, &tmpLeft, pcmBuf, 0);
                if (err == ERR_MP3_NONE) {
                    MP3FrameInfo mInfo;
                    MP3GetLastFrameInfo(hMP3Decoder, &mInfo);
                    if (mInfo.samprate > 0) sampRate = mInfo.samprate;
                    codec = CODEC_MP3;
                    Serial.printf("[AudioStream] Detectado stream MP3 a %d Hz, %d canales, %d kbps\n", sampRate, mInfo.nChans, mInfo.bitrate / 1000);
                }
            }
        }
    }

    // 3. Fallback por defecto si no pudo determinarse antes
    if (codec == CODEC_AUTO) {
        codec = isAacHeader ? CODEC_AAC : CODEC_MP3;
    }

    if (!installI2S(driver->_bclk, driver->_lrck, driver->_dout, sampRate)) {
        free(streamBuf);
        free(pcmBuf);
        client->stop();
        delete client;
        driver->playing = false;
        esp_task_wdt_delete(NULL);
        vTaskDelete(NULL);
        return;
    }

    int currentSampleRate = sampRate;
    Serial.printf("[AudioStream] Streaming activo (%s) a %d Hz\n",
                  (codec == CODEC_AAC) ? "AAC" : "MP3", currentSampleRate);

    // Bucle principal de decodificación de stream
    while (driver->playing && client->connected()) {
        esp_task_wdt_reset();

        // Rellenar streamBuf desde la red si hay espacio
        if (bytesLeft < STREAM_BUF_SIZE / 2) {
            memmove(streamBuf, readPtr, bytesLeft);
            readPtr = streamBuf;

            int avail = client->available();
            if (avail > 0) {
                int toRead = min(avail, (int)(STREAM_BUF_SIZE - bytesLeft));
                int got = client->read(streamBuf + bytesLeft, toRead);
                if (got > 0) bytesLeft += got;
            }
        }

        // Si no hay suficientes datos para decodificar, esperar un poco
        if (bytesLeft < 2048) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        if (codec == CODEC_AAC) {
            int offset = AACFindSyncWord(readPtr, bytesLeft);
            if (offset < 0) {
                bytesLeft = 0;
                vTaskDelay(pdMS_TO_TICKS(5));
                continue;
            }
            readPtr += offset;
            bytesLeft -= offset;

            int err = AACDecode(hAACDecoder, &readPtr, &bytesLeft, pcmBuf);
            if (err == ERR_AAC_NONE) {
                AACFrameInfo info;
                AACGetLastFrameInfo(hAACDecoder, &info);
                if (info.sampRateOut > 0 && info.sampRateOut != currentSampleRate) {
                    currentSampleRate = info.sampRateOut;
                    i2s_set_sample_rates(I2S_NUM_0, currentSampleRate);
                    Serial.printf("[AudioStream] AAC sample rate actualizado: %d Hz\n", currentSampleRate);
                }
                int pcmBytes = info.outputSamps * sizeof(int16_t);
                i2s_write(I2S_NUM_0, (const char*)pcmBuf, pcmBytes, &written, pdMS_TO_TICKS(200));
            } else if (err != ERR_AAC_INDATA_UNDERFLOW) {
                readPtr++;
                bytesLeft--;
            }
        } else { // CODEC_MP3
            int offset = MP3FindSyncWord(readPtr, bytesLeft);
            if (offset < 0) {
                bytesLeft = 0;
                vTaskDelay(pdMS_TO_TICKS(5));
                continue;
            }
            readPtr += offset;
            bytesLeft -= offset;

            int err = MP3Decode(hMP3Decoder, &readPtr, &bytesLeft, pcmBuf, 0);
            if (err == ERR_MP3_NONE) {
                MP3FrameInfo info;
                MP3GetLastFrameInfo(hMP3Decoder, &info);
                if (info.samprate > 0 && info.samprate != currentSampleRate) {
                    currentSampleRate = info.samprate;
                    i2s_set_sample_rates(I2S_NUM_0, currentSampleRate);
                    Serial.printf("[AudioStream] MP3 sample rate actualizado: %d Hz\n", currentSampleRate);
                }
                int pcmBytes = info.outputSamps * sizeof(int16_t);
                i2s_write(I2S_NUM_0, (const char*)pcmBuf, pcmBytes, &written, pdMS_TO_TICKS(200));
            } else if (err != ERR_MP3_INDATA_UNDERFLOW) {
                readPtr++;
                bytesLeft--;
            }
        }

        taskYIELD();
    }

    i2s_zero_dma_buffer(I2S_NUM_0);
    free(streamBuf);
    free(pcmBuf);
    client->stop();
    delete client;
    driver->playing = false;
    Serial.println("[AudioStream] Stream finalizado");
    esp_task_wdt_delete(NULL);
    vTaskDelete(NULL);
}
