# Reglas del Proyecto y Guía de Compilación (espOS32)

## 📌 Entorno de Compilación y Flasheo (ESP32-S3 Directo)
- **SIEMPRE** compilar y flashear utilizando el entorno `-e esp32`.
- **NUNCA** usar el entorno `emulator` ya que está en desuso y no cuenta con compatibilidad con `<Arduino.h>`.
- El archivo `platformio.ini` reside dentro del directorio `firmware/`.

### 🚀 ComandosEstándar:
- **Compilar Firmware:**
  ```bash
  cd firmware
  pio run -e esp32
  ```
- **Flashear a la Placa (ESP32-S3):**
  ```bash
  cd firmware
  pio run -e esp32 -t upload
  ```
- **Monitorear Puerto Serie (Serial Monitor):**
  ```bash
  cd firmware
  pio device monitor -b 115200
  ```

---

## 🛠️ Reglas del Proyecto
1. **Target de Hardware:** ESP32-S3 (AMOLED/LCD QSPI, Touch GT911/AXS15231B).
2. **UI Framework:** LVGL v9 con el tema base `DefaultTheme`.



`opencode run "<instrucción>"` para ahorrar tokens de contexto.
    *   **Selección de modelo:** Para usar modelos específicos (como deepseek o mimo), debes indicarlo siempre con el flag `-m` (ej. `-m opencode/deepseek-v4-flash-free` o `-m opencode/mimo-v2.5-free`)  ejemplo:  `opencode run -m opencode/deepseek-v4-flash-free run "<instrucción>"`
    