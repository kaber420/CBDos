# Reglas del Proyecto y Guía de Compilación (CBDos v0.2.0)

## 📂 Directorio del Código Fuente Original (Referencia Obligatoria)
- **Ruta del Proyecto Original:** `/home/kaber420/Documentos/proyectos/espOS32`
- Todas las aplicaciones, vistas, componentes de UI y comportamientos deben tomar como fuente de la verdad el código original ubicado en esa carpeta antes de portar a `cbdos/core/`.

---

## 📌 Arquitectura Multi-Target y Entornos de Compilación

El proyecto opera bajo un modelo desacoplado:
* **`core/`**: Núcleo agnóstico en C++ (UI LVGL 9.5, lógica de apps, APIs de sistema).
* **`bsp/`**: Capa de soporte de hardware (Board Support Package) por microcontrolador.

### 1. Target ESP32-P4 (JC4880P443C - 480x800 MIPI-DPI @ 60 FPS)
- **Framework:** ESP-IDF 5.5 nativo (CMake / Ninja).
- **Ruta del BSP:** `bsp/esp32_p4_jc4880`
- **Comandos estándar:**
  ```bash
  # Cargar entorno ESP-IDF y compilar
  . /home/kaber420/esp/esp-idf/export.sh
  cd bsp/esp32_p4_jc4880
  idf.py build
  
  # Flashear y monitorear
  idf.py -p /dev/ttyACM0 flash monitor
  ```

### 2. Target ESP32-S3 (JC3248W535 - 320x480 QSPI @ 30 FPS)
- **Framework:** PlatformIO + Arduino Core (pioarduino).
- **Ruta del BSP:** `bsp/esp32_s3_jc3248`
- **Comandos estándar:**
  ```bash
  # Compilar
  pio run -d bsp/esp32_s3_jc3248
  
  # Flashear por puerto serie
  pio run -d bsp/esp32_s3_jc3248 -t upload --upload-port /dev/ttyACM0
  
  # Monitorear puerto serie
  pio device monitor -d bsp/esp32_s3_jc3248 -b 115200
  ```

---

## 🔍 Guía de Diagnóstico de Reinicios / Crashes (Backtrace por Serie)
Cuando ocurra un reinicio no deseado (Kernel Panic / Guru Meditation Error):
1. **En ESP32-P4:** `idf.py monitor` decodifica automáticamente la traza con el archivo ELF.
2. **En ESP32-S3:** `pio device monitor -d bsp/esp32_s3_jc3248 -b 115200 --filter esp32_exception_decoder`.
3. **Analizar la traza de pila (*Backtrace*):** El filtro traducirá las direcciones hexadecimales a la línea exacta de código fuente C/C++.

---

## ⚠️ Reglas Obligatorias de Desarrollo

1. **Target de Hardware:**
   - **ESP32-P4:** 480x800, MIPI-DSI ST7701, Touch GT911 (I2C), Acelerador 2D PPA.
   - **ESP32-S3:** 320x480, QSPI AXS15231B, Touch AXS15231B (I2C), Renderizado Software.

2. **UI Framework (LVGL v9.5 Estricto):** 
   - El proyecto utiliza **LVGL v9.5** con el tema base `DefaultTheme` y gestión de memoria en PSRAM.
   - **ESTRICTAMENTE PROHIBIDO** usar macros, funciones o sintaxis obsoletas de **LVGL v8** (ej. `LV_MEM_CUSTOM`, `lv_scr_act()`, etc.). Todo componente debe usar las APIs oficiales de LVGL 9.5 (`lv_screen_active()`, `lv_button_create()`, `lv_image_create()`, `lv_tick_set_cb`, `lv_indev_set_display`, etc.).

3. **Control de Ejecución Estricto (Zero Presumption & Zero Acciones Silenciosas):**
   - **El usuario dirige y autoriza; la IA propone y ejecuta únicamente con aprobación previa.**
   - **PROHIBIDO** editar código fuente, crear archivos, borrar ficheros o flashear sin la previa propuesta, explicación y **autorización explícita** del usuario.
   - **PROHIBIDO** ejecutar comandos ocultos, descargas o bucles de herramientas en silencio. Siempre se debe explicar brevemente qué se va a hacer antes de tocar nada.
   - Ante cualquier falla o diagnóstico, la IA debe presentar el diagnóstico al usuario, explicar la causa y la solución propuesta, y **esperar a que el usuario dé la orden de aplicar los cambios**.

4. **Uso de OpenCode (Modelos externos):**
   - Usar `opencode run "<instrucción>"` para ahorrar tokens de contexto.
   - **Selección de modelo:** Para usar modelos específicos (como deepseek o mimo), indicarlo con `-m` (ej. `opencode run -m opencode/deepseek-v4-flash-free "<instrucción>"`).