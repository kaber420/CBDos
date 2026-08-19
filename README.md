# 🦾 CyBerDeck OS (CBDos v0.2.0)

**CyBerDeck OS (CBDos)** es un sistema operativo embebido modular y agnóstico diseñado para cyberdecks, consolas portátiles y dispositivos multimedia basados en microcontroladores ESP32 (ESP32-P4, ESP32-S3) y simuladores en PC.

---

## 🏛️ Arquitectura Modular (Core / HAL / BSP)

```
┌─────────────────────────────────────────────────────────────┐
│  APLICACIONES & UI (LVGL 9, Lua Engine, Helix Audio, Games) │  <- 100% Agnóstico
├─────────────────────────────────────────────────────────────┤
│  CBDos API (cbdos::system, display, input, audio, network)  │  <- API Estándar C++
├─────────────────────────────────────────────────────────────┤
│  BOARD SUPPORT PACKAGES (BSP / HAL)                         │
│  ├─ bsp/esp32_p4_jc4880/  (ESP-IDF v5.x + ST7701S + GT911)  │  <- Target P4
│  ├─ bsp/esp32_s3_jc3248/  (PlatformIO/Arduino + AXS15231)   │  <- Target S3
│  └─ bsp/pc_simulator/     (SDL2 Linux/macOS/Windows)        │  <- Target PC
└─────────────────────────────────────────────────────────────┘
```

---

## 📂 Estructura del Proyecto

* **`core/`**: Código 100% universal (interfaz de usuario, widgets, motor Lua, decodificadores de audio).
* **`core/include/cbdos/`**: Cabeceras de la API unificada de CBDos.
* **`bsp/`**: Adaptadores específicos para cada plataforma física o simulada.
* **`docs/`**: Documentación técnica, esquemas y guías de arquitectura.
* **`wallpapers/`**: Fondos de pantalla y recursos gráficos.
* **`scripts/`**: Scripts de utilidad y programas en Lua.
