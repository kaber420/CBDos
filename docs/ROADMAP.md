# 🗺️ CBDos v0.2.0 - Roadmap & Bitácora de Desarrollo

Este documento centraliza el estado actual de avance, la arquitectura, el registro de cambios (Changelog) y las metas futuras de **CBDos** (CyBerDeck OS), diseñado para operar offline y con soporte multi-target desacoplado.

---

## 🏗️ 1. Arquitectura del Sistema

El sistema utiliza una arquitectura **Dual-Target desacoplada**:
* **`core/`**: C++ Agnóstico y UI basada en **LVGL v9.5**. No contiene dependencias directas a SDKs de hardware (`#include <driver/...>` o `#include <Arduino.h>`). Utiliza interfaces HAL (`AudioHAL`, `StorageHAL`, `NetworkHAL`, `SystemHAL`).
* **`bsp/`**: Board Support Package específico para cada microcontrolador y placa.
  * **Target ESP32-P4:** `bsp/esp32_p4_jc4880` (JC4880P443C, 480×800 IPS MIPI-DSI @ 60 FPS, ESP-IDF 5.5).
  * **Target ESP32-S3:** `bsp/esp32_s3_jc3248` (JC3248W535, 320×480 IPS QSPI @ 30 FPS, PlatformIO + Arduino Core).

---

## 📦 2. Registro de Módulos y Estado de Implementación

| Módulo / Componente | Descripción | Estado | Target S3 | Target P4 |
| :--- | :--- | :---: | :---: | :---: |
| **Core UI Engine** | Gestor de ciclo de vida de vistas (`BaseView`, `UIManager`) en LVGL 9.5 | ✅ 100% | ✅ | ✅ |
| **Theme Engine** | Paletas de colores dinámicas, Cyberpunk, Dark, Light y acentos | ✅ 100% | ✅ | ✅ |
| **Wallpaper Engine** | Gestor de fondos de pantalla dinámicos en PSRAM | ✅ 100% | ✅ | ✅ |
| **Splash Screen View** | Animación de booteo y diagnóstico inicial de hardware | ✅ 100% | ✅ | ✅ |
| **Dashboard View** | Vista principal tipo Cyberdeck con accesos directos y widgets | ✅ 100% | ✅ | ✅ |
| **Config View** | Menú maestro de ajustes del sistema | ✅ 100% | ✅ | ✅ |
| **WiFi Config View** | Escaneo, conexión y gestión de credenciales WiFi | ✅ 90% | ✅ | 🟡 (vía C6) |
| **Storage Config View**| Diagnóstico de particiones, MicroSD y LittleFS/SPIFFS | ✅ 95% | ✅ | 🟡 (SDMMC) |
| **Audio Core (Helix)** | Decodificador MP3 Helix en PSRAM + Buffer I2S | ✅ 95% | ✅ | 🟡 (Driver ES8311) |
| **Music Player View** | UI de reproductor de audio, lista de pistas y controles | ✅ 95% | ✅ | ✅ |
| **Synth Sound Engine** | Motor de síntesis y generador de ondas básico | 🔄 En Plan | ⏳ | ⏳ |
| **File Manager View** | Explorador de archivos universal para MicroSD/Flash | ⏳ Pendiente | ⏳ | ⏳ |
| **System Info Monitor** | Monitor de recursos en tiempo real (RAM, Heap, FPS, Temp) | ⏳ Pendiente | ⏳ | ⏳ |
| **Lua Script Engine** | Intérprete Lua embebido para scripts y micro-apps | ⏳ Pendiente | ⏳ | ⏳ |

*Leyenda: ✅ Operativo / 🟡 En integración de driver hardware / 🔄 En diseño / ⏳ Pendiente*

---

## 📅 3. Roadmap por Fases

### 🟢 Fase 1: Arquitectura Base, Shell y UI Core (Completada)
- [x] Desacoplamiento de `core/` y `bsp/`.
- [x] Migración total de vistas y componentes a **LVGL v9.5**.
- [x] Implementación de `UIManager`, `BaseView`, `ThemeEngine` y `WallpaperManager`.
- [x] Pantallas base: `SplashScreenView`, `DashboardView`, `ConfigView`, `WiFiConfigView`, `StorageConfigView`.

### 🟡 Fase 2: Subsistema Multimedia y Audio (Fase Actual)
- [x] Motor agnóstico `AudioPlayer` con decodificación Helix MP3.
- [x] Vista completa de reproductor de música `MusicPlayerView` (listas, scrubber, volumen, carátulas).
- [ ] Validación e integración del driver I2S + Códec **ES8311** en el BSP del ESP32-P4.
- [ ] Verificación de lectura de MP3 vía SDMMC en ESP32-P4 y SPI en ESP32-S3.
- [ ] Implementación de la aplicación de sintetizador de audio (*Synth App*).

### 🔵 Fase 3: Ecosistema de Aplicaciones y Utilidades
- [ ] **File Explorer:** Navegador de directorios con operaciones básicas (ver, abrir, borrar).
- [ ] **System Monitor:** Widget y vista dedicada con gráficos de consumo de RAM/PSRAM, FPS y temperatura.
- [ ] **Lua Runtime:** Consola interactiva y ejecutor de micro-scripts desde tarjeta SD.
- [ ] **Text Editor / Reader:** Visor de archivos de texto y logs.

### 🟣 Fase 4: Optimización de Hardware y Conectividad
- [ ] Co-procesador ESP32-C6 en el target P4 (comunicación SDIO/SPI para WiFi 6 y BLE).
- [ ] Aceleración 2D PPA (Pixel Processing Accelerator) en ESP32-P4 para renderizado LVGL a 60 FPS.
- [ ] Soporte de teclado físico (I2C CardKB / USB HID).
- [ ] Gestión de energía y modo suspensión / Deep Sleep.

---

## 🛠️ 4. Guía de Comandos Rápidos de Compilación (Cheat Sheet)

### Target ESP32-P4 (ESP-IDF 5.5)
```bash
. /home/kaber420/esp/esp-idf/export.sh
cd bsp/esp32_p4_jc4880
idf.py build
# Flashear y monitorear:
idf.py -p /dev/ttyACM0 flash monitor
```

### Target ESP32-S3 (PlatformIO)
```bash
cd /home/kaber420/Documentos/proyectos/cbdos
pio run -d bsp/esp32_s3_jc3248
# Flashear:
pio run -d bsp/esp32_s3_jc3248 -t upload --upload-port /dev/ttyACM0
# Monitor serie:
pio device monitor -d bsp/esp32_s3_jc3248 -b 115200
```

---

## 📚 5. Enlaces a Documentación de Referencia Local

* 📌 [Hardware Pinouts & Registers Reference](file:///home/kaber420/Documentos/proyectos/cbdos/docs/hardware_pinouts_reference.md)
* 📐 [Arquitectura Agnóstica HAL / Core](file:///home/kaber420/Documentos/proyectos/cbdos/docs/arquitectura_agnostica_hal_core.md)
* 🎨 [Especificación de UI Core Fase 1](file:///home/kaber420/Documentos/proyectos/cbdos/docs/fase1_ui_core_spec.md)
* 🎹 [Plan Synth App](file:///home/kaber420/Documentos/proyectos/cbdos/docs/plan_synth_app.md)
