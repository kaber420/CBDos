# Plan: Cyberdeck Retro para JC3248W535

## Concepto

Un cyberdeck con estética retro (años 90s) pero capacidades modernas:
- Navegador web real (Wikipedia offline/online)
- Mensajería mesh (sin dependencia de internet)
- Música, fotos, juegos ligeros
- Form factor compacto, táctil, autónomo

## Hardware Objetivo

### JC3248W535 (principal)
- ESP32-S3
- 320x480 pantalla táctil
- Touch I2C
- MicroSD en SPI (CS=10, MOSI=11, SCLK=12, MISO=13)
- 16MB flash
- PSRAM OPI (8MB)

### Periféricos adicionales
- LoRa 2.4GHz (SX1280 con amplificador) — mesh networking
- Speaker/headphone — audio
- GPS (futuro) — ubicación

## Arquitectura del Sistema

```
┌─────────────────────────────────────┐
│  📱 OS / Launcher                    │
│  (siempre activo, ~20KB LVGL)       │
│                                     │
│  ┌─────────────────────────────┐    │
│  │  Menú Principal (grid)      │    │
│  │  [Browser] [MeshChat]       │    │
│  │  [Music]   [Weather]        │    │
│  │  [Notes]   [Settings]       │    │
│  └─────────────────────────────┘    │
│         │      │      │             │
│    ┌────┘      │      └────┐        │
│    ▼           ▼           ▼        │
│  ┌──────┐  ┌──────┐  ┌──────┐      │
│  │ App1 │  │ App2 │  │ App3 │      │
│  │(load)│  │(load)│  │(load)│      │
│  └──────┘  └──────┘  └──────┘      │
└─────────────────────────────────────┘
```

### Servicios Compartidos (OS)

| Servicio | Descripción |
|----------|-------------|
| **Display** | LVGL, 320x480, touch I2C |
| **WiFi** | Conexión compartida, reconnect automático |
| **SD** | Acceso compartido a MicroSD |
| **Storage** | Config, datos de apps, caché |
| **Memory** | Gestión de PSRAM, cleanup al cambiar de app |
| **Input** | Touch events, botones hardware |

### Gestión de Apps

```
App lifecycle:
  1. User toca ícono → OS llama app_create()
  2. App crea sus widgets LVGL
  3. App usa servicios del OS (WiFi, SD, etc.)
  4. User toca "Volver" → OS llama app_destroy()
  5. App destruye sus widgets, libera memoria
  6. OS muestra menú principal
```

```cpp
// Interfaz de cada app
struct App {
    const char* name;
    const char* icon;        // emoji o path a imagen
    void (*create)(void);    // crear UI
    void (*destroy)(void);   // destruir UI, liberar memoria
};
```

## Aplicaciones

### Prioridad Alta (Core)

#### 1. Picobrowser (Navegador actual)
- Parser HTML propio
- HTML 4.01 Transitional
- CSS style attribute
- HTTP/HTTPS
- Use case: Wikipedia offline/online
- STATUS: En desarrollo

#### 2. LWS Browser (futuro)
- Parser LHP de libwebsockets
- HTML5/CSS más estándar
- Misma UI que Picobrowser
- Use case: Wikipedia con better formatting
- STATUS: Planificado

#### 3. MeshChat (Mensajería mesh)
- LoRa 2.4GHz (SX1280)
- Msgpack para serialización
- MeshCore/MeshStatic para routing
- Mensajes privados + broadcast
- Use case: Comunicación sin internet
- STATUS: Por definir

### Prioridad Media (Apps)

#### 4. Music Player
- Audio desde SD (MP3, FLAC)
- Streaming WiFi (Shoutcast, etc.)
- UI con visualización de spectrum
- STATUS: Por definir

#### 5. Photo Viewer
- Ver fotos desde SD
- Slideshow automático
- Thumbnails para navegar
- STATUS: Por definir

### Prioridad Baja (Extras)

#### 6. Games
- Snake (clásico Nokia)
- Tetris
- Text adventures
- Quiz/trivia (descargable vía mesh)
- NO: Doom, NES, juegos pesados
- STATUS: Por definir

#### 7. Weather
- API: OpenWeatherMap, etc.
- Mostrar temperatura, humedad, pronóstico
- Fallback offline con datos cacheados
- STATUS: Por definir

#### 8. Notes
- Notas de texto en SD
- Editor simple con LVGL
- Sync con mesh (futuro)
- STATUS: Por definir

#### 9. Settings
- WiFi config (ssid, pass)
- Mesh config (callsign, red)
- Display brightness
- Sound volume
- STATUS: Por definir

## Memoria y Recursos

### Distribución típica

| Componente | SRAM Interna | PSRAM |
|-----------|-------------|-------|
| OS/Launcher | ~30KB | - |
| App activa | ~50-80KB | - |
| Display buffer | - | ~300KB |
| Audio buffer | - | ~32KB |
| HTTP response | ~10-50KB | Opcional |
| Mesh buffers | - | ~64KB |

### Estrategia de memoria
- **SRAM interna**: Stack FreeRTOS, heap para LVGL widgets, strings
- **PSRAM**: Buffers grandes (display, audio, imágenes, mesh)
- **SD**: Assets, caché, datos persistentes
- **Flash**: Código, firmware, configuración

### Limpieza al cambiar de app
```cpp
void switch_app(App* new_app) {
    if (current_app) {
        current_app->destroy();  // Destruir widgets LVGL
    }
    lv_obj_clean(screen);        // Limpiar pantalla
    new_app->create();           // Crear nueva app
    current_app = new_app;
}
```

## Mesh Network

Para una especificación completa del direccionamiento jerárquico de 10 bytes, las cabeceras global/local y el comportamiento del gateway proxy, consulta [custom_mesh_protocol.md](file:///home/kaber420/Documentos/proyectos/lvgl-test/drafts/custom_mesh_protocol.md).

### Stack

```
┌─────────────────────────┐
│  MeshChat UI (LVGL)     │
├─────────────────────────┤
│  Msgpack serialize/     │
│  deserialize            │
├─────────────────────────┤
│  MeshCore/Static        │
│  (routing, encriptación)│
├─────────────────────────┤
│  LoRa 2.4GHz radio      │
│  (SX1280 + amplificador)│
└─────────────────────────┘
```

### Protocolo de mensajes (msgpack)
```cpp
// Mensaje de texto
{
    "type": "msg",
    "from": "kaber",
    "to": "all",         // o "juan"
    "text": "Hola mundo",
    "ts": 1234567890
}

// Mensaje de sistema
{
    "type": "sys",
    "from": "node1",
    "action": "heartbeat",
    "ts": 1234567890
}
```

### SX1280 Specs
- Frecuencia: 2.4GHz
- Velocidad: hasta 1.3 Mbps
- Alcance: ~1km (urbano), ~5km (linea vista)
- Modulación: LoRa, FSK, BLE
- Amplificador incluido

## Use Cases Principales

### 1. Wikipedia Offline
- Dump de Wikipedia comprimido en SD
- Acceso sin internet
- Navegación por artículos
- Search local

### 2. Wikipedia Online
- Acceso vía WiFi a Wikipedia real
- Renderizado en navegador retro
- Ahorro de datos (solo texto)

### 3. Mesh Messaging
- Comunicación sin internet
- Mensajes privados y broadcast
- Encriptación básica
- Ideal para eventos, emergencias, zonas sin cobertura

### 4. Música Portátil
- Reproducir MP3/FLAC desde SD
- Streaming WiFi (Shoutcast, etc.)
- UI retro con visualización

### 5. Galería de Fotos
- Ver fotos desde SD
- Slideshow automático
- Compartir fotos vía mesh (futuro)

### 6. Juegos Ligeros
- Snake, Tetris, Pong
- Text adventures
- Quiz/trivia descargable vía mesh

## Futuro Lejano

- **GPS**: Ubicación, mapas offline
- **Cámaras**: Ver fotos desde SD
- **App Store mesh**: Descargar apps desde la red
- **Multi-device**: Sincronización entre dispositivos
- **Wikipedia mesh**: Nodos compartiendo artículos en la red
- **Teclado virtual**: Para输入 de texto más cómodo
- **Soporte teclado físico**: USB OTG para teclados mecánicos

## Notas

- Este es un **cyberdeck retro**, no un emulador histórico
- Estética de los 90s (gris, bordes 3D, fuentes bitmap) con capacidades modernas
- El navegador es **una app** dentro del sistema, no el foco principal
- Use case principal: **Wikipedia access** (offline/online/mesh)
- Cada app es independiente y se puede desarrollar por separado
- El OS/launcher es el núcleo que mantiene todo junto
- Prioridad: primero completar el navegador, luego expandir a otras apps
- NO Doom, NO NES — juegos ligeros solamente
- El valor está en la utilidad real (Wikipedia, mesh, música) no en la potencia brute
