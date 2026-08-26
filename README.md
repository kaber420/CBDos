# 🦾 CyBerDeck OS — CBDos v0.2.1

**CyBerDeck OS (CBDos)** es un sistema operativo embebido modular, agnóstico y *offline-first* diseñado para cyberdecks, consolas portátiles y dispositivos multimedia basados en microcontroladores ESP32. Incluye soporte para topologías de red federadas, jerárquicas y ruteadas (basadas en Torres/Zonas). Arquitectura `core/` desacoplado de cualquier SDK de hardware, UI en **LVGL v9.5** y soporte multi-target simultáneo.

> **Estado general:** CBDos v0.2.1. El núcleo `core/`, el motor de UI, el reproductor de audio, emuladores y las vistas principales están portados y operativos. Soporte completo multi-target ESP32-P4 y ESP32-S3.

---

## 🏛️ Arquitectura Modular (Core / HAL / BSP)

```
┌──────────────────────────────────────────────────────────────────────────┐
│  APLICACIONES & UI                                                        │
│  SplashScreen · Dashboard · MusicPlayer · RadioView · FlasherView        │
│  ConfigView · WiFiConfig · StorageConfig · WallpaperConfig · Lua Engine  │
├──────────────────────────────────────────────────────────────────────────┤
│  CBDos API  (cbdos::system · display · input · audio · network · fs)     │
│  UIManager · ThemeEngine · WallpaperManager · AudioPlayer (Helix MP3)   │
├──────────────────────────────────────────────────────────────────────────┤
│  HAL Interfaces  (AudioHAL · StorageHAL · NetworkHAL · SystemHAL)        │
│  100% agnóstico — sin dependencias directas a ESP-IDF ni Arduino.h       │
├────────────────────────┬─────────────────────────┬───────────────────────┤
│  BSP: ESP32-P4         │  BSP: ESP32-S3           │  BSP: PC Simulator    │
│  bsp/esp32_p4_jc4880   │  bsp/esp32_s3_jc3248     │  bsp/pc_simulator     │
│  ESP-IDF 5.5 / CMake   │  PlatformIO + Arduino    │  SDL2 / Linux/macOS   │
└────────────────────────┴─────────────────────────┴───────────────────────┘
```

---

## 🎯 Dispositivos y Targets Oficiales

### 📟 Guition JC4880P443C — `bsp/esp32_p4_jc4880` *(target principal)*

| Componente | Detalle |
|:---|:---|
| **SoC** | ESP32-P4 RISC-V Dual-Core @ 400 MHz (Chip Rev 1.3) |
| **Memoria** | 16 MB Flash · 32 MB Hexal-PSRAM |
| **Pantalla** | 4.3" IPS 480×800 MIPI-DSI (ST7701S) @ 60 FPS |
| **Táctil** | Goodix GT911 (I2C SDA=7, SCL=8, RST=3, INT=4) |
| **Audio** | Everest ES8311 (I2C + I2S MCLK=13 BCLK=12 WS=10 DOUT=9) · Amp PA=11 |
| **Almacenamiento** | MicroSD Slot 0 SDMMC 4-bit (GPIO 39-44) + LDO VO4 3.3 V |
| **Coprocesador** | ESP32-C6-MINI (Wi-Fi 6 / BT 5) vía SDIO Slot 1 |
| **Framework** | ESP-IDF 5.5 nativo (CMake / Ninja) |

### 📟 Guition JC3248W535 — `bsp/esp32_s3_jc3248`

| Componente | Detalle |
|:---|:---|
| **SoC** | ESP32-S3 Xtensa Dual-Core @ 240 MHz |
| **Memoria** | 16 MB Flash · 8 MB PSRAM |
| **Pantalla** | 3.5" IPS 320×480 QSPI (AXS15231B) @ 30 FPS |
| **Audio** | PDM TX / Helix MP3 · MicroSD SPI |
| **Framework** | PlatformIO + Arduino Core (pioarduino) |

---

## 📦 Estado de Módulos y Migración

> **Fuente de la verdad original:** `/home/kaber420/Documentos/proyectos/espOS32`
> Toda lógica se porta desde allí, adaptada a la arquitectura agnóstica `cbdos/core/`.

| Módulo / Vista | Descripción | Estado | S3 | P4 |
|:---|:---|:---:|:---:|:---:|
| **Core UI Engine** | `BaseView`, `UIManager`, ciclo de vida de vistas — LVGL 9.5 | ✅ | ✅ | ✅ |
| **Theme Engine** | Paletas dinámicas: Cyberpunk · Dark · Light · Acentos | ✅ | ✅ | ✅ |
| **Wallpaper Engine** | Fondos dinámicos en PSRAM + `WallpaperConfigView` | ✅ | ✅ | ✅ |
| **Splash Screen View** | Animación de booteo + diagnóstico hardware | ✅ | ✅ | ✅ |
| **Dashboard View** | Pantalla principal tipo Cyberdeck + accesos directos | ✅ | ✅ | ✅ |
| **Config View** | Menú maestro de ajustes del sistema | ✅ | ✅ | ✅ |
| **WiFi Config View** | Escaneo, conexión y gestión de credenciales | ✅ 90% | ✅ | 🟡 vía C6 |
| **Storage Config View** | Diagnóstico de particiones, MicroSD, LittleFS | ✅ 95% | ✅ | 🟡 SDMMC |
| **Audio Core (Helix)** | Decodificador MP3 Helix en PSRAM + buffer I2S | ✅ 95% | ✅ | 🟡 ES8311 |
| **Music Player View** | UI de reproductor: lista, scrubber, volumen, carátulas | ✅ 95% | ✅ | ✅ |
| **Radio View** | Radio por internet, lista de emisoras, streaming | 🔄 Portando | 🟡 | 🟡 |
| **Flasher View** | Interfaz gráfica para flashear coprocesadores vía UART | 🔄 Portando | ➖ | 🟡 |
| **Synth Sound Engine** | Motor de síntesis y generador de ondas | 📋 Planificado | ⏳ | ⏳ |
| **File Manager View** | Explorador de archivos universal MicroSD/Flash | ⏳ | ⏳ | ⏳ |
| **System Info Monitor** | Monitor de RAM, Heap, FPS y temperatura en tiempo real | ⏳ | ⏳ | ⏳ |
| **Lua Script Engine** | Intérprete Lua embebido para micro-apps desde SD | ⏳ | ⏳ | ⏳ |

*Leyenda: ✅ Operativo · 🟡 Integración de driver en curso · 🔄 Portando desde espOS32 · 📋 Planificado · ⏳ Pendiente · ➖ No aplica*

---

## 🔧 Herramienta: C6 Flasher Bridge

El ESP32-P4 incluye soporte para flashear el coprocesador inalámbrico **ESP32-C6-MINI** sin necesidad de un adaptador USB-Serie externo, tanto mediante la app integrada **Flasher** en CBDos como a través del puente UART en `tools/c6_flasher_bridge/`.

La placa Guition JC4880P443C expone la cabecera **JP1 (2×13 pines)** que da acceso a las líneas de alimentación, UART y BOOT del C6.

![Diagrama de Conexiones de Flasheo ESP32-C6 en JP1](docs/images/esp32_c6_flasher_diagram.png)

### 🔌 Conexiones Requeridas en la Cabecera JP1

| Cable / Jumper | Origen (Lado P4 / Izq) | Destino (Lado C6 / Der) | Función |
| :--- | :--- | :--- | :--- |
| 🟧 **Cable Naranja** | `3V3` (Pin 3) | `ESP_3V3` (Pin 18) | **Alimentación:** Proporciona 3.3V al carril de potencia del C6 |
| 🟩 **Jumper Verde** | `GPIO 32` (Pin 19) | `C6_U0RXD` (Pin 20) | **UART TX:** Transmisión de firmware P4 ➔ C6 |
| 🟪 **Jumper Magenta** | `GPIO 28` (Pin 21) | `C6_U0TXD` (Pin 22) | **UART RX:** Recepción y sincronización P4 🠄 C6 |
| 🟦 **Cable Celeste** | `GPIO 34` (Pin 17) | `C6_IO9` (Pin 24) | **Auto-Bootloader:** Control automático del modo descarga (BOOT) |

> ℹ️ **Nota de Hardware sobre Reset:** La línea **`C6_CHIP_PU` (Pin 26 / Reset del C6)** está unida de **manera interna en la PCB al GPIO 54 del ESP32-P4**. Por lo tanto, el sistema gestiona el reset hardware del coprocesador de forma automática y **no es necesario puentear el pin 26 externamente**.

### Procedimiento Rápido

```bash
# 1. Grabar el firmware puente en el P4 (o usar la app Flasher desde CBDos)
cd tools/c6_flasher_bridge
idf.py -p /dev/ttyACM0 flash

# 2. Conectar los jumpers y cables en JP1 según el diagrama

# 3. Flashear el firmware SDIO al C6
python -m esptool --chip esp32c6 -p /dev/ttyACM0 -b 115200 \
  --connect-attempts 30 --before no_reset \
  write_flash 0x0 bsp/esp32_c6_slave/build/network_adapter.bin

# 4. Retirar los jumpers TX/RX y el cable de Boot (GPIO34 -> IO9);
#    DEJAR SIEMPRE CONECTADO el cable naranja 3V3 -> ESP_3V3 para mantener alimentado el C6.
#    Volver a flashear CBDos en el P4:
cd bsp/esp32_p4_jc4880
idf.py -p /dev/ttyACM0 flash
```

> 📖 Guía completa y mapa de registros: [`docs/hardware_pinouts_reference.md`](docs/hardware_pinouts_reference.md) y [`tools/c6_flasher_bridge/README.md`](tools/c6_flasher_bridge/README.md)

---

## 📂 Estructura del Proyecto

```
cbdos/
├── core/                    # 100% agnóstico — sin SDK de hardware
│   ├── include/cbdos/       # Cabeceras de la API unificada CBDos
│   └── src/
│       ├── cbdos_core.cpp   # Punto de entrada del núcleo
│       ├── audio/           # AudioPlayer (Helix MP3)
│       ├── lua/             # Motor Lua embebido
│       ├── network/         # NetworkHAL
│       └── ui/
│           ├── UIManager    # Gestor de vistas y ciclo de vida
│           ├── ThemeEngine  # Sistema de temas dinámicos
│           ├── WallpaperManager
│           ├── views/       # Todas las vistas de la UI
│           │   ├── SplashScreenView
│           │   ├── DashboardView
│           │   ├── ConfigView
│           │   ├── WiFiConfigView
│           │   ├── StorageConfigView
│           │   ├── WallpaperConfigView
│           │   ├── MusicPlayerView
│           │   ├── RadioView        ← en portado
│           │   └── FlasherView      ← en portado
│           ├── components/  # Widgets reutilizables
│           └── modals/      # Diálogos y ventanas modales
├── bsp/
│   ├── esp32_p4_jc4880/    # Target principal (ESP-IDF 5.5)
│   ├── esp32_s3_jc3248/    # Target secundario (PlatformIO)
│   ├── esp32_c6_slave/     # Firmware SDIO del coprocesador C6
│   └── pc_simulator/       # Simulador de escritorio (SDL2)
├── tools/
│   └── c6_flasher_bridge/  # Puente UART P4→C6 para flasheo sin hardware externo
├── docs/                   # Documentación técnica, roadmap y referencias
├── scripts/                # Utilidades y scripts Lua
└── wallpapers/             # Recursos gráficos del sistema
```

---

## 🚀 Compilar y Flashear

### Target ESP32-P4 (ESP-IDF 5.5)

```bash
. /home/kaber420/esp/esp-idf/export.sh
cd bsp/esp32_p4_jc4880
idf.py build

# Flashear y monitorear:
idf.py -p /dev/ttyACM0 flash monitor
```

### Target ESP32-S3 (PlatformIO + Arduino)

```bash
# Compilar:
pio run -d bsp/esp32_s3_jc3248

# Flashear:
pio run -d bsp/esp32_s3_jc3248 -t upload --upload-port /dev/ttyACM0

# Monitor serie:
pio device monitor -d bsp/esp32_s3_jc3248 -b 115200
```

### Coprocesador ESP32-C6 (Firmware SDIO)

```bash
. /home/kaber420/esp/esp-idf/export.sh
cd bsp/esp32_c6_slave
idf.py build
# Para flashear usa el C6 Flasher Bridge (ver sección arriba)
```

---

## 📚 Documentación Técnica

| Documento | Descripción |
|:---|:---|
| [`docs/ROADMAP.md`](docs/ROADMAP.md) | Estado de avance, changelog y fases del proyecto |
| [`docs/hardware_pinouts_reference.md`](docs/hardware_pinouts_reference.md) | Referencia completa de GPIO, buses I2C, I2S y LDOs |
| [`docs/arquitectura_agnostica_hal_core.md`](docs/arquitectura_agnostica_hal_core.md) | Diseño del desacoplamiento Core/HAL/BSP |
| [`docs/fase1_ui_core_spec.md`](docs/fase1_ui_core_spec.md) | Especificación del motor de UI — Fase 1 |
| [`docs/guia_esp32_p4_c6_hosted_wifi.md`](docs/guia_esp32_p4_c6_hosted_wifi.md) | Guía de integración Wi-Fi ESP-Hosted (P4 + C6) |
| [`docs/plan_synth_app.md`](docs/plan_synth_app.md) | Plan de la aplicación sintetizador |
| [`tools/c6_flasher_bridge/README.md`](tools/c6_flasher_bridge/README.md) | Guía completa de flasheo del coprocesador C6 |

---

## ⚠️ Reglas de Desarrollo

- **Offline-First estricto:** El sistema es 100% funcional sin red. La inicialización de Wi-Fi/BT es exclusivamente bajo demanda desde la UI.
- **LVGL v9.5 estricto:** Prohibido usar macros o sintaxis de LVGL v8 (`lv_scr_act()`, `LV_MEM_CUSTOM`, etc.). Solo APIs v9.5 (`lv_screen_active()`, `lv_button_create()`, etc.).
- **Fuente de la verdad:** `/home/kaber420/Documentos/proyectos/espOS32`. Toda lógica se porta desde allí; prohibido reinventar desde cero.
- **Verificación dual-target:** Cada cambio en `core/` debe compilar en **ambos** entornos (`idf.py build` y `pio run`).
- **Documentar hardware:** Cualquier pin, bus o registro descubierto se registra inmediatamente en `docs/hardware_pinouts_reference.md`.

---

## 📄 Licencia

Este proyecto está bajo la Licencia **GNU General Public License v3.0 (GPLv3)**.  
Copyright (C) 2026 **kaber420** (<https://github.com/kaber420/CBD-os>).

Consulta el archivo [`LICENSE`](LICENSE) para obtener los términos y condiciones completos, o visita <https://www.gnu.org/licenses/gpl-3.0.html>.

