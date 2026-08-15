# CBDos — Sistema Operativo Táctil, Ecosistema Mesh & Navegador TLVGL / BCML

<p align="center">
  <img src="https://img.shields.io/badge/Platform-ESP32--S3-blue?style=for-the-badge&logo=espressif" alt="ESP32-S3"/>
  <img src="https://img.shields.io/badge/UI_Framework-LVGL_v9.5-orange?style=for-the-badge" alt="LVGL v9"/>
  <img src="https://img.shields.io/badge/Display-AMOLED%2FLCD_QSPI_320x480-purple?style=for-the-badge" alt="Display"/>
  <img src="https://img.shields.io/badge/Status-Active_Development-success?style=for-the-badge" alt="Status"/>
</p>

**CBDos** es un sistema operativo multitarea y ecosistema de aplicaciones diseñado específicamente para microcontroladores **ESP32-S3** con pantalla táctil capacitiva (AMOLED/LCD QSPI 320x480). 

Integra una suite completa de aplicaciones nativas para productividad, multimedia (streaming y local), emulación de videojuegos clásicos mediante arquitectura de cartuchos OTA, utilidades del sistema y un revolucionario **navegador binario hiperligero (TLVGL / CBML)** capaz de renderizar páginas dinámicas a 60 FPS sobre redes de radio de bajo ancho de banda (Mesh / LoRa / FLRC / WiFi).

---

## 📊 Estado Actual del Proyecto y Suite de Aplicaciones

| Componente / App | Estado | Operatividad | Descripción |
| :--- | :---: | :---: | :--- |
| **Dashboard & Launcher** | 🟢 Estable | **100%** | Grid dinámico con scroll, barra de estado superior (`HeaderBar`), reloj, batería, RSSI y wallpapers. |
| **Sistema de Fondos (Wallpaper)** | 🟢 Estable | **100%** | Selección híbrida de fondos de pantalla: predeterminados en Flash o imágenes JPG dinámicas desde MicroSD. |
| **Radio Web Online** | 🟢 Estable | **95%** | Streaming HTTP de emisoras Shoutcast/Icecast con decodificación MP3 por hardware/software (`libhelix`) y reconexión automática. |
| **Reproductor de Música** | 🟢 Estable | **90%** | Reproducción de archivos MP3 locales almacenados en la tarjeta MicroSD con barra de progreso y control de volumen. |
| **Visor de Medios / Galería** | 🟢 Estable | **90%** | Explorador de archivos en MicroSD con decodificación y renderizado nativo en pantalla de imágenes JPG, PNG y BMP. |
| **Cartucho DOOM (app1)** | 🟢 Estable | **95%** | Motor nativo `doomgeneric` optimizado en PSRAM con pad virtual táctil (`CartridgeGamepad`), soporte para mandos Bluetooth BLE y retorno seguro al OS. |
| **Cartucho Game Boy Color (app2)** | 🟡 Beta | **75%** | Emulador nativo `Peanut-GB` con overlay clásico Game Boy y selector de ROMs (`.gb` / `.gbc`) desde MicroSD. |
| **BLE Gamepad Tester** | 🟢 Estable | **95%** | Sniffer y analizador de mandos inalámbricos Bluetooth para inspección de paquetes HID en tiempo real. |
| **Todo App / Bloc de Notas** | 🟢 Estable | **90%** | Gestor de tareas con casillas de verificación (*checkboxes*) y persistencia de notas en tarjeta MicroSD (`/notes`). |
| **Cronómetro / Timer** | 🟢 Estable | **95%** | Cronómetro digital de precisión con soporte para registro de vueltas (*laps*). |
| **Calculadora** | 🟡 En Desarrollo | **60%** | Esqueleto UI y operaciones aritméticas básicas operativas. *Detalle:* En números grandes o desbordamientos la representación de caracteres requiere ajuste de formato numérico. |
| **Navegador TLVGL / Mesh** | 🟢 Estable | **10%** | Renderizado binario de páginas CBML servidas por el Router Mesh en Go y el servidor de hosting Python. |
| **Gestor de Conectividad & Radio** | 🟢 Estable | **50%** | Paneles de configuración para WiFi, Gateway Mesh, MQTT y transceptores RF duales (LoRa / FLRC SX1280). |

---

## 🎮 Emulación y Arquitectura de Cartuchos (Dual/Multi Firmware OTA)

Para superar los límites de memoria y garantizar el máximo rendimiento sin sacrificar la interfaz de usuario, CBDos implementa un esquema de **particionado modular multi-cartucho**:

```text
[ Flash 16MB SPI ]
├── 0x010000 (ota_0 - 6MB): CBDos (Sistema Operativo, Dashboard, WiFi, LVGL v9)
├── 0x610000 (ota_1 - 4MB): Cartucho DOOM (Motor Nativo en PSRAM + Virtual Gamepad)
├── 0xA10000 (ota_2 - 4MB): Cartucho Game Boy Color (Emulador Peanut-GB)
└── 0xE10000 (nvs / data):  Almacenamiento de Sistema y Configuraciones
```

### 🚀 Comandos de Despliegue (PlatformIO)
Dado el sistema de particiones, cada entorno debe compilarse y flashearse de manera independiente:
* `pio run -e esp32 -t upload` *(CBDos - OS Principal)*
* `pio run -e doom -t upload` *(Cartucho DOOM)*
* `pio run -e gbc -t upload` *(Cartucho Game Boy)*

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
│   │   ├── UI/Views/         # Vistas: Dashboard, Radio, Música, Galería, Config, etc.
│   │   │   └── Utilities/    # Calculadora, TodoApp/Notas, Cronómetro
│   │   ├── Core/             # Drivers: Audio Nativo, LVFS, TLV Parser
│   │   ├── DoomLauncher.cpp  # Lanzador Cartucho DOOM
│   │   └── GBCLauncher.cpp   # Lanzador Cartucho Game Boy Color
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
