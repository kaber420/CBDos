# 📋 Roadmap y Especificación de Utilidades del Sistema (espOS32)

Este documento recopila la propuesta y arquitectura para las aplicaciones de utilidad rápida, productividad y el módulo de gestión de servidor / SysInfo en **espOS32**.

---

## 🏗️ 1. Arquitectura: SysInfo & Control Remoto de Servidor

### 💡 Análisis y Recomendación de Implementación
* **¿Como "ROM / App Aislada" o como "Vista Nativa LVGL"?:**
  * **Recomendación:** Implementarlo como una **Vista Nativa de LVGL** (ej. `ServerDashboardView`).
  * **Razón técnica:** A diferencia de emuladores pesados (Doom o GameBoy) que consumen el 100% de la CPU para rendering y emulación de hardware, un dashboard de servidor solo intercambia pequeños paquetes de red (JSON REST / WebSockets / MQTT / TLV) y actualiza etiquetas de texto o barras de progreso en LVGL.
  * No necesita apropiarse de todo el microcontrolador ni descargarse de memoria. Puede convivir con los servicios de fondo del sistema operativo.
* **Flujo de desarrollo (C + SDL2 ➔ ESP32-S3):**
  * Prototipar la lógica de interfaz en C / SDL2 en la PC es ideal. Al pasar al ESP32, la capa de red se enlaza con `HTTPClient` / `WiFiClient` y la capa visual se mapea directamente a widgets de LVGL v9.

### 🎯 Capacidades del Módulo SysInfo / Server Manager:
1. **Control de Contenedores Docker:**
   * Listado de contenedores activos en el servidor local.
   * Botones táctiles de acción: *Start / Stop / Restart*.
   * Indicadores de estado visuales (Verde = Running, Rojo = Stopped).
2. **Métricas de Rendimiento del Servidor y del ESP32:**
   * **Servidor:** Uso de CPU, RAM del host, temperatura, almacenamiento en disco.
   * **ESP32 Local:** Free Heap, Free PSRAM, temperatura del chip (`temperatureRead()`), espacio libre en tarjeta SD.
3. **Gestión de Red del servidorl:**
   * Conmutador táctil entre **DHCP** e **IP Estática**.
   * Modificación de configuración de red guardada en el servidor
4. **Control Multimedia del Servidor:**
   * Mandar comandos de reproducción (Play / Pause / Next / Volume) a servicios de música del servidor (Spotify daemon, MPD, etc.).

---

## 📝 2. Notas Rápidas y Listas To-Do con Checkboxes (Estilo Google Keep)

### 📌 Concepto y Caso de Uso:
Una aplicación de notas táctil para llevar listas de compras, tareas pendientes o notas rápidas. Al estar en el supermercado o taller, puedes ir marcando checkboxes con el dedo para tachar elementos guardados en la tarjeta SD.

### 🛠️ Especificación Técnica:
* **Almacenamiento:** Archivos `.json` o `.txt` estructurados en `/sdcard/notes/` (ej. `compras.json`).
* **Componentes UI (LVGL v9):**
  * Contenedor con scroll vertical (`lv_list` o contenedor flex).
  * Elementos con `lv_checkbox` interactivos. Al marcarse, el texto se atenúa o tacha.
  * Botón flotante `[+]` para agregar nuevos elementos.
  * Teclado virtual táctil en pantalla (`lv_keyboard` + `lv_textarea`) para escribir ítems.
  * Botón de papelera para limpiar elementos completados.

---

## ⏱️ 3. Cronómetro y Temporizador Pomodoro

### 📌 Concepto:
Herramienta de productividad con dos modos integrados y alertas acústicas reales mediante el driver I2S.

### 🛠️ Especificación Técnica:
* **Modo Cronómetro:**
  * Dígitos grandes (`MM:SS.ms`).
  * Botones táctiles: Iniciar, Pausar, Reset y Guardar Vuelta (*Lap*).
* **Modo Pomodoro:**
  * Ciclos configurables: 25 min de trabajo / 5 min de descanso corto / 15 min de descanso largo.
  * Indicador visual de progreso circular (`lv_arc`).
  * **Alarma Sonora:** Al concluir el tiempo, reproduce un tono corto de aviso con `NativeAudioDriver` (`/sdcard/audio/alarm.wav` o tono sintetizado).

---

## 📂 4. Gestor de Archivos SD (File Manager Táctil)

### 📌 Concepto:
Explorador nativo de directorios y archivos de la tarjeta SD sin necesidad de conectar el dispositivo a una PC.

### 🛠️ Especificación Técnica:
* **UI:** Lista interactiva con iconos diferenciados por tipo de archivo (`.mp3`, `.gbc`, `.wad`, `.png`, `.txt`).
* **Acciones directas:**
  * Tocar `.mp3` ➔ Abre `MusicView`.
  * Tocar `.gbc` ➔ Lanza `GBCLauncher`.
  * Tocar `.wad` ➔ Lanza `DoomLauncher`.
  * Tocar `.txt` ➔ Abre el lector de notas.
  * Mantener pulsado ➔ Menú contextual con opción **Borrar archivo** e información de tamaño.

---

## 🌐 5. Web File Uploader por WiFi (OTA SD Manager)

### 📌 Concepto:
Servidor web embebido para cargar y descargar archivos a la tarjeta SD directamente desde el navegador de un móvil o PC conectado a la misma red local.

### 🛠️ Especificación Técnica:
* Al activar la opción en el menú de red, se inicia un `WebServer` en el puerto 80.
* En pantalla se muestra la URL (ej. `http://192.168.1.120`) o un código QR para escanear con el móvil.
* Página web HTML/JS ligera que permite arrastrar archivos ROMs, música y fondos de pantalla directamente a la SD.

---

## 🖩 6. Calculadora Táctil

### 📌 Concepto:
Calculadora de operaciones básicas (`+ - * / % .`) con interfaz optimizada para dedos en la pantalla táctil de resolución alta.
* **Componentes UI:** Grid de botones estilizados con el tema visual de espOS32 y feedback sonoro / háptico al pulsar.

---

## 📶 7. Escáner de Redes WiFi (WiFi Analyzer)

### 📌 Concepto:
Herramienta de diagnóstico para listar todas las redes WiFi 2.4 GHz al alcance.
* **Qué muestra:** SSID, canal, nivel de señal en dBm / barras de potencia (RSSI), y tipo de cifrado (WPA2/WPA3/Abierta).
* **Acción:** Al pulsar sobre una red, abre el diálogo con teclado táctil para introducir contraseña y conectarse.

---

## 📊 Matriz de Prioridad y Dificultad

| Aplicación / Utilidad | Complejidad | Dependencias Clave | Impacto de Uso |
| :--- | :---: | :--- | :---: |
| **Notas & To-Do Checkboxes** | Baja | SD (`LVFS`), `lv_checkbox`, `lv_keyboard` | ⭐⭐⭐⭐⭐ (Diario) |
| **Cronómetro & Pomodoro** | Baja | `NativeAudioDriver`, `lv_arc`, FreeRTOS Timer | ⭐⭐⭐⭐ (Productividad) |
| **Calculadora Táctil** | Muy Baja | `lv_grid` / `lv_btnmatrix` | ⭐⭐⭐ (Básico) |
| **File Manager SD** | Media | `LVFS_Driver`, Navegación de vistas | ⭐⭐⭐⭐⭐ (Gestión) |
| **WiFi Web File Uploader** | Media | `WebServer`, WiFi, SD write | ⭐⭐⭐⭐⭐ (Comodidad) |
| **Server / Docker Dashboard** | Media/Alta | WiFi / HTTPClient / REST API, LVGL widgets | ⭐⭐⭐⭐⭐ (Avanzado) |
| **WiFi Analyzer** | Baja | `WiFi.scanNetworks()`, `lv_list` | ⭐⭐⭐⭐ (Diagnóstico) |
