# Especificación Técnica: Ecosistema de Cartuchos MicroPython para CBDos

## 📌 1. Visión General: Filosofía Dual y Niveles de Uso

El **Ecosistema de Cartuchos MicroPython** transforma el dispositivo CBDos (ESP32-S3 / JC3248W535) en una estación de desarrollo e interpretación de código Python autónoma y portátil.

Para equilibrar **seguridad/confianza oficial** con **máxima capacidad gráfica y multimedia**, el ecosistema se divide en dos líneas de cartuchos:

```
┌────────────────────────────────────────────────────────────────────────┐
│                   ECOSISTEMA DE CARTUCHOS CBDos                        │
├────────────────────────────────┬───────────────────────────────────────┤
│ 🔵 LÍNEA OFICIAL (VANILLA)     │ 🟢 LÍNEA CBDos ENHANCED (COMPLETA)     │
├────────────────────────────────┼───────────────────────────────────────┤
│ • micropython_core.bin         │ • micropython_media.bin                │
│   (100% Upstream oficial)      │   (Canvas 2D, Audio I2S, Touch nativo) │
│                                │ • micropython_lvgl.bin                 │
│ • Máxima confianza y seguridad │   (Entorno GUI completo en Python)     │
│ • Para IoT, terminal y cómputo │ • Para desarrollar y jugar en el S3    │
└────────────────────────────────┴───────────────────────────────────────┘
```

### 1.1. Línea Oficial (Vanilla / Upstream)
* **`micropython_core.bin` (Headless & Máxima Potencia):**
  - **Origen:** 100% oficial e intacto descargado de [micropython.org](https://micropython.org) para `ESP32_GENERIC_S3-SPIRAM_OCT`.
  - **Propósito:** Para desarrolladores que priorizan la seguridad de una fuente upstream oficial verificada. Cero código de terceros inyectado.
  - **Recursos:** 100% de CPU libre, casi 8 MB de PSRAM dedicados al heap.
  - **Uso:** Servidores locales, bots, matemáticas, protocolos de radio (LoRa/FLRC), scripts de fondo y automatizaciones IoT.
  - **Retorno al SO:** Mediante la API estándar de particiones de MicroPython (`esp32.Partition.find(...)`) o script `boot.py`.

### 1.2. Línea CBDos Enhanced (Gráficos, Audio y UI Táctil)
* **`micropython_media.bin` (Arcade 2D + Audio I2S + Touch):**
  - **Origen:** Build personalizada de la comunidad CBDos con drivers nativos embebidos en C (Pantalla QSPI AXS15231B, Audio I2S DMA y Touch GT911).
  - **Propósito:** Creación de videojuegos retro en Python (estilo Pygame/PICO-8), visualizadores de audio y sintetizadores.
* **`micropython_lvgl.bin` (Rich GUI con LVGL):**
  - **Origen:** Build con bindings oficiales de LVGL integrados.
  - **Propósito:** Desarrollo de aplicaciones gráficas completas con ventanas, botones, teclados virtuales y dashboards táctiles escritos 100% en Python directamente sobre el ESP32-S3.

---

## 🗺️ 2. Mapa de Memoria y Ubicación de Ranura

Los cartuchos de MicroPython están diseñados para operar en la **Ranura 1 (Slot Grande de 4.0 MB)** o ser cargados dinámicamente desde la tarjeta MicroSD.

| Propiedad | Valor / Detalle |
| :--- | :--- |
| **Ranura Destino en Flash** | **Ranura 1 (app1 / ota_1)** |
| **Offset en Flash** | `0x510000` |
| **Tamaño Máximo de Ranura** | `0x400000` (**4.0 MB**) |
| **Archivos en MicroSD** | `/sd/cartridges/micropython_core.bin`<br>`/sd/cartridges/micropython_media.bin`<br>`/sd/cartridges/micropython_lvgl.bin` |
| **Tamaño Típico del Binario** | ~1.6 MB (Core) a ~2.5 MB (LVGL) |

---

## ⚙️ 3. Características del Runtime y Ecosistema

### 3.1. Gestión de Memoria (Heap & PSRAM)
* **PSRAM Heap Dedicado:** El Garbage Collector (GC) de MicroPython se inicializa mapeando hasta **6-7 MB de PSRAM** para variables, arrays y buffers de datos grandes.
* **Inspección de Memoria:**
  ```python
  import gc, esp32
  print("RAM Libre:", gc.mem_free())
  print("PSRAM Total:", gc.mem_alloc() + gc.mem_free())
  ```

### 3.2. Interfaces de Desarrollo y REPL
1. **USB-CDC REPL (Plug & Play):**
   - Conexión por cable Type-C directa con **Thonny IDE**, **VS Code (extensión MicroPico)** o cualquier emulador de terminal serie a 115200 baudios.
   - Acceso interactivo inmediato a la consola `>>>`.
2. **WebREPL (Inalámbrico):**
   - Servidor WebREPL por WiFi para transferir archivos `.py` y ejecutar comandos por red local sin conectar cables.
3. **Ejecución Automática (`boot.py` y `main.py`):**
   - Búsqueda y ejecución automática de scripts presentes en la raíz de la MicroSD (`/sd/main.py`).

---

## 🔌 4. Integración con Hardware y Módulo `cbdos`

Para asegurar la convivencia con CBDos, el firmware de MicroPython incluye módulos específicos:

### 4.1. Montaje Automático de MicroSD (`/sd`)
Al bootear el cartucho, `boot.py` inicializa el bus SPI/SD y monta la tarjeta en `/sd`:
```python
import os, machine
# Mapeo a los pines de la JC3248W535 (SCK: 12, MISO: 13, MOSI: 11, CS: 10)
from machine import Pin, SPI
import sdcard

spi = SPI(1, baudrate=10000000, sck=Pin(12), mosi=Pin(11), miso=Pin(13))
sd = sdcard.SDCard(spi, Pin(10))
os.mount(sd, "/sd")
print("[MicroPython] MicroSD montada en /sd")
```

### 4.2. Módulo de Sistema `cbdos` (Retorno al SO)
Permite al usuario volver a la interfaz táctil de CBDos desde la consola o desde scripts:
```python
import cbdos

# Restaura la partición de arranque a app0 (0x010000) y reinicia el dispositivo
cbdos.exit()
```

**Implementación en C del módulo `cbdos`:**
```c
#include "esp_ota_ops.h"
#include "esp_system.h"

STATIC mp_obj_t cbdos_exit(void) {
    const esp_partition_t* os_partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
    if (os_partition != NULL) {
        esp_ota_set_boot_partition(os_partition);
    }
    esp_restart();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(cbdos_exit_obj, cbdos_exit);
```

### 4.3. Soporte de Pantalla y Touch
* Driver framebuffer para el panel AMOLED/LCD **AXS15231B (QSPI)** en resolución 320x480.
* Driver I2C para el panel táctil **GT911** expuesto mediante eventos en Python.

---

## 🔄 5. Flujo de Uso del Usuario

```
[ Dashboard CBDos ] 
        │
        ▼ (Toca app "Cartuchos")
[ Gestor de Cartuchos ] 
        │
        ├─► Ranura 1: Muestra "MicroPython v1.23" -> Toca "Iniciar"
        │       └─► El ESP32 bootea MicroPython (Consola REPL USB activa)
        │
        └─► (Opcional) Toca "Instalar" -> Selecciona /sd/cartridges/micropython.bin
                └─► Flasheo ultra-rápido en 2 segundos a la Ranura 1
```

---

## 🛠️ 6. Guía de Generación del Cartucho `.bin`

### Compilación desde el árbol oficial de MicroPython:
1. Clonar el repositorio oficial de MicroPython:
   ```bash
   git clone https://github.com/micropython/micropython.git
   cd micropython/ports/esp32
   ```
2. Configurar la placa base para **ESP32_GENERIC_S3** con soporte `SPIRAM_OCT`:
   ```bash
   make BOARD=ESP32_GENERIC_S3 BOARD_VARIANT=SPIRAM_OCT
   ```
3. Obtener el archivo binario generado:
   ```bash
   cp build-ESP32_GENERIC_S3-SPIRAM_OCT/micropython.bin /ruta/a/tu/sd/cartridges/micropython.bin
   ```
4. Flashear directamente desde la pantalla de CBDos o mediante PlatformIO:
   ```bash
   # Flasheo por cable al offset de la Ranura 1:
   esptool.py --chip esp32s3 write_flash 0x510000 micropython.bin
   ```

---

## 📊 7. Comparativa: Lua In-App vs MicroPython Cartucho

| Característica | Lua (Dentro de CBDos) | MicroPython (Cartucho Ranura 1) |
| :--- | :--- | :--- |
| **Propósito** | Automatizaciones, widgets y scripts integrados en la UI. | Prototipado Python completo, IoT, scripts complejos y REPL interactivo. |
| **Memoria Disponible** | Comparte heap con CBDos (~1-2 MB). | **8 MB PSRAM dedicados al 100%**. |
| **Herramientas de PC** | Editor de texto interno de CBDos. | **Thonny IDE**, **VS Code**, WebREPL, scripts en SD. |
| **Aislamiento** | Si un script falla, puede afectar la tarea de UI. | **Aislamiento total** (un cuelgue no afecta a CBDos). |
| **Retorno a CBDos** | Cierra la ventana del runner. | `cbdos.exit()` o botón físico de reset. |
