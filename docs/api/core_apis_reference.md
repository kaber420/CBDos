# 📚 Manual de APIs del Sistema (CBDos SDK & Core APIs)

Este documento es la **guía oficial de referencia para desarrolladores**. Aquí se describen todas las APIs, servicios y abstracciones que ofrece el núcleo de **CBDos** para crear aplicaciones, juegos y herramientas de sistema.

---

## 🧭 Índice de APIs
1. [cbdos::system - Información y Control del Sistema](#1-cbdossystem)
2. [cbdos::storage - Sistema de Archivos (SD y Flash)](#2-cbdosstorage)
3. [cbdos::audio - Reproducción de Sonido y Radio](#3-cbdosaudio)
4. [cbdos::uart - Comunicación Serial](#4-cbdosuart)
5. [cbdos::flasher - Flasheo Universal de Microcontroladores](#5-cbdosflasher)
6. [cbdos::display - Capacidades de Pantalla y Brillo](#6-cbdosdisplay)
7. [cbdos::network - Conectividad WiFi y Tiempo](#7-cbdosnetwork)
8. [cbdos::theme & DefaultTheme - Sistema de Estilos LVGL](#8-cbdostheme--defaulttheme)

---

## 1. `cbdos::system`
Header: `#include "cbdos/system.hpp"`

Permite consultar métricas de rendimiento, memoria, temperatura y control del microcontrolador.

```cpp
// Obtener tiempos (milisegundos y microsegundos)
uint32_t ms = cbdos::system::getTimeMs();
uint64_t us = cbdos::system::getTimeUs();

// Pausas y Yield
cbdos::system::sleepMs(50);
cbdos::system::yieldTask();

// Métricas de Memoria RAM (Internal Heap y PSRAM)
size_t freeHeap   = cbdos::system::getFreeHeap();
size_t totalHeap  = cbdos::system::getTotalHeap();
size_t freePsram  = cbdos::system::getFreePsram();
size_t totalPsram = cbdos::system::getTotalPsram();

// Temperatura del SoC en °C
float temp = cbdos::system::getCpuTemperature();

// Logs estructurados del sistema
cbdos::system::log(cbdos::system::LogLevel::Info, "MiApp", "Iniciando proceso con valor: %d", 42);

// Reinicio de hardware
cbdos::system::restart();
```

---

## 2. `cbdos::storage`
Header: `#include "cbdos/storage.hpp"`

Abstracción transparente para trabajar con la memoria Flash interna (`/spiffs`) y la tarjeta MicroSD (`/sdcard`).

```cpp
// Comprobar estado de montaje
bool sdOk    = cbdos::storage::isSdMounted();
bool flashOk = cbdos::storage::isFlashMounted();

// Listar archivos de un directorio
std::vector<cbdos::storage::FileEntry> files = cbdos::storage::listDir("/sdcard/music");
for (const auto& file : files) {
    printf("Nombre: %s | Tamaño: %u bytes | Es Dir: %s\n", 
           file.name.c_str(), (unsigned)file.size, file.isDirectory ? "SI" : "NO");
}

// Comprobar existencia
if (cbdos::storage::fileExists("/sdcard/config.json")) { ... }

// Leer archivo completo a std::string
std::string data = cbdos::storage::readFile("/sdcard/nota.txt");

// Escribir archivo completo
bool ok = cbdos::storage::writeFile("/sdcard/logs/test.txt", "Hola mundo desde CBDos!");

// Crear carpetas
cbdos::storage::makeDir("/sdcard/mis_datos");

// Borrar y Copiar
cbdos::storage::deleteFile("/sdcard/archivo_viejo.txt");
cbdos::storage::copyFile("/spiffs/default.cfg", "/sdcard/config.cfg");

// Estadísticas de almacenamiento
cbdos::storage::StorageStats sdStats = cbdos::storage::getSdCardStats();
printf("SD Total: %llu MB | Libre: %llu MB\n", sdStats.totalBytes / (1024*1024), sdStats.freeBytes / (1024*1024));
```

---

## 3. `cbdos::audio`
Header: `#include "cbdos/audio.hpp"`

Control del subsistema de sonido, códec I2S y volumen general.

```cpp
// Control de volumen general (0 a 100)
cbdos::audio::setVolume(80);
int vol = cbdos::audio::getVolume();

// Mute / Unmute
cbdos::audio::setMute(true);
bool muted = cbdos::audio::isMuted();

// Salida de audio activa
cbdos::audio::setAudioOutput(cbdos::audio::AudioOutput::Speaker);
```

---

## 4. `cbdos::uart`
Header: `#include "cbdos/uart.hpp"`

Control bidireccional de puertos serie de hardware (UART) para consolas, comunicación con sensores o pentesting de routers.

```cpp
// Inicializar UART en pines específicos y velocidad deseada
int txPin = cbdos::uart::getDefaultTxPin();
int rxPin = cbdos::uart::getDefaultRxPin();
cbdos::uart::init(txPin, rxPin, 115200);

// Comprobar bytes disponibles
size_t avail = cbdos::uart::available();
if (avail > 0) {
    // Leer como string
    std::string text = cbdos::uart::readString(avail);
    
    // O leer a buffer binario
    uint8_t buf[128];
    size_t bytesRead = cbdos::uart::read(buf, sizeof(buf));
}

// Enviar comandos / datos
cbdos::uart::writeString("help\r\n");
cbdos::uart::write((const uint8_t*)"\x03", 1); // Enviar Ctrl+C

// Cambiar velocidad al vuelo
cbdos::uart::setBaudrate(9600);

// Cerrar puerto
cbdos::uart::deinit();
```

---

## 5. `cbdos::flasher`
Header: `#include "cbdos/flasher.hpp"`

Servicio autónomo para quemar firmware a microcontroladores ESP conectados por UART.

```cpp
// Obtener presets disponibles para la placa actual
const auto& presets = cbdos::flasher::getPresets();

// Configurar flasheo personalizado
cbdos::flasher::FlasherConfig cfg;
cfg.txPin = 32;
cfg.rxPin = 28;
cfg.bootPin = 34;
cfg.rstPin = 54;
cfg.baudRate = 115200;
cfg.flashOffset = 0x0;
cfg.binPath = "/sdcard/nuevo_firmware.bin";

// Iniciar flasheo no bloqueante con callback de progreso
cbdos::flasher::startFlash(cfg, [](cbdos::flasher::FlasherStatus status, int pct, const char* msg) {
    printf("[Progreso: %d%%] Estado: %s\n", pct, msg);
});
```

---

## 6. `cbdos::display`
Header: `#include "cbdos/display.hpp"`

Consulta de resolución y control de brillo de pantalla.

```cpp
// Obtener capacidades del display actual
cbdos::display::DisplayCaps caps = cbdos::display::getCapabilities();
printf("Resolución: %dx%d @ %d FPS | Color: %s\n", 
       caps.width, caps.height, caps.targetFps, caps.colorFormat.c_str());

// Ajustar brillo de pantalla (0 a 100%)
cbdos::display::setBrightness(90);
int brightness = cbdos::display::getBrightness();
```

---

## 7. `cbdos::network`
Header: `#include "cbdos/network.hpp"`

Estado y control de conectividad WiFi y sincronización horaria.

```cpp
// Estado de conexión
cbdos::network::NetworkStatus status = cbdos::network::getStatus();
if (status == cbdos::network::NetworkStatus::Connected) {
    std::string ip = cbdos::network::getIpAddress();
    int rssi = cbdos::network::getRssi();
    printf("Conectado con IP: %s (Señal: %d dBm)\n", ip.c_str(), rssi);
}

// Conectar a red WiFi bajo demanda
cbdos::network::connectWifi("MiWiFi_SSID", "MiPassword123");
```

---

## 8. `DefaultTheme` y Estilos de Interfaz (LVGL 9.5)
Header: `#include "../themes/DefaultTheme.h"`

Helpers visuales predefinidos para mantener una estética consistente y atractiva en todo el sistema operativo.

```cpp
// Crear una tarjeta con elevación (sombra y borde sutil)
lv_obj_t* card = lv_obj_create(parent);
lv_obj_set_size(card, 200, 100);
DefaultTheme::applyRaisedCard(card, 12); // Radio de 12px

// Crear un contenedor hundido (ideal para paneles de datos o terminales)
lv_obj_t* sunken = lv_obj_create(parent);
DefaultTheme::applySunkenCard(sunken, 8);

// Aplicar estilo de botón estándar
lv_obj_t* btn = lv_button_create(parent);
DefaultTheme::applyButton(btn, 10);

// Colores oficiales del tema
lv_color_t bg       = DefaultTheme::getBgColor();          // Fondo oscuro (#161821)
lv_color_t primary  = DefaultTheme::getPrimaryAccent();     // Verde azulado (#00F5D4)
lv_color_t second   = DefaultTheme::getSecondaryAccent();   // Violeta Cyberpunk (#9D4EDD)
lv_color_t text     = DefaultTheme::getTextColor();         // Blanco suave (#F1F5F9)
lv_color_t muted    = DefaultTheme::getMutedTextColor();    // Gris (#94A3B8)
```
