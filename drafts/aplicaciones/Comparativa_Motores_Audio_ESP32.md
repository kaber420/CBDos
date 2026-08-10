# Comparativa de Motores de Audio y Decodificación para ESP32-S3

Este borrador analiza las mejores opciones de librerías para reproducir audio (MP3/WAV/AAC) y mostrar imágenes (JPEG/PNG) en el ESP32-S3 con LVGL v9 y tarjeta MicroSD.

---

## 1. Motores de Audio MP3 / WAV para ESP32-S3

### 🏆 Opción A: ESP-ADF / `esp_audio` (Framework Oficial de Espressif)
* **Descripción:** Es el Framework de Desarrollo de Audio Oficial de Espressif. Utiliza decodificadores optimizados en ensamblador para la arquitectura Tensilica / Vector de ESP32-S3 (DSP Extensions).
* **Consumo de Memoria/CPU:** **Extremadamente Bajo (< 10-15% CPU)**. Usa las instrucciones aceleradas por hardware del ESP32-S3.
* **Formatos:** MP3, AAC, FLAC, WAV, Ogg.
* **Salida:** I2S (Internal DAC / External Codec como ES8388, MAX98357A, PCM5102).
* **Pros:** El mejor rendimiento del mercado, cero tirones en la interfaz gráfica LVGL mientras se reproduce música en segundo plano (Core 0).
* **Contras:** En PlatformIO con Arduino Framework requiere incluir componentes nativos del ESP-IDF (`esp_audio` / `helix-mp3`).

### 🥈 Opción B: `libhelix` / `helix-mp3` (Decodificador Helix en C puro)
* **Descripción:** Decodificador MP3 en punto fijo de la fundación RealNetworks/Helix optimizado para procesadores embebidos de 32 bits.
* **Consumo de CPU:** **Muy Bajo (~ 15-20% CPU)**.
* **Pros:** Muy liviano, cabe directo en el SDK de Arduino sin dependencias pesadas.
* **Contras:** Solo maneja decodificación MP3 bruta; la salida I2S debe escribirse manualmente mediante `i2s_write()`.

### 🥉 Opción C: `ESP32-audioI2S` (Schreibfaul1) / `ESP8266Audio`
* **Descripción:** Librerías populares basadas en wrappers de Helix y Helix-AAC para Arduino ESP32.
* **Consumo de CPU:** **Moderado (20-30% CPU)**.
* **Pros:** Extremadamente fácil de integrar en Arduino (`audio.connecttoFS(SD, "/cancion.mp3")`).
* **Contras:** Ocupa más RAM y buffer que la solución nativa de Espressif.

---

## 2. Decodificación de Imágenes (JPEG / PNG / BMP) para LVGL v9

### 🖼️ A. Decodificador TJpgDecoder / Helix JPEG (Soporte Directo LVGL)
- LVGL v9 soporta integrar `lv_tjpgd` o `TJpg_Decoder` que usa las instrucciones de decodificación rápida por DMA.
- **Ruta de Archivo:** Requiere registrar correctamente el driver de sistema de archivos `LVFS_Driver` asociando `A:/nombre.jpg` para que LVGL abra el archivo directo sin cargarlo completo a la memoria RAM antes de descomprimir.

---

## 3. Recomendación de Arquitectura Final

1. **Audio:** Usar el decodificador **Helix MP3 (ESP-ADF / Helix MP3)** ejecutándose en un bucle asíncrono en **Core 0** (mientras LVGL renderiza en **Core 1**). Esto garantiza reproducción fluida sin interferir con la pantalla táctil.
2. **Imágenes:** Habilitar el decodificador de formato de imagen dinámico en el `LVFS_Driver` para decodificar JPEG directo a la PSRAM.
