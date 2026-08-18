# CBDos — Sistema Operativo Táctil, Ecosistema Mesh & Navegador TLVGL / BCML

<p align="center">
  <img src="https://img.shields.io/badge/Platform-ESP32--S3-blue?style=for-the-badge&logo=espressif" alt="ESP32-S3"/>
  <img src="https://img.shields.io/badge/UI_Framework-LVGL_v9.5-orange?style=for-the-badge" alt="LVGL v9"/>
  <img src="https://img.shields.io/badge/Display-AMOLED%2FLCD_QSPI_320x480-purple?style=for-the-badge" alt="Display"/>
  <img src="https://img.shields.io/badge/Status-Active_Development-success?style=for-the-badge" alt="Status"/>
</p>

**CBDos** es un sistema operativo multitarea y ecosistema de aplicaciones diseñado específicamente para microcontroladores **ESP32-S3** con pantalla táctil capacitiva (AMOLED/LCD QSPI 320x480). 

Utiliza **LVGL v9.5** como motor gráfico principal con gestión de memoria nativa optimizada en **PSRAM externa de 8MB** (`LV_MEM_POOL_ALLOC`), garantizando renderizado fluido a 60 FPS sin saturar la memoria interna SRAM del microcontrolador.

Integra una suite completa de aplicaciones nativas para productividad, multimedia (streaming y local), emulación de videojuegos clásicos mediante arquitectura de cartuchos OTA, utilidades del sistema y un revolucionario **navegador binario hiperligero (TLVGL / CBML)** capaz de renderizar páginas dinámicas a 60 FPS sobre redes de radio de bajo ancho de banda (Mesh / LoRa / FLRC / WiFi).

---

## 📊 Estado Actual del Proyecto y Suite de Aplicaciones

| Componente / App | Estado | Operatividad | Descripción |
| :--- | :---: | :---: | :--- |
| **Dashboard & Launcher** | 🟢 Estable | **100%** | Grid dinámico con scroll, barra de estado superior (`HeaderBar`), reloj, batería, RSSI y wallpapers. |
| **Gestor de Cartuchos (`CartridgeView`)** | 🟢 Estable | **100%** | Administrador de ranuras OTA fijas con inspección de firmware, validación anti-cuelgues y flasheo directo de `.bin` desde MicroSD. |
| **Sistema de Fondos (Wallpaper)** | 🟢 Estable | **100%** | Selección híbrida de fondos de pantalla: predeterminados en Flash o imágenes JPG dinámicas desde MicroSD. |
| **Motor Lua Script Runner (In-App)** | 🟢 Estable | **95%** | Motor de scripting integrado en CBDos para automatizaciones, consola interactiva y mini-apps UI sin reiniciar el SO. |
| **Cartucho MicroPython (app1)** | 🟡 En Integración | **85%** | Entorno Python autónomo en Ranura 1 (4MB) con 8MB PSRAM dedicados, USB REPL, WebREPL y montaje de `/sd`. |
| **Cartucho DOOM (app1)** | 🟢 Estable | **95%** | Motor nativo `doomgeneric` en PSRAM con pad virtual táctil (`CartridgeGamepad`), soporte mandos BLE y retorno seguro al OS. |
| **Cartucho Game Boy Color (app2)** | 🟡 Beta | **80%** | Emulador nativo `Peanut-GB` con overlay clásico Game Boy y selector de ROMs (`.gb` / `.gbc`) desde MicroSD. |
| **Radio Web Online** | 🟢 Estable | **95%** | Streaming HTTP de emisoras Shoutcast/Icecast con decodificación MP3 y AAC (`libhelix`) y reconexión automática. |
| **Reproductor de Música** | 🟢 Estable | **90%** | Reproducción de archivos MP3 locales almacenados en la tarjeta MicroSD. Incluye control de reproducción.|
| **Visor de Medios / Galería** | 🟢 Estable | **90%** | Explorador de archivos en MicroSD con decodificación y renderizado nativo en pantalla de imágenes JPG, PNG y BMP. |
| **BLE Gamepad Tester** | 🟢 Estable | **95%** | Sniffer y analizador de mandos inalámbricos Bluetooth para inspección de paquetes HID en tiempo real. |
| **Todo App / Bloc de Notas** | 🟢 Estable | **90%** | Gestor de tareas con casillas de verificación (*checkboxes*) y persistencia de notas en tarjeta MicroSD (`/notes`). |
| **Cronómetro / Timer** | 🟢 Estable | **95%** | Cronómetro digital de precisión con soporte para registro de vueltas (*laps*). |
| **Calculadora** | 🟡 En Desarrollo | **60%** | Esqueleto UI y operaciones aritméticas básicas operativas. |
| **Navegador TLVGL / Mesh** | 🟢 Estable | **10%** | Renderizado binario de páginas CBML servidas por el Router Mesh en Go y el servidor de hosting Python. |
| **Gestor de Conectividad & Radio** | 🟢 Estable | **50%** | Paneles de configuración para WiFi, Gateway Mesh, MQTT y transceptores RF duales (LoRa / FLRC SX1280). |

---

## 🎮 Arquitectura Multi-Slot de Cartuchos & Flasheo desde MicroSD

Para superar los límites de memoria y garantizar la ejecución aislada de juegos, emuladores y motores de programación pesados, CBDos implementa un esquema de **2 Ranuras Fijas de Memoria Flash (16MB)** con soporte para flashear binarios `.bin` directamente desde la tarjeta MicroSD sin necesidad de cables ni PC:

```text
[ Flash 16MB SPI ]
├── 0x010000 (app0 - 5.0MB): CBDos (Sistema Operativo Base, Dashboard, WiFi, LVGL v9.5)
├── 0x510000 (app1 - 4.0MB): Ranura 1 (Slot Grande) ◄── [MicroPython / DOOM / Motores 3D]
├── 0x910000 (app2 - 2.0MB): Ranura 2 (Slot Pequeño) ◄─ [Game Boy Color / NES / Testers]
├── 0xB10000 (spiffs - 4.0MB): LittleFS (Fondos de pantalla, fuentes y caché del sistema)
└── 0xF10000 (fatfs - 0.9MB): Almacenamiento FAT secundario
```

### 🚀 Características de la Arquitectura de Cartuchos:
* **Flasheo Directo desde MicroSD:** Puedes guardar tus ejecutables compilados en `/sd/cartridges/*.bin` y cargarlos a la Flash en 2-3 segundos desde la UI de CBDos (`CartridgeManager`).
* **Protección Anti-Cuelgues:** Inspección de cabeceras de firmware (`esp_app_desc_t`) para prevenir reinicios si una ranura está vacía.
* **Control Táctil Universal (`CartridgeGamepad`):** Overlay virtual táctil acelerado por hardware con multitouch GT911 y botón `[SALIR]` que conmuta la partición OTA para reiniciar de vuelta a CBDos.

### 💻 Despliegue por Cable (PlatformIO):
* `pio run -e esp32 -t upload` *(CBDos - OS Principal a 0x010000)*
* `pio run -e doom -t upload` *(DOOM a Ranura 1: 0x510000)*
* `pio run -e gbc -t upload` *(Game Boy Color a Ranura 2: 0x910000)*

* **Control Táctil Universal (`CartridgeGamepad`):** Overlay virtual acelerado por hardware con respuesta inmediata, multitouch GT911 y botón físico/táctil `[SALIR]` que conmuta la partición OTA para reiniciar de vuelta a CBDos sin pérdida de estado.
* **Soporte Mandos BLE:** Conexión inalámbrica a mandos Bluetooth (ej. iPega, DualShock, mandos genéricos HID).

---

## 📻 Multimedia & Audio

* **Streaming de Radio por Internet:** Conexión TCP con buffer elástico en PSRAM y decodificación optimizada para microcontroladores con I2S directo al DAC/amplificador (MAX98357A / ES8311).
* **Reproductor Local:** Navega por las carpetas de la MicroSD, lee metadatos básicos y reproduce listas de pistas de música en segundo plano.
* **Galería Gráfica:** Apertura fluida de imágenes gracias al subsistema de archivos `LVFS` y decodificadores de LVGL en memoria extendida.

---

## 🌐 Ecosistema Mesh & Navegador TLVGL / CBML

CBDos incluye un navegador de hipertexto binario diseñado para redes de microcontroladores:

```text
[ Usuario / Diseñador ] ──(HTML/CBML)──> [ Alternet Studio / Compilador TLVGL ]
                                                        │
                                                        ▼ (Bytecode binario TLV)
[ Nodo ESP32 Touch (CBDos) ] <──(Red Mesh / Radio)── [ Router Mesh Go ] <── [ Servidor de Hosting ]
```

* **CBML (Compact Binary Markup Language):** Lenguaje declarativo estilo HTML que se compila a etiquetas TLV ultracompactas (Type-Length-Value).
* **Zero Overhead de Parseo:** El ESP32 traduce directamente las tramas TLV a widgets LVGL v9 nativos a 60 FPS sin procesar DOM pesado ni ejecutar JavaScript en el nodo.
* **Router Mesh en Go (`cbdos-router`):** Enrutador de Capa 3/4 agnóstico al contenido con soporte para TCP y sockets Unix locales.

---

## 🛠️ Especificaciones de Hardware Soportado

* **Placa Target:** JC3248W535 (o compatibles ESP32-S3).
* **SoC:** ESP32-S3 Xtensa Dual-Core 240 MHz.
* **Memoria:** 16 MB Flash SPI + 8 MB OPI PSRAM (Octal SPI de alta velocidad).
* **Pantalla:** AMOLED / LCD QSPI 320x480 (Controlador AXS15231B).
* **Panel Táctil:** Capacitivo I2C (GT911 / AXS15231B) con soporte Multitouch.
* **Almacenamiento Externo:** Slot MicroSD por bus SPI dedicado.
* **Audio:** DAC I2S integrado (MAX98357A / ES8311).

---

## 🚀 Guía de Compilación y Uso

### 1. Requisitos Previos
* [PlatformIO Core](https://platformio.org/) o extensión de VSCode/PlatformIO IDE.
* Python 3.10+ (para las herramientas del Gateway y servidor de hosting).
* Go 1.22+ (para el Router Mesh).

### 2. Firmware ESP32-S3 (CBDos)
```bash
# Entrar al directorio del firmware
cd firmware

# Compilar el sistema operativo completo (entorno esp32)
pio run -e esp32

# Flashear a la placa ESP32-S3 conectada por USB
pio run -e esp32 -t upload

# Monitorear puerto serie con decodificador de excepciones activo
pio device monitor -b 115200 --filter esp32_exception_decoder
```

### 3. Gateway y Servidor de Hosting
```bash
# Iniciar Servidor de Hosting TLVGL (Python)
python3 gateway/tlvgl/tlvgl_server.py --port 8766

# Compilar e Iniciar Router Mesh en Go
cd gateway/router-go
go build -o build/router cmd/router/main.go
./build/router -tcp :8765 -hosting 127.0.0.1:8766
```

---

## 📁 Estructura del Repositorio

```text
├── firmware/                 # Código fuente C/C++ (PlatformIO)
│   ├── src/
│   │   ├── main.cpp          # Punto de entrada, init de hardware, LVGL loop
│   │   ├── UI/Views/         # Vistas: Dashboard, CartridgeView, LuaRunner, Radio, Música, etc.
│   │   │   └── Utilities/    # Calculadora, TodoApp/Notas, Cronómetro
│   │   ├── Core/             # CartridgeManager, LuaEngine, Audio Nativo, LVFS, StorageManager
│   │   ├── CartridgeGamepad.cpp # Gamepad virtual táctil multitouch para cartuchos
│   │   ├── DoomLauncher.cpp  # Punto de entrada Cartucho DOOM (app1)
│   │   └── GBCLauncher.cpp   # Punto de entrada Cartucho Game Boy Color (app2)
│   ├── include/              # Cabeceras del sistema y configuración de LVGL
│   └── platformio.ini        # Definición de entornos y librerías
├── gateway/
│   ├── router-go/            # Enrutador Mesh de alto rendimiento en Go
│   └── tlvgl/                # Servidor de aplicaciones y compilador CBML/HTML a TLV
└── drafts/                   # Especificaciones técnicas, RFCs y hojas de ruta
```

---

## 📄 Licencia

Proyecto desarrollado para el ecosistema **CBDos**.
