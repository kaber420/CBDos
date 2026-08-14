# Plan de Integración: Motor de Juegos, Multitouch GT911 y VirtualGamepad (espOS32)

## 1. Visión General y Propósito

Este documento establece el plan operativo y la especificación de diseño para:
1. **Activar el soporte Multitouch nativo** del controlador capacitivo **GT911** (JC3248W535) en espOS32.
2. **Implementar el componente `VirtualGamepad`**, capaz de procesar múltiples toques simultáneos en dos modos (Zonas Invisibles y Overlay D-Pad/Botones LVGL).
3. **Seleccionar e integrar los motores id Tech 1 (`doomgeneric` para DOOM y `hereticgeneric` para Heretic)** como Apps Lazy (`GameView`), permitiendo ejecutar múltiples juegos (Doom, Doom II, Heretic, Hexen, Chex Quest) y sentando las bases para emuladores (NES, Game Boy) y Wolfenstein 3D.

---

## 2. Investigación de Motores: DOOM vs. Heretic / Hexen

### 2.1 Mapeo Real de Motores y Compatibilidad WAD

Existe el mito de que Heretic y Hexen corren en el mismo ejecutable de DOOM. **Técnicamente derivan del motor id Tech 1, pero Raven Software modificó el código C profundamente** (inventario, vuelo/pitch, scripting ACS, efectos de ambiente). Un motor de DOOM puro no puede leer WADs de Heretic.

Por ello, la arquitectura de espOS32 adopta dos motores C ligeros bajo la misma interfaz:

| Juego | Motor C Seleccionado | Formato WAD | Integración en espOS32 |
| :--- | :--- | :--- | :--- |
| **DOOM 1, DOOM II, Chex Quest** | **`doomgeneric`** (PrBoom core) | `doom1.wad`, `doom2.wad`, `chex.wad` | `DoomEngine` (`IGameEngine`) |
| **Heretic, Hexen** | **`hereticgeneric`** (Chocolate Heretic core) | `heretic1.wad`, `hexen.wad` | `HereticEngine` (`IGameEngine`) |

### 2.2 Por qué `doomgeneric` es el motor ideal para DOOM
`doomgeneric` desacopla 100% el SO y el hardware reduciendo la integración a **solo 5 funciones callback C**:
1. `DG_Init()`: Inicialización.
2. `DG_DrawFrame()`: Copia de píxeles al canvas RGB565 en PSRAM (320×200).
3. `DG_SleepMs(ms)`: Tarea RTOS / Delay.
4. `DG_GetTicksMs()`: Ticks del sistema (`millis()`).
5. `DG_GetKey(pressed, keycode)`: Entrada procesada desde `VirtualGamepad`.

---

## 3. Habilitación del Soporte Multitouch (Driver GT911)

### 3.1 Diagnóstico del Estado Actual
- **Hardware**: Chip Goodix GT911 en pantalla de 320x480 (JC3248W535). Capacidad hardware: **5 puntos táctiles simultáneos**.
- **Librería base**: `tamctec/TAMC_GT911` en `platformio.ini`.
- **Limitación actual**: `firmware/src/main.cpp:43` (`my_touchpad_read`) solo invoca `touchDriver.read(tp)` para 1 punto.

### 3.2 Plan de Refactorización del Driver Táctil
1. **Extensión de `JC3248W535_Touch`**:
   - Añadir método `getTouches(TouchPoint points[2])` para consultar Puntos 0 y 1.
2. **Desacoplamiento para LVGL y para Juegos**:
   - **Para LVGL (UI General)**: `my_touchpad_read` sigue enviando el Puntero 0 a LVGL para gestos estándar.
   - **Para `VirtualGamepad` / `GameView`**: Se consulta directamente el estado multi-punto (Puntos 0 y 1) durante la tarea del juego en el Core 1, permitiendo combos simultáneos (ej: Avanzar + Disparar).

---

## 4. Componente Transversal: `VirtualGamepad`

Se abstrae el control de entrada de todos los juegos y emuladores en un componente único e independiente.

### 4.1 Arquitectura del Componente
```
        ┌──────────────────────────────────────────────┐
        │        Hardware Touch GT911 (I2C)            │
        └──────────────────────┬───────────────────────┘
                               │ (Multitouch: 2 Puntos)
                               ▼
        ┌──────────────────────────────────────────────┐
        │            VirtualGamepad Mux                │
        ├──────────────────────────────────────────────┤
        │ Modo A: Zonas Invisibles (320x480)           │
        │ Modo B: Overlay D-Pad + Botones A/B (LVGL)   │
        └──────────────────────┬───────────────────────┘
                               │ Bitmask (g_zoneBits / GamepadState)
                               ▼
        ┌──────────────────────────────────────────────┐
        │   Contrato IGameEngine (keyEvent / state)     │
        └──────────────────────────────────────────────┘
```

---

## 5. Integración del Motor DOOM / WAD Loader (`GameView`)

### 5.1 Arquitectura de Carga Lazy
- **Memoria**: Allocación dinámica de **canvas RGB565 (128 KB)** en PSRAM al abrir la App.
- **Streaming de WAD**: Apertura por páginas desde la SD (`A:/doom/doom1.wad`), **pico total de PSRAM: ~1.1 MB**.
- **Liberación en Destrucción**: `destroyTransient()` detiene la RTOS task del Core 1 y ejecuta `free()` del canvas y recursos del WAD, restaurando la PSRAM intacta.

---

## 6. Hoja de Ruta de Implementación

| Fase | Tarea | Entregables / Criterios de Aceptación |
| :-: | :--- | :--- |
| **Fase 1** | **Multitouch Driver** | Extender `JC3248W535_Touch` para retornar array de 2 puntos GT911. |
| **Fase 2** | **VirtualGamepad** | Crear la clase `VirtualGamepad` con soporte multitouch y bitmask. |
| **Fase 3** | **Evaluación Motor `doomgeneric`** | Compilar `doomgeneric` en `env:esp32` (Core 1, ≥15 FPS). |
| **Fase 4** | **Evaluación Motor `hereticgeneric`** | Adaptar Chocolate Heretic C core bajo la misma interfaz `IGameEngine`. |
| **Fase 5** | **Integración Lazy `GameView`** | Conectar `DoomView` y `HereticView` a `UIManager` con cero fugas de PSRAM. |
