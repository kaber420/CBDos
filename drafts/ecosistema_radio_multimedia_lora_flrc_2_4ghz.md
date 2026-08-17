# Ecosistema Multimedia y Radio Digital Privada (2.4 GHz FLRC / LoRa + Opus)

> **Documento de Diseño y Arquitectura de Telecomunicaciones**  
> **Proyecto:** espOS32 (CBDos)  
> **Área:** Conectividad RF / Audio Nativo / Redes Descentralizadas  

---

## 1. Visión General y Diferenciación

A diferencia de las redes tradicionales en Sub-GHz (433 / 868 / 915 MHz como Meshtastic) que están limitadas por ancho de banda a **1–5 kbps** (restringidas a telemetría y mensajes de texto cortos), el ecosistema en **2.4 GHz (Semtech SX1280 / SX1281)** permite tasas de transferencia de **260 kbps a 1.3 Mbps**.

Esta velocidad, combinada con la eficiencia del códec **Opus** y la potencia de cómputo del **ESP32-S3**, permite crear una **red de telecomunicaciones multimedia privada, autónoma y descentralizada** capaz de transmitir:

1. **Radio Digital Continua en Tiempo Real (Broadcast/Multicast)** con calidad estéreo de estudio.
2. **Metadatos y Sistema RDS embebido** (títulos, avisos de emergencia, clima).
3. **Walkie-Talkie Digital PTT (Push-to-Talk)** de alta fidelidad.
4. **Transmisión rápida de imágenes** (WebP / JPEG en menos de 1 segundo).
5. **Distribución aérea de archivos y cartuchos/ROMs** para CBDos.

```
┌──────────────────────────────────────────────────────────────────────────┐
│                   ESTACIÓN BASE TRANSMISORA (EMISOR)                     │
│  [Fuente Audio / Servidor] ──► [Encoder Opus] ──► [E28 SX1280 + PA]      │
│                                                   (Canal 13 / 2472 MHz)  │
└────────────────────────────────────┬─────────────────────────────────────┘
                                     │  Antena Sectorial 120° (+14 dBi)
                                     ▼  Potencia Legal: 4W PIRE (+36 dBm)
                         📡)) Ondas RF en el aire ((📡
                (Broadcast Multicast sin ACKs / Unidireccional)
                                     │
         ┌───────────────────────────┼───────────────────────────┐
         ▼                           ▼                           ▼
┌─────────────────┐         ┌─────────────────┐         ┌─────────────────┐
│ RECEPTOR CBDos 1│         │ RECEPTOR CBDos 2│         │ RECEPTOR CBDos N│
│ ESP32-S3 + E28  │         │ ESP32-S3 + E28  │         │ ESP32-S3 + E28  │
│ Opus Decoder    │         │ Opus Decoder    │         │ Opus Decoder    │
│ I2S / LVGL UI   │         │ I2S / LVGL UI   │         │ I2S / LVGL UI   │
└─────────────────┘         └─────────────────┘         └─────────────────┘
```

---

## 2. Capa Física y Espectro de Radiofrecuencia (2.4 GHz)

### 2.1. El Espectro "Virgen" de 10.5 MHz (2473.0 MHz a 2483.5 MHz)
* **El fin del tráfico WiFi convencional:** Como la inmensa mayoría de routers domésticos en México operan como máximo en el **Canal 11 (frecuencia central 2462 MHz, con su lóbulo superior terminando en ~2473 MHz)**, todo el espacio por encima de 2473 MHz está completamente desierto.
* **Segmento disponible:** Desde los **2473.0 MHz hasta el límite superior de la banda ISM en 2483.5 MHz** existen exactamente **10.5 MHz de espectro 100% limpio y libre de colisiones**.
* **Capacidad en este bloque:**
  * En modo **FLRC a 300 kHz (0.3 MHz)** por canal $\rightarrow$ ¡Caben hasta **35 subcanales de radio/datos simultáneos** sin tocarse!
  * En modo **FLRC a 600 kHz (0.6 MHz)** por canal $\rightarrow$ Caben hasta **17 canales multimedia de alta velocidad**.
  * En modo **LoRa 2.4 GHz a 800 kHz (0.8 MHz)** $\rightarrow$ Caben hasta **13 canales de ultra largo alcance**.

### 2.2. Modulaciones RF Disponibles (SX1280)

| Modulación | Ancho de Banda (BW) | Bitrate en Aire | Sensibilidad (Rx) | Uso Principal en el Ecosistema |
| :--- | :---: | :---: | :---: | :--- |
| **FLRC** | **300 kHz** | 260 kbps | -108 dBm | Audio Opus Voz / Música optimizada |
| **FLRC** | **600 kHz** | 520 kbps | -105 dBm | **Estándar recomendado (Audio + RDS + Imágenes)** |
| **FLRC** | **1.2 MHz** | 1.3 Mbps | -102 dBm | Transferencia de archivos de alta velocidad |
| **LoRa 2.4 GHz** | **800 kHz** (SF5/SF6) | 120–200 kbps | -108 a -111 dBm | Máxima penetración urbana con obstáculos densos |

### 2.3. Cumplimiento Normativo Legal en México (IFT / NOM-208)

* **Marco Regulatorio:** Banda de Uso Libre 2400 – 2483.5 MHz (IFT-008-SCFI-2015).
* **Límite Máximo de Potencia Radiada (PIRE / EIRP):** **4 Watts (+36 dBm)** para transmisiones Punto a Multipunto / Sectoriales.

#### Configuración de la Estación Base (100% Legal):
* **Potencia Conducida (Módulo Ebyte E28):** Configurado a **+23 dBm** (~200 mW).
* **Antena Emisora:** Sectorial de 120° con ganancia de **+14 dBi**.
* **Pérdida en cable y conectores:** **-1 dB**.
* **PIRE Total:** $\text{PIRE} = 23\text{ dBm} + 14\text{ dBi} - 1\text{ dB} = \mathbf{+36\text{ dBm} \ (4.0\text{ Watts})}$.

#### Alcance Estimado en el Sector de 120°:
* **Línea de vista directa (LOS / Tejado a Tejado):** **10 a 20 km**.
* **Entorno Urbano / Suburbano (con casas y árboles):** **2 a 4.5 km**.
* **Receptores en Interiores (Indoor con antena de 3–5 dBi en el ESP32):** **1 a 2.5 km**.

---

## 3. Capa de Audio y Códec OPUS

### 3.1. Rendimiento en el ESP32-S3
* **Carga de CPU:** ~18% a 25% de un solo núcleo a 240 MHz (utilizando `libopus` en punto fijo `FIXED_POINT`).
* **Ejecución en Core 0:** Corre de forma autónoma dentro de una tarea FreeRTOS, dejando el Core 1 libre al 100% para LVGL 9.5 y la interfaz táctil.
* **Frecuencia de Muestreo:** Decodificación nativa a **48.000 Hz estéreo**, perfectamente compatible con el bus I2S del ESP32-S3.

### 3.2. Mecanismo de Tolerancia a Pérdida de Paquetes (PLC)
En un esquema de radio difusión (Broadcast Multicast), el emisor **no espera confirmación (ACK)**. Para mitigar interferencias:
1. Cada paquete transporta un número de secuencia (`SeqID` de 16 bits).
2. Si el receptor detecta la pérdida de un paquete ($Seq_{n+1} - Seq_{n} > 1$), invoca el algoritmo nativo de Opus:
   ```c
   opus_decode(decoder, NULL, 0, pcm_out, frame_size, 1 /* FEC/PLC flag */);
   ```
3. El decodificador Opus interpola matemáticamente las frecuencias del audio anterior, haciendo la pérdida de paquetes **prácticamente inaudible** sin clics ni silencios bruscos.

---

## 4. Estructura de Tramas y Protocolo de Paquetes

Para permitir multiplexar audio, metadatos y archivos en el mismo canal RF:

```text
┌──────────────┬──────────────┬──────────────┬──────────────┬───────────────────────────┬──────────────┐
│ SyncWord     │ Type (1B)    │ SeqID (2B)   │ PayloadLen   │ Payload de Datos          │ CRC (2B)     │
│ (4 Bytes)    │ (Audio/Data) │ (0 - 65535)  │ (1 Byte)     │ (Opus / RDS / Image / OTA)│ (Hardware)   │
└──────────────┴──────────────┴──────────────┴──────────────┴───────────────────────────┴──────────────┘
```

### Tipos de Paquetes (`Type`):
* `0x01` - **AUDIO_OPUS_STREAM:** Frame de audio Opus (duración típica 20 ms a 48 kbps $\approx$ 120 bytes).
* `0x02` - **RADIO_RDS_METADATA:** Título de canción, artista, estado de la estación, teletexto.
* `0x03` - **EMERGENCY_BROADCAST:** Alerta prioritaria con interrupción de audio y notificación en pantalla.
* `0x04` - **IMAGE_CHUNK:** Fragmento de imagen WebP/JPEG para mostrar carátula o foto comunitaria.
* `0x05` - **FILE_CHUNK_OTA:** Paquete binario para distribución de cartuchos/ROMs hacia la tarjeta SD.

---

## 5. Capacidades del Ecosistema

### 5.1. Radio Digital Comunitaria (Broadcast)
* Emisión continua 24/7 de música o programación local.
* Cantidad ilimitada de receptores ESP32 sintonizando simultáneamente.

### 5.2. Walkie-Talkie Digital (PTT / Voice)
* Comunicación de voz bidireccional punto a punto o por canales de grupo.
* Opus optimizado para voz (12–16 kbps) con cifrado AES-128 opcional en el payload.

### 5.3. Transmisión Rápida de Imágenes
* Transmisión de fotos de 40 a 80 KB en **0.5 a 1.5 segundos**.
* Visualización directa en el frame buffer de LVGL.

### 5.4. Distribución de Archivos y Juegos
* Distribución inalámbrica de ROMs de Game Boy (ej. 256 KB) o utilidades a receptores en un radio de varios kilómetros en pocos segundos.

---

## 6. Hoja de Ruta de Implementación en CBDos (espOS32)

1. **Fase 1 - Driver de Radio SX1280 (FLRC/LoRa):**
   * Integración de la librería SPI para SX1280 en `firmware/src/Core/` o módulo de red.
   * Configuración de interrupciones DIO1 para recepción de paquetes DMA sin bloqueo.
2. **Fase 2 - Decodificador Opus en `NativeAudioDriver`:**
   * Integración de `libopus` (compilación en punto fijo con memoria en PSRAM).
   * Buffer de Jitter de 60–100 ms en Core 0.
3. **Fase 3 - Interfaz de Usuario LVGL 9.5:**
   * Creación de la app "Radio Digital FLRC" en CBDos.
   * Indicadores de intensidad de señal (RSSI, SNR), frecuencia de sintonía, visualización de metadatos RDS y carátulas recibidas por RF.
