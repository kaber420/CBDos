# Plan Definitivo: Arquitectura Dual-Firmware (Cartuchos OTA) para espOS32

## 🎯 Objetivo
Ejecutar **DOOM** (y futuros emuladores como NES o Game Boy) en una partición dedicada (`app1`) con el **100% de la CPU, PSRAM y memoria libre**, eliminando cualquier sobrecarga del sistema operativo (LVGL, WiFi, MQTT, etc.).

---

## 🏛️ Arquitectura Dual-Environment (PlatformIO)

El proyecto mantendrá dos entornos de compilación independientes dentro del mismo `platformio.ini`:

```
+-------------------------------------------------------------+
|                      Memoria Flash (16MB)                   |
|                                                             |
|  [0x007000] app0: espOS32 (UI, LVGL, WiFi, MQTT) (6MB)     |
|       │                                                     |
|       │  esp_ota_set_boot_partition(OTA_1) + esp_restart()  |
|       ▼                                                     |
|  [0x607000] app1: DOOM / Emulador Dedicado (6MB)           |
|       │                                                     |
|       │  esp_ota_set_boot_partition(OTA_0) + esp_restart()  |
|       ▼                                                     |
|  [0x007000] app0: Regreso al Sistema Operativo              |
|                                                             |
|  [0xC07000] spiffs / fatfs: Assets, DOOM1.WAD, ROMs (3.8MB)|
+-------------------------------------------------------------+
```

---

## 🛠️ Cambios Específicos a Implementar

### 1. Reestructuración de `platformio.ini`
Separar la configuración base de las dependencias específicas de cada firmware:

* **Bloque base `[env]`**:
  * Placa: `jc3248w535`
  * Flash y PSRAM: Modo `qio_opi`, 16MB, OPI PSRAM
  * Tabla de particiones: `custom_16MB_ota.csv`
* **Entorno `[env:esp32]` (Sistema Operativo)**:
  * `lib_deps`: LVGL, ArduinoJson, PubSubClient, drivers de pantalla/touch.
  * `build_src_filter = +<**/*.cpp> -<DoomLauncher.cpp>`
  * Flasheo por defecto en `app0` (0x007000).
* **Entorno `[env:doom]` (Cartucho)**:
  * `lib_deps`: Únicamente los drivers básicos de pantalla (AXS15231B) y touch (GT911/AXS).
  * `build_src_filter = -<**/*> +<DoomLauncher.cpp> +<esp_lcd_*.c>`
  * `board_upload.offset_address = 0x607000` (Offset exacto calculado de `app1`).

---

### 2. Punto de Entrada Dedicado: `src/DoomLauncher.cpp`
Crear un archivo exclusivo para el cartucho de Doom que actuará como `setup()` y `loop()` de `app1`:

1. **Inicialización Mínima:**
   * Inicializar el bus QSPI y la pantalla AMOLED (AXS15231B) directamente, **sin inicializar LVGL**.
   * Inicializar el driver del panel táctil.
   * Montar la partición de almacenamiento (SPIFFS/FATFS) para leer `DOOM1.WAD`.
2. **Ciclo de Juego:**
   * Invocar `doomgeneric_Create()`.
   * En `loop()` ejecutar `doomgeneric_Tick()`.
3. **Mecanismo de Salida (Regreso al OS):**
   * Al pulsar el botón "Salir" del menú o un gesto táctil definido:
   ```cpp
   esp_ota_set_boot_partition(esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL));
   esp_restart();
   ```

---

### 3. Ajustes en `DoomView.cpp` (Lado espOS32)
El botón "Jugar" en la interfaz del OS simplemente ejecuta el salto a la partición de juegos:
```cpp
esp_ota_set_boot_partition(esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, NULL));
esp_restart();
```

---

## 📋 Flujo de Compilación y Flasheo

1. **Flashear el Sistema Operativo (app0):**
   ```bash
   cd firmware
   pio run -e esp32 -t upload
   ```
2. **Flashear el Cartucho Doom (app1):**
   ```bash
   cd firmware
   pio run -e doom -t upload
   ```

---

## ✅ Ventajas de este Enfoque
* **Cero conflictos de RAM:** Doom dispone de los 8MB de PSRAM y los ~512KB de SRAM interna en su totalidad.
* **Escalabilidad:** En el futuro, agregar un nuevo juego/emulador es tan simple como añadir un nuevo entorno `[env:nes]` o `[env:gameboy]` en `platformio.ini` apuntando a `app1`.
* **Seguridad del OS:** Si el juego crashea, no compromete la memoria ni los archivos de configuración del sistema operativo.
