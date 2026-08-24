# 📋 Borrador: SplashScreen de Arranque y Diagnóstico del Sistema ("Acerca de")

> **Proyecto:** CyBerDeck OS (CBDos v0.2.0)  
> **Ubicación:** `cbdos/`  
> **Fecha:** 19 de Agosto, 2026  

---

## 1. 🚀 Pantalla de Inicio (SplashScreen de Arranque)

### Objetivo
Ocultar el proceso de arranque en frío (montaje de partición LittleFS en Flash, asignación de búferes en PSRAM, calibración táctil e inicialización del stack de red) mostrando una pantalla de bienvenida durante **1.5 segundos** antes de pasar fluidamente al Dashboard.

### Composición Visual en Pantalla
```
┌────────────────────────────────────────────────────────┐
│                                                        │
│                                                        │
│                       ⚡                              │
│                 CyBerDeck OS                           │
│             v0.2.0 - Universal Core                    │
│                                                        │
│        Target: ESP32-P4 (480x800 @ 60 FPS)             │
│        [====================            ]              │
│                                                        │
│                                                        │
└────────────────────────────────────────────────────────┘
```

### Especificaciones de Diseño:
* **Fondo:** **Negro puro sólido (`#000000`)**, sin ningún wallpaper ni distracciones visuales.
* **Icono Central:** Símbolo / glifo minimalista de CyBerDeck OS en color Cyan Neón (`#00F5D4`).
* **Tipografía:** 
  * Título: `Montserrat 24/28` en color Blanco puro (`#FFFFFF`).
  * Subtítulo: `Montserrat 14` en color Violeta neón (`#9D4EDD`).
  * Detección de Target: `Montserrat 12` en color atenuado (`#64748B`).
    * En ESP32-P4: `"JC4880P443C (480x800 MIPI-DSI @ 60 FPS)"`
    * En ESP32-S3: `"JC3248W535 (320x480 QSPI @ 30 FPS)"`
* **Indicador de Carga:** Barra delgada minimalista en la parte inferior en color Cyan (`#00F5D4`).
* **Temporizador:** 1.500 ms de duración. Al concluir, realiza la transición directa al Dashboard (donde se carga el fondo de pantalla).

---

## 2. 🔍 Ventana de Diagnóstico del Sistema ("Acerca de")

### Objetivo
Permite al usuario verificar el estado en tiempo real del hardware, memoria libre, red, hora y almacenamiento desde **Configuración -> Sistema**, sin necesidad de reiniciar ni conectar cables de depuración USB serie.

### Estructura de la Tarjeta Modal (`DiagnosticsModal`)

```
┌────────────────────────────────────────────────────────┐
│               Diagnóstico del Sistema                  │
├────────────────────────────────────────────────────────┤
│ [Hardware]  Chip:       ESP32-P4 / ESP32-S3            │
│ [Pantalla]  Resolución: 480x800 @ 60FPS / 320x480      │
│ [Sistema]   Uptime:     00h 14m 23s                    │
│ [Hora Real] NTP:        14:30:15 (Sincronizado OK)     │
├────────────────────────────────────────────────────────┤
│ [Memoria]   Heap Libre:  285 KB                        │
│ [Memoria]   PSRAM Libre: 7.850 KB                      │
├────────────────────────────────────────────────────────┤
│ [WiFi]      Estado:     Conectado (MiRed_5G)           │
│ [WiFi]      IP:         192.168.1.145                  │
│ [WiFi]      MAC:        80:F1:B2:D1:7F:D5              │
│ [WiFi]      Señal:      -54 dBm (Excelente)            │
├────────────────────────────────────────────────────────┤
│ [Flash]     LittleFS:   /wallpaper.bin (Guardado OK)   │
│ [SD Card]   Estado:     Montada / Sin Tarjeta          │
├────────────────────────────────────────────────────────┤
│                  [ Cerrar Diagnóstico ]                │
└────────────────────────────────────────────────────────┘
```

### Especificaciones de Diseño:
* **Capa:** Se monta en `lv_layer_top()` como ventana modal instantánea.
* **Tarjeta:** Glassmorphism (`DefaultTheme::applyRaisedCard`, radio 16px, borde `#3B4252`).
* **Filas de Información:** Pares de clave-valor con alineación justificada:
  * Etiqueta en color atenuado (`#94A3B8`, Montserrat 12).
  * Valor en color texto principal (`#FFFFFF` si está OK, `#EF4444` si hay error o está desconectado).
* **Botón de Cierre:** Botón en la parte inferior con acento Cyan (`#00F5D4`) que elimina el modal con `lv_obj_delete_async()`.

---

## 3. 🛠️ Archivos del Proyecto a Crear / Modificar

| Archivo | Acción | Descripción |
| :--- | :--- | :--- |
| `core/src/ui/views/SplashScreenView.hpp` & `.cpp` | **NUEVO** | Vista del Splash Screen con temporizador de 1.5s. |
| `core/src/ui/modals/DiagnosticsModal.hpp` & `.cpp` | **NUEVO** | Modal flotante con la tarjeta de información y diagnóstico en tiempo real. |
| `core/src/ui/views/ConfigView.cpp` | **MODIFICAR** | Conectar la opción 6 ("Sistema") a `DiagnosticsModal::show()`. |
| `core/src/ui/UIManager.cpp` | **MODIFICAR** | En `init()`, cargar `SplashScreenView` como primera vista antes del Dashboard. |
| `core/include/cbdos/system.hpp` | **MODIFICAR** | Añadir estructura agnóstica `SystemStats` para consultar Heap, PSRAM, Uptime. |

---

## 4. 📌 Comparativa: ¿Dónde conviene ubicar cada elemento?

1. **SplashScreen:**
   * **Ubicación obligatoria:** Únicamente durante los primeros 1.5 segundos del arranque. Da identidad al encender y enmascara la inicialización técnica.
2. **Diagnóstico / Acerca de:**
   * **Ubicación recomendada:** Dentro del menú de **Configuración -> Sistema**, ya que es donde el usuario busca deliberadamente detalles técnicos, IP, memoria libre y estado del hardware.
