# Plan de Arquitectura: Módulo Reutilizable `CartridgeGamepad` y Cartucho Game Boy Color (GBC)

## 🎯 Objetivo General
1. **Crear el módulo centralizado `CartridgeGamepad`:** Un componente modular, ligero y desacoplado para el manejo de controles táctiles en pantalla (overlay visual, lectura de coordenadas, detección de pulsaciones y botón de retorno OTA a `espOS32`).
2. **Implementar el Cartucho de Game Boy / Game Boy Color (GBC):** Motor de emulación de 8-bit a 60 FPS reales (*Peanut-GB*), con audio I2S DMA en Núcleo 0, carga de ROMs desde MicroSD y controles táctiles mediante el nuevo módulo `CartridgeGamepad`.
3. **Estandarizar la tabla de particiones multi-cartucho:** Dejar el sistema listo para soportar `espOS32` (OS), `DOOM`, `Game Boy Color` y futuros emuladores (como `NES`).

---

## 🧩 Parte 1: Módulo `CartridgeGamepad`

### 1.1 Estructura del Módulo
Ubicación: `firmware/include/CartridgeGamepad.h` y `firmware/src/CartridgeGamepad.cpp` (o biblioteca compartida).

```
+-------------------------------------------------------------------------+
|                         CartridgeGamepad                                |
+-------------------------------------------------------------------------+
| [ Métodos Principales ]                                                 |
|   - init(display, touch, layoutType)                                    |
|   - read() -> bitmask (BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT,           |
|                        BTN_A, BTN_B, BTN_START, BTN_SELECT)             |
|   - draw() -> Dibuja los botones visuales en los márgenes de pantalla  |
|   - handleExit() -> Conmuta partición OTA y reinicia hacia espOS32      |
+-------------------------------------------------------------------------+
```

### 1.2 Layouts / Disposiciones de Botones

#### Layout A: Mando Nintendo Clásico (Game Boy / NES)
* **Pantalla Landscape (480x320) con juego centrado (320x288):**
  * **Margen Izquierdo (X: 0 a 80):**
    * D-Pad (Cruceta de 4 direcciones: `▲`, `▼`, `◄`, `►`).
  * **Margen Derecho (X: 400 a 480):**
    * Botón **`A`** (Acción / Salto) en círculo azul/rojo.
    * Botón **`B`** (Ataque / Correr) en círculo amarillo/verde.
  * **Borde Inferior (Y: 290 a 320):**
    * Botón **`SELECT`** y Botón **`START`**.
  * **Borde Superior Derecho:**
    * Botón **`[ SALIR ]`** (Regreso seguro a `espOS32`).

#### Layout B: Modo DOOM (FPS Clásico)
* **D-Pad de movimiento:** Adelante, Atrás, Strafe L, Strafe R.
* **Giro:** Botones de rotación de cámara `L` y `R`.
* **Acciones:** `FIRE` (Disparo), `ENTER / OK`, `ABRIR / USE`.
* **Borde Superior Derecho:** Botón **`[ SALIR ]`**.

---

## 🎮 Parte 2: Cartucho de Game Boy Color (GBC)

### 2.1 Especificaciones Técnicas
* **Motor Emulador:** *Peanut-GB* (núcleo en C puro de alta fidelidad, compatible con GB original y Game Boy Color).
* **Resolución Nativa:** 160 x 144 píxeles.
* **Escalado en Pantalla AMOLED (480x320):** Escalado entero x2 exacto (**320 x 288 píxeles**) centrado en `(X=80, Y=16)`.
* **Frecuencia de Cuadros:** 60 FPS estables.
* **Audio I2S DMA:** Tarea FreeRTOS en **Núcleo 0 (Core 0)** a 32,000 Hz / 44,100 Hz con sintetizador APU de 4 canales estéreo.
* **Carga en Memoria (Zero Lag):** Las ROMs se leen desde la MicroSD (`/sd/roms/gbc/`) y se **cargan al 100% en la memoria PSRAM (8 MB)** al iniciar la partida para ejecutar a 240 MHz sin latencia de bus.

---

## 🗺️ Parte 3: Tabla de Particiones Multi-Cartucho Compacta (`custom_16MB_ota.csv`)

Los emuladores son muy compactos (~0.4 MB para GBC y ~0.5 MB para NES), por lo que asignamos particiones ajustadas de **1.0 MB**:

```csv
# Name,   Type, SubType, Offset,   Size,     Flags
nvs,      data, nvs,     ,         0x5000,
otadata,  data, ota,     ,         0x2000,
app0,     app,  ota_0,   0x010000, 0x600000, # 6.0 MB: espOS32 (Sistema Operativo)
app1,     app,  ota_1,   0x610000, 0x180000, # 1.5 MB: Cartucho DOOM (ocupa ~805 KB)
app2,     app,  ota_2,   0x790000, 0x100000, # 1.0 MB: Cartucho Game Boy Color (ocupa ~400 KB)
app3,     app,  ota_3,   0x890000, 0x100000, # 1.0 MB: Cartucho NES (ocupa ~500 KB)
spiffs,   data, spiffs,  0x990000, 0x600000, # 6.0 MB: Almacenamiento SPIFFS
fatfs,    data, fat,     0xF90000, 0x70000,  # 0.5 MB: FATFS
```

---

## 🛠️ Fases de Ejecución Propuestas

1. **Fase 1: Módulo `CartridgeGamepad`**
   - Crear `CartridgeGamepad.h` y `CartridgeGamepad.cpp`.
   - Probar el módulo conectándolo primero a DOOM para validar que los controles y la salida OTA funcionen de forma 100% idéntica y limpia.
2. **Fase 2: Motor de Game Boy Color (`[env:gbc]`)**
   - Integrar *Peanut-GB* en `firmware/lib/peanut_gb/`.
   - Crear `firmware/src/GBCLauncher.cpp` con carga de ROM desde MicroSD, render x2 en AMOLED y audio en Core 0.
   - Conectar los controles de Game Boy usando `CartridgeGamepad`.
3. **Fase 3: Integración en `espOS32`**
   - Añadir el botón **"Game Boy Color"** en el selector de aplicaciones de `espOS32` para arrancar `app2`.
