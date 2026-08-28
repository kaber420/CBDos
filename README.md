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
| **Flasher View** | Programador de campo autónomo (Flasheo por USB-C y UART a ESP32-C3, S3, C6) | ✅ | ➖ | ✅ |
| **Synth Sound Engine** | Motor de síntesis y generador de ondas | 📋 Planificado | ⏳ | ⏳ |
| **File Manager View** | Explorador de archivos universal MicroSD/Flash | ⏳ | ⏳ | ⏳ |
| **System Info Monitor** | Monitor de RAM, Heap, FPS y temperatura en tiempo real | ⏳ | ⏳ | ⏳ |
| **Lua Script Engine** | Intérprete Lua embebido para micro-apps y Direct 2D GFX | ✅ 90% | ⏳ | ✅ |

*Leyenda: ✅ Operativo · 🟡 Integración de driver en curso · 🔄 Portando desde espOS32 · 📋 Planificado · ⏳ Pendiente · ➖ No aplica*

---

## 🔌 Programador de Campo Autónomo (Universal Flasher en CBDos)

El **ESP32-P4** con CBDos opera como un **Programador de Campo Totalmente Autónomo y Desatendido (Standalone Field Programmer)**, permitiendo programar otros microcontroladores directamente desde la pantalla táctil sin necesidad de una laptop:

### 1. ⚡ Flasheo Directo por Puerto USB 2.0 High-Speed (Cable USB-C a USB-C)
- **Controlador USB High-Speed:** Utiliza el puerto USB OTG High-Speed dedicado del ESP32-P4 con soporte de entrega de energía (5V VBUS hacia el target).
- **Auto-Bootloader por Hardware (Zero Botones):** El P4 conmuta las señales de control virtual DTR/RTS por hardware sobre CDC-ACM, forzando la entrada al ROM Download Mode del chip destino de forma 100% desatendida.
- **Dispositivos Target Compatibles:** ESP32-C3, ESP32-S3, ESP32-C6, ESP32-P4 y cualquier chip con interfaz USB-Serial/JTAG nativa.
- **Flujo Autónomo:** Selecciona el archivo binario desde la MicroSD (`/sdcard/cartridges/*.bin`), detecta el chip (`Target Chip ID`), borra sectores, escribe bloques Flash a alta velocidad y verifica la integridad por MD5.

### 2. 🪛 Flasheo por UART / Cabecera JP1 (Coprocesador Integrado ESP32-C6)
- **Flasheo Simplificado con solo 3 Cables:** El coprocesador inalámbrico ESP32-C6 ya recibe su alimentación de forma interna en la PCB. Para programarlo mediante la app integrada **Flasher** o el puente UART, solo se requieren **3 conexiones temporales** en la cabecera **JP1 (2×13 pines)**:

![Diagrama de Conexiones de Flasheo ESP32-C6 en JP1](docs/images/esp32_c6_flasher_diagram.png)

#### 🔌 Conexiones Requeridas en la Cabecera JP1 para ESP32-C6 (3 Cables Únicamente)

| Cable / Jumper | Origen (Lado P4 / Izq) | Destino (Lado C6 / Der) | Función |
| :--- | :--- | :--- | :--- |
| 🟩 **Jumper Verde** | `GPIO 32` (Pin 19) | `C6_U0RXD` (Pin 20) | **UART TX:** Transmisión de firmware P4 ➔ C6 |
| 🟪 **Jumper Magenta** | `GPIO 28` (Pin 21) | `C6_U0TXD` (Pin 22) | **UART RX:** Recepción y sincronización P4 🠄 C6 |
| 🟦 **Cable Celeste** | `GPIO 34` (Pin 17) | `C6_IO9` (Pin 24) | **Auto-Bootloader:** Control automático del modo descarga (BOOT) |

> ℹ️ **Alimentación y Reset 100% Internos en la PCB:** 
> - **Alimentación (`ESP_3V3`):** El módulo C6 está alimentado internamente por el regulador de la placa, por lo que **no se requiere ningún cable externo de 3.3V**.
> - **Reset (`C6_CHIP_PU`):** La línea de Reset del C6 está conectada internamente al **GPIO 54 del ESP32-P4**, permitiendo que CBDos reinicie el coprocesador automáticamente sin puentear pines de reset.

> 📖 Hito técnico validado en hardware: [`docs/hardware/usb_c_field_flasher_milestone.md`](docs/hardware/usb_c_field_flasher_milestone.md) y [`docs/hardware/pinouts_and_ports.md`](docs/hardware/pinouts_and_ports.md)

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

## 📚 Documentación Técnica y Arquitectura

| Documento | Descripción |
|:---|:---|
| [`docs/README.md`](docs/README.md) | **Portal y Mapa Maestro de Navegación de Documentación** |
| [`docs/ROADMAP.md`](docs/ROADMAP.md) | Estado de avance, changelog y fases del proyecto |
| [`docs/hardware/pinouts_and_ports.md`](docs/hardware/pinouts_and_ports.md) | Referencia completa de GPIO, buses I2C, I2S y pines JP1 |
| [`docs/hardware/usb_c_field_flasher_milestone.md`](docs/hardware/usb_c_field_flasher_milestone.md) | Hito de flasheo autónomo USB-C en campo con Auto-Bootloader |
| [`docs/architecture/hal_and_core_architecture.md`](docs/architecture/hal_and_core_architecture.md) | Arquitectura agnóstica de `core/`, Ley de Pureza y contratos HAL |
| [`docs/architecture/multi_radio_hub_router_design.md`](docs/architecture/multi_radio_hub_router_design.md) | Estación Base y Router Multi-Antena con Hub USB y C3s |
| [`docs/architecture/modular_lua_bridge_architecture.md`](docs/architecture/modular_lua_bridge_architecture.md) | Arquitectura modular de bindings Lua por dominios |
| [`docs/network/plan_espnow_usb_bridge.md`](docs/network/plan_espnow_usb_bridge.md) | Firmware del módem USB ESP-NOW y protocolo de enmarcado |
| [`docs/api/core_apis_reference.md`](docs/api/core_apis_reference.md) | Referencia completa de APIs públicas del SDK |
| [`docs/api/how_to_create_an_app.md`](docs/api/how_to_create_an_app.md) | Guía de creación de aplicaciones nativas en C++ y LVGL 9.5 |
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

