# Plan de Implementación: Audio I2S con DMA para DOOM (Cartucho app1)

## 🎯 Objetivo
Habilitar los **Efectos de Sonido (SFX)** originales de DOOM (disparos, monstruos, puertas, explosiones, elevadores) reproduciéndose en tiempo real a través del amplificador/altavoz I2S de la placa JC3248W535, utilizando **acceso directo a memoria (DMA)** para garantizar 0% de sobrecarga en la CPU y cero lag en los FPS del juego.

---

## 📻 Configuración de Hardware (I2S DMA)

El chip ESP32-S3 se conectará al amplificador de audio con la siguiente configuración:

* **Controlador:** `I2S_NUM_0` en modo Maestro Transmisor (`I2S_MODE_MASTER | I2S_MODE_TX`).
* **Pines de Hardware (JC3248W535):**
  * **BCLK (Bit Clock):** `GPIO 42`
  * **LRCK / WS (Word Select):** `GPIO 2`
  * **DOUT (Data Output):** `GPIO 41`
* **Formato de Audio:** PCM 16-bit, Estéreo (Canal Izquierdo y Derecho independientes).
* **Frecuencia de Muestreo (Sample Rate):** `11,025 Hz` (la resolución nativa original de todas las muestras de sonido de DOOM).
* **Buffers DMA:** 4 descriptores DMA de 256 muestras cada uno (latencia ultra baja < 25 ms, imperceptible al jugar).

---

## 🧩 Arquitectura del Mezclador de Sonido (`DG_sound_module`)

DOOM incluye una interfaz estándar para motores de sonido (`sound_module_t`). Crearemos la implementación para el ESP32 en `firmware/lib/doomgeneric/doomgeneric_sound_esp32.c`:

```
+-------------------------------------------------------------------+
|                        Motor de DOOM                              |
|   (Emite eventos: disparo de escopeta, gruñido, puerta, etc.)     |
+---------------------------------+---------------------------------+
                                  │
                                  ▼
+-------------------------------------------------------------------+
|               Mezclador de 8 Canales de Audio                     |
|                                                                   |
|   Canal 0: [Disparo (Vol: 100%, Pan: Centro)]                     |
|   Canal 1: [Monstruo (Vol: 60%, Pan: Izquierda)]                  |
|   Canal 2: [Puerta (Vol: 80%, Pan: Derecha)]                      |
|   ... Canales 3 al 7                                              |
|                                                                   |
|   -> Suma y mezcla matemática de ondas PCM en estéreo 16-bit      |
+---------------------------------+---------------------------------+
                                  │
                                  ▼
+-------------------------------------------------------------------+
|                      Driver Hardware I2S                          |
|   (i2s_write directo a los Buffers DMA del ESP32-S3)              |
+---------------------------------+---------------------------------+
                                  │
                                  ▼
                  [Altavoz / Salida de Audio JC3248W535]
```

### Funciones a implementar:
1. **`DG_Sound_Init()`:**
   - Configura el periférico `I2S_NUM_0` con los pines y la tasa de 11.025 Hz.
   - Limpia los 8 canales del mezclador.
2. **`DG_Sound_StartSound(sfxinfo, channel, vol, sep)`:**
   - Asigna los datos PCM del sonido a reproducir en el canal indicado.
   - Ajusta el volumen (0 a 127) y la separación estéreo (paneo izquierda/derecha `sep` de 0 a 255).
3. **`DG_Sound_StopSound(channel)`:**
   - Detiene el canal si el sonido termina o es interrumpido.
   - Pone el puntero del canal en reposo.
4. **`DG_Sound_Update()`:**
   - Se ejecuta en cada ciclo de pantalla (*Tick*).
   - Genera un bloque de 256 muestras estéreo mezclando todos los canales activos.
   - Envía el bloque al hardware con `i2s_write()` sin bloquear el juego.
5. **`DG_Sound_Shutdown()`:**
   - Libera el periférico I2S de forma limpia al salir al sistema operativo.

---

## 🛠️ Archivos a Modificar / Crear

1. **[NUEVO] `firmware/lib/doomgeneric/doomgeneric_sound_esp32.c`**:
   - Código del mezclador de 8 canales e integración con el driver ESP-IDF `driver/i2s.h`.
2. **[MODIFICAR] `firmware/lib/doomgeneric/i_sound.c`**:
   - Enlazar `&DG_sound_module` para que DOOM reconozca el nuevo módulo de sonido.
3. **[MODIFICAR] `firmware/platformio.ini`**:
   - Activar la bandera de compilación `-DFEATURE_SOUND=1` en el entorno `[env:doom]`.
   - Definir los pines `-DI2S_BCLK=42 -DI2S_LRC=2 -DI2S_DOUT=41`.

---

## 📋 Plan de Verificación

1. **Compilación:** Compilar `pio run -e doom` y verificar que el mezclador se compile sin errores ni advertencias.
2. **Flasheo:** Subir el binario con `pio run -e doom -t upload`.
3. **Prueba de Audio en Vivo:**
   - Al iniciar DOOM, escuchar el sonido del menú inicial.
   - En la partida, disparar la pistola/escopeta y verificar sonido nítido e instantáneo en el altavoz.
   - Comprobar que los FPS del juego se mantengan al 100% fluidos sin interrupciones.
