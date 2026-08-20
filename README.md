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
## 🎯 Dispositivos y Targets Oficiales
* **Guition JC4880P443C (JC-ESP32P4-M3 Rev 1.3):**
  * ESP32-P4 RISC-V @ 400 MHz (32 MB PSRAM, 16 MB Flash).
  * Pantalla IPS 4.3" 480×800 MIPI-DSI (ST7701S @ 60 FPS) + Touch GT911.
  * Audio I2S + DAC ES8311 con amplificador integrado.
  * Almacenamiento MicroSD SDMMC 4-bit (Slot 0).
  * Coprocesador inalámbrico ESP32-C6-MINI (Wi-Fi 6 / BT 5 por SDIO Slot 1).
* **Guition JC3248W535:**
  * ESP32-S3 Xtensa @ 240 MHz (8 MB PSRAM, 16 MB Flash).
  * Pantalla IPS 3.5" 320×480 QSPI (AXS15231B @ 30 FPS).
  * Audio PDM TX / Helix MP3 + MicroSD SPI.


---

## 📂 Estructura del Proyecto

* **`core/`**: Código 100% universal (interfaz de usuario, widgets, motor Lua, decodificadores de audio).
* **`core/include/cbdos/`**: Cabeceras de la API unificada de CBDos.
* **`bsp/`**: Adaptadores específicos para cada plataforma física o simulada.
* **`docs/`**: Documentación técnica, esquemas y guías de arquitectura.
* **`wallpapers/`**: Fondos de pantalla y recursos gráficos.
* **`scripts/`**: Scripts de utilidad y programas en Lua.
