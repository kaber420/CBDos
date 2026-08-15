# Reglas del Proyecto y Guía de Compilación (CBDos)

## 📌 Entornos de Compilación y Flasheo (ESP32-S3 Directo)
El proyecto usa un esquema multi-firmware OTA con entornos separados en `platformio.ini`:

- **Sistema Base (CBDos):** `pio run -e esp32 -t upload`
- **Cartucho DOOM:** `pio run -e doom -t upload` (Flashea en la partición 0x610000)
- **Cartucho Game Boy (GBC):** `pio run -e gbc -t upload` (Flashea en la partición 0x790000)

- **NUNCA** usar el entorno `emulator` ya que está en desuso y no cuenta con compatibilidad con `<Arduino.h>`.
- El archivo `platformio.ini` reside dentro del directorio `firmware/`.

### 🚀 Comandos Estándar (Ejemplo para CBDos):
- **Compilar Firmware:**
  ```bash
  cd firmware
  pio run -e esp32
  ```
- **Monitorear Puerto Serie (Serial Monitor Básico):**
  ```bash
  cd firmware
  pio device monitor -b 115200
  ```
- **Monitorear y Decodificar Crashes/Excepciones (Backtrace Decoder):**
  ```bash
  cd firmware
  pio device monitor -b 115200 --filter esp32_exception_decoder
  ```

---

## 🔍 Guía de Diagnóstico de Reinicios / Crashes (Backtrace por Serie)
Cuando ocurra un reinicio no deseado (Kernel Panic / Guru Meditation Error):
1. **Ejecutar el monitor con el filtro decodificador:** `pio device monitor -b 115200 --filter esp32_exception_decoder`.
2. **Reproducir la falla en la pantalla táctil / dispositivo.**
3. **Analizar la traza de pila (*Backtrace*):** El filtro traducirá las direcciones hexadecimales (ej. `0x42001fd7`) a la línea exacta de código fuente C/C++ en el proyecto (ej. `NativeAudioDriver.cpp:291`).

---

## 🛠️ Reglas del Proyecto y Política de Comunicación Estricta

1. **Target de Hardware:** ESP32-S3 (AMOLED/LCD QSPI, Touch GT911/AXS15231B).
2. **UI Framework:** LVGL v9 con el tema base `DefaultTheme`.
3. **Control de Ejecución Estricto (Zero Presumption & Zero Acciones Silenciosas):**
   - **El usuario dirige y autoriza; la IA propone y ejecuta únicamente con aprobación.**
   - **PROHIBIDO** editar código fuente, crear archivos, borrar ficheros o flashear sin la previa propuesta, explicación y **autorización explícita** del usuario.
   - **PROHIBIDO** ejecutar comandos ocultos, descargas o bucles de herramientas en silencio. El asistente NUNCA debe hacer que el usuario tenga que adivinar qué está haciendo; siempre debe explicar brevemente qué se va a hacer antes de tocar nada.
   - Ante cualquier falla o diagnóstico, la IA debe presentar el diagnóstico al usuario, explicar la causa y la solución propuesta, y **esperar a que el usuario dé la orden de aplicar los cambios**.

4. **Uso de OpenCode (Modelos externos):**
   - Usar `opencode run "<instrucción>"` para ahorrar tokens de contexto.
   - **Selección de modelo:** Para usar modelos específicos (como deepseek o mimo), indicarlo con `-m` (ej. `opencode run -m opencode/deepseek-v4-flash-free "<instrucción>"`).

    