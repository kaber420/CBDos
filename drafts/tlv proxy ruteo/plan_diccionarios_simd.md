# Plan de Implementación: Método Híbrido Unificado de Diccionarios (Plan 3 Oficial) y Aceleración SIMD

Este plan especifica la implementación del **Método Híbrido Unificado (Plan 3 Oficial)** definido en `pseudohtml_tags_plan.md`, optimizado en C y C++ nativo para la arquitectura **ESP32-S3 (Xtensa LX7)**.

---

## 📐 Estructura Matemática del Método Híbrido (128 Tokens Superiores: `0x80` a `0xFF`)

```
┌──────────────────────────────────────────────────────────────────────────┐
│ Rango VIP / Ultra-Corto (0x80 - 0xBF) -> 64 Valores | 1 Byte Total      │
│  Atajos universales de 1 solo byte: "https://", ".com", "www.", "para"   │
├──────────────────────────────────────────────────────────────────────────┤
│ Rango Core Local (0xC0 - 0xDF)       -> 32 Bloques | 2 Bytes Totales    │
│  32 x 256 = 8,192 atajos de 2 bytes para vocabulario común local.        │
├──────────────────────────────────────────────────────────────────────────┤
│ Rango Diccionarios (0xE0 - 0xFF)     -> 32 Seleccionadores | 3 Bytes    │
│  32 x 65,536 = 2.1 Millones de atajos aislados (Idiomas, Macro-URLs).   │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## Principios de Diseño

* **Cero Asignación de Memoria Dinámica (No Malloc en Loop):** Toda la descompresión ocurre directamente en buffers de pila (Stack) o memoria Flash constante (`PROGMEM`), protegiendo los 60 FPS de LVGL v9.
* **Aceleración SIMD ESP-DSP:** Búsqueda en paralelo de marcadores `0x80..0xFF` procesando bloques de **16 bytes por ciclo de reloj** mediante registros vectoriales de 128-bits del procesador Xtensa LX7.

---

## Cambios Propuestos

### Core Firmware (C/C++ en espOS32)

#### 1. `tlv_dictionary.h` (C/C++)
- Tablas en Flash (`PROGMEM`) para los 64 Atajos VIP de 1 Byte (`0x80` - `0xBF`).
- Tablas para los 8,192 Atajos Core de 2 Bytes (`0xC0` - `0xDF`).
- Función de descompresión: `size_t tlv_decode_hybrid_text(const uint8_t* in, size_t in_len, char* out, size_t max_out)`.

#### 2. `tlv_dictionary.c` (C/C++)
- Implementación de la descompresión híbrida de 3 niveles (1B, 2B y 3B).
- Optimización con bucles vectoriales SIMD para el procesador Xtensa LX7.

#### 3. `tlv_parser.c` (C/C++)
- Integrar `tlv_decode_hybrid_text` en todas las etiquetas que renderizan texto (`TYPE_ABS_TEXT`, `TYPE_ABS_LINK`, `TYPE_ABS_CHECKBOX`, `TYPE_ABS_DROPDOWN`).

---

### Gateway (Python)

#### `gateway_prototype.py`
- Módulo de compresión de texto en Python que sustituye texto plano HTML por tokens VIP de 1 Byte (`0x80`-`0xBF`) y Core de 2 Bytes (`0xC0`-`0xDF`) antes de transmitir, reduciendo el ancho de banda por aire en un 50% a 70%.

---

## Plan de Verificación

### Pruebas Automatizadas
- Compilación limpia PlatformIO para ESP32-S3 (`-e esp32`):
  ```bash
  cd firmware
  pio run -e esp32
  ```

### Verificación Manual
- Transmitir un payload con tokens VIP `0x80` (`https://`), `0x81` (`.com`), etc., y verificar que en la pantalla del ESP32-S3 aparezcan las palabras y URLs completas descomprimiéndose a 60 FPS.
