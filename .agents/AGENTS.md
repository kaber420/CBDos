# Reglas del Proyecto y Guía de Desarrollo (CBDos v0.2.1)

## 📂 1. Directorio del Código Fuente Original (Fuente de la Verdad Absoluta)
- **Ruta del Proyecto Original:** `/home/kaber420/Documentos/proyectos/espOS32`
- **REGLA DE ORO DE PORTADO:** Todas las aplicaciones, vistas, componentes de UI, algoritmos de decodificación y comportamientos deben tomar como **única fuente de la verdad** el código original ubicado en `espOS32`.
- **PROHIBIDO REINVENTAR LA RUEDA:** Queda estrictamente prohibido improvisar código desde cero, proponer librerías alternativas no probadas o ignorar la lógica ya afinada en `espOS32` (ej: decodificación con Helix, tamaños de búfer en PSRAM, salto de metadatos ID3v2, layouts de pantalla, etc.).
- **Flujo Obligatorio:**
  1. Inspeccionar primero el archivo fuente correspondiente en `/home/kaber420/Documentos/proyectos/espOS32`.
  2. Extraer la lógica y adaptarla a la arquitectura agnóstica de `cbdos/core/` (C++ y LVGL 9.5).
  3. Verificar que no se degrade ninguna función ni rendimiento.

---

## 📌 2. Arquitectura Multi-Target y Entornos de Compilación

El proyecto opera bajo un modelo desacoplado:
* **`core/`**: Núcleo agnóstico en C++ (UI LVGL 9.5, lógica de apps, decodificadores Helix, APIs de sistema).
* **`bsp/`**: Capa de soporte de hardware (Board Support Package) por microcontrolador.

### Target ESP32-P4: Guition JC4880P443C (Módulo JC-ESP32P4-M3 Rev 1.3)
- **SoC Principal:** ESP32-P4 RISC-V Dual-Core @ 400 MHz (Chip Rev 1.3, 16 MB Flash, 32 MB Hexal-PSRAM).
- **Coprocesador:** ESP32-C6-MINI (WiFi 6 / Bluetooth 5 vía SDIO Slot 1).
- **Pantalla:** 4.3" IPS 480x800 MIPI-DPI (ST7701S) @ 60 FPS.
- **Touch:** Goodix GT911 (I2C SDA=7 SCL=8 RST=3 INT=4).
- **Audio:** Everest ES8311 (I2C SDA=7 SCL=8 + I2S MCLK=13 BCLK=12 WS=10 DOUT=9 PA=11).
- **Almacenamiento:** MicroSD Slot 0 SDMMC 4-bit (GPIO 39-44) + LDO VO4 (3.3V).
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

### Target ESP32-S3 (JC3248W535 - 320x480 QSPI @ 30 FPS)
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

## ⚠️ 3. Reglas Obligatorias de Desarrollo

1. **Persistencia Obligatoria y Modularidad de Documentación (`docs/`):**
   - **Hardware y Pines:** Toda información técnica (GPIOs, buses, LDOs, puertos UART) DEBE documentarse y actualizarse en `/home/kaber420/Documentos/proyectos/cbdos/docs/hardware/pinouts_and_ports.md`.
   - **Arquitectura de Software y HAL:** Toda interfaz abstracta, módulo HAL o patrón arquitectónico DEBE documentarse en `/home/kaber420/Documentos/proyectos/cbdos/docs/architecture/hal_and_core_architecture.md`.
   - **APIs para Desarrolladores y SDK:** Todo nuevo servicio o API accesible para aplicaciones DEBE documentarse con ejemplos en `/home/kaber420/Documentos/proyectos/cbdos/docs/api/core_apis_reference.md` y guías en `/home/kaber420/Documentos/proyectos/cbdos/docs/api/how_to_create_an_app.md`.

2. **Verificación Multi-Target Obligatoria:**
   - Cada cambio en `core/` debe compilar limpiamente en **AMBOS** entornos:
     - `idf.py build` (ESP32-P4)
     - `pio run -d bsp/esp32_s3_jc3248` (ESP32-S3)

3. **UI Framework (LVGL v9.5 Estricto):** 
   - El proyecto utiliza **LVGL v9.5** con el tema base `DefaultTheme` y gestión de memoria en PSRAM.
   - **ESTRICTAMENTE PROHIBIDO** usar macros o sintaxis obsoletas de **LVGL v8** (ej. `LV_MEM_CUSTOM`, `lv_scr_act()`, etc.). Usar exclusivamente las APIs de LVGL 9.5 (`lv_screen_active()`, `lv_button_create()`, `lv_image_create()`, etc.).

4. **Integración con HeaderBar y Navegación:**
   - Toda vista derivada de `BaseView` debe usar el contenedor base `m_container` (sin redeclararlo en la clase derivada para evitar *variable shadowing*).
   - Los botones de navegación de cabecera deben usar `HeaderBar::setRightAction()`.
   - Si la aplicación no usa internet, debe llamar a `HeaderBar::showWifi(false)`.

5. **Control de Ejecución Estricto (Zero Presumption & Zero Acciones Silenciosas):**
   - **El usuario dirige y autoriza; la IA propone y ejecuta únicamente con aprobación previa.**
   - **PROHIBIDO** editar código fuente, crear archivos, borrar ficheros o flashear sin la previa propuesta, explicación y **autorización explícita** del usuario.
   - **PROHIBIDO** ejecutar comandos ocultos, descargas o bucles de herramientas en silencio. Siempre se debe explicar brevemente qué se va a hacer antes de tocar nada.
   - Ante cualquier falla o diagnóstico, la IA debe presentar el diagnóstico al usuario, explicar la causa y la solución propuesta, y **esperar a que el usuario dé la orden de aplicar los cambios**.

6. **Uso de OpenCode (Modelos externos):**
   - Usar `opencode run "<instrucción>"` para ahorrar tokens de contexto.
   - **Selección de modelo:** Para usar modelos específicos (como deepseek o mimo), indicarlo con `-m` (ej. `opencode run -m opencode/deepseek-v4-flash-free "<instrucción>"`).

7. **Desacoplamiento Total del Arranque (Offline-First Estricto):**
   - **ESTRICTAMENTE PROHIBIDO** inicializar la red (Wi-Fi, Bluetooth, ESP-Hosted, DHCP o tareas de fondo de red) de forma síncrona o automática en `app_main()` o durante el encendido del sistema.
   - El sistema operativo CBDos **DEBE ser 100% funcional y autónomo** (pantalla, táctil, audio, almacenamiento MicroSD, interfaz gráfica LVGL) **con o sin red conectada**, sin depender de la presencia, alimentación o respuesta de ningún coprocesador inalámbrico (ESP32-C6).
   - Toda inicialización de red debe ser **exclusivamente bajo demanda** cuando el usuario lo solicite explícitamente desde la UI o API.

8. **Ley de Pureza Arquitectónica de `core/` (Zero Platform Pollution):**
   - **Agnosticismo Estricto de `core/`:** `core/` DEBE ser código C++ estándar y LVGL 9.5 puro.
   - **PROHIBIDO** incluir headers de plataformas o frameworks dentro de `core/` (ej. `<Arduino.h>`, `<Preferences.h>`, `<SD.h>`, `<WiFi.h>`, `<esp_wifi.h>`, `<driver/...>`).
   - **PROHIBIDO** bifurcar la lógica de negocio mediante `#ifdef ARDUINO` o `#ifdef ESP_PLATFORM` dentro de `core/`.
   - **Patrón de Abstracción HAL / Interfaces:** Todo acceso a hardware, NVS, sistema de archivos o red DEBE realizarse mediante interfaces abstractas (`IStorageBackend`, `IAudioSink`, `INetworkAdapter`). Las implementaciones concretas residen ÚNICAMENTE en sus respectivos directorios `bsp/` y se inyectan en tiempo de inicialización.

9. **Integridad Absoluta de Documentación e Historial (`docs/`):**
   - **ESTRICTAMENTE PROHIBIDO** sobreescribir, borrar, truncar o reciclar archivos de documentación existentes en `docs/` o `docs/drafts/` a menos que el usuario lo ordene explícitamente indicando el archivo.
   - Toda nueva investigación, borrador o análisis DEBE crearse en un archivo nuevo con nombre único y descriptivo.

10. **Prohibición Universal de Resúmenes y Ediciones Destructivas (Aplica a TODO el Proyecto):**
    - **ESTRICTAMENTE PROHIBIDO** condensar, simplificar, resumir, truncar o sobreescribir de forma destructiva **CUALQUIER archivo del repositorio** (código fuente `.cpp`/`.c`, cabeceras `.hpp`/`.h`, documentación `.md`, planes, especificaciones, scripts `.lua`/`.py`, Makefiles o configuraciones).
    - Toda modificación en cualquier archivo del proyecto debe ser **estrictamente quirúrgica**, preservando intacto todo el código preexistente, comentarios, contratos de funciones, diagramas ASCII, tablas y explicaciones técnicas previamente desarrolladas.