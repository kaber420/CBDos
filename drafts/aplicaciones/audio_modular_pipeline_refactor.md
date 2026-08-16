# 🎧 Propuesta Arquitectónica: Pipeline Modular de Audio para CBDos

## 📌 Contexto y Diagnóstico Actual
En la versión prototipo actual, [`NativeAudioDriver.cpp`](file:///home/kaber420/Documentos/proyectos/espOS32/firmware/src/Core/NativeAudioDriver.cpp) opera de forma monolítica, mezclando en una sola clase y tarea FreeRTOS:
1. **Capa de Transporte:** Sockets TCP/TLS (`WiFiClient`, `WiFiClientSecure`), resolución de cabeceras HTTP, seguimiento de redirecciones (301/302) y parseo de playlists (`.pls`, `.m3u`).
2. **Capa de Almacenamiento:** Lectura de archivos locales en MicroSD vía SPI/VFS y cálculo de cabeceras ID3v2.
3. **Capa de Códecs:** Detección de syncwords y decodificación por software de tramas MP3 y AAC (Helix).
4. **Capa de Hardware:** Inicialización del periférico I2S y alimentación de descriptores DMA.

Aunque este diseño inicial ofrece **cero copias de memoria intermedias**, a largo plazo dificulta la mantenibilidad, pruebas unitarias y la incorporación de nuevos formatos o fuentes de audio (WAV, FLAC, WebSockets, Bluetooth A2DP Sink).

---

## 🏗️ Arquitectura Propuesta (Capa por Capa)

```text
┌─────────────────────────────────────────────────────────────┐
│ 1. Fuentes de Audio (AudioSource Interface)                │
│    ├── HttpStreamSource  (Sockets, HTTP, 302, PLS, M3U)     │
│    ├── SDFileSource      (MicroSD, FATFS, ID3v2 Parser)     │
│    └── MemoryAudioSource (Sonidos del sistema / UI Clicks)  │
└──────────────────────────────┬──────────────────────────────┘
                               │ (Empuja bytes binarios crudos)
                               ▼
┌─────────────────────────────────────────────────────────────┐
│ RingBuffer elástico en PSRAM (AudioRingBuffer)              │
│ - Tamaño: 64 KB a 128 KB en PSRAM (absorbe jitter y latencia) │
└──────────────────────────────┬──────────────────────────────┘
                               │ (Lee tramas de datos crudos)
                               ▼
┌─────────────────────────────────────────────────────────────┐
│ 2. Motor de Códecs (AudioDecoder Pipeline)                  │
│    ├── MP3DecoderHelix   (Decodifica frames MPEG 1/2 Layer 3)│
│    ├── AACDecoderHelix   (Decodifica frames ADTS / AAC-LC)  │
│    └── WAVDecoder        (Pasa audio PCM sin compresión)     │
└──────────────────────────────┬──────────────────────────────┘
                               │ (Muestras PCM int16_t L/R)
                               ▼
┌─────────────────────────────────────────────────────────────┐
│ 3. Driver Hardware I2S (I2SOutputSink)                      │
│    ├── Configuración de Pines I2S (BCLK=42, LRCK=2, DOUT=41)│
│    ├── Control de Volumen por Software / AGC Digital        │
│    └── DMA TX Buffers (Alimentación directa al hardware)    │
└─────────────────────────────────────────────────────────────┘
```

---

## 🧩 Definición de Interfaces (C++)

### 1. `AudioSource` (Fuente Abstracta de Datos)
```cpp
class AudioSource {
public:
    virtual ~AudioSource() = default;
    virtual bool open(const char* uri) = 0;
    virtual size_t read(uint8_t* buffer, size_t maxLen) = 0;
    virtual bool available() const = 0;
    virtual void close() = 0;
    virtual bool isEOF() const = 0;
};
```

### 2. `AudioDecoder` (Decodificador Abstracto de Audio)
```cpp
struct AudioFrameInfo {
    uint32_t sampleRate;
    uint8_t channels;
    uint32_t bitrate;
};

class AudioDecoder {
public:
    virtual ~AudioDecoder() = default;
    virtual bool init() = 0;
    virtual bool decode(uint8_t** inBuf, int* bytesLeft, int16_t* outPcm, int* outSamples) = 0;
    virtual void reset() = 0;
    virtual AudioFrameInfo getFrameInfo() const = 0;
};
```

### 3. `I2SOutputSink` (Driver Hardware)
```cpp
class I2SOutputSink {
public:
    bool begin(int bclk, int lrck, int dout, uint32_t sampleRate);
    void setSampleRate(uint32_t sampleRate);
    void setVolume(uint8_t volumePct); // 0 - 100%
    size_t write(const int16_t* pcmData, size_t samples);
    void pause();
    void resume();
    void stop();
};
```

---

## 🚀 Plan de Migración para Versiones Futuras
1. **Fase 1 (Aislamiento de Red):** Extraer la lógica de conexión HTTP y resolución de playlists a `HttpStreamClient` sin cambiar el núcleo I2S.
2. **Fase 2 (Buffer Desacoplado):** Implementar `AudioRingBuffer` en PSRAM para independizar completamente el hilo de descarga de red del hilo de decodificación/reproducción I2S.
3. **Fase 3 (Controlador de Volumen Digital):** Agregar atenuación de volumen por software para permitir control de volumen granular desde la UI de CBDos.
