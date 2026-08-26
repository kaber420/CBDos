# 📖 Portal de Documentación Oficial de CBDos (CyBerDeck OS)

Bienvenido al centro de documentación técnica y manuales de desarrollo de **CBDos**, un sistema operativo embebido multi-target desacoplado para microcontroladores ESP32-P4 y ESP32-S3.

---

## 🗺️ Mapa de Navegación

```
docs/
├── 📚 api/               -> Referencia de APIs del SDK y Guías de Desarrollo
├── 🏛️ architecture/      -> Filosofía del Core agnóstico, HAL y patrones
├── 🔌 hardware/          -> Mapas de pines GPIO, conectores y periféricos
├── 📱 apps/              -> Especificaciones funcionales de cada aplicación
├── 💾 storage/           -> Almacenamiento SPIFFS, MicroSD y persistencia NVS
├── 🌐 network/           -> Conectividad WiFi y coprocesadores de red
└── 📝 drafts/            -> Borradores de trabajo e ideas de evolución
```

---

### 1. 📚 [SDK y Guías de Desarrollo (`docs/api/`)](api/)
* **[Guía para Desarrollar una App en C++](api/how_to_create_an_app.md):** Tutorial paso a paso con código de ejemplo para crear vistas nativas en LVGL 9.5.
* **[Especificación de Lua++ y Formato `.luapp`](api/luapp_specification.md):** Guía de desarrollo de micro-aplicaciones dinámicas en Lua++ sin necesidad de compilar.
* **[Referencia de APIs del Sistema (SDK)](api/core_apis_reference.md):** Documentación completa de `cbdos::system`, `storage`, `audio`, `uart`, `flasher`, `display`, `network` y `DefaultTheme`.

---

### 2. 🏛️ [Arquitectura y HAL (`docs/architecture/`)](architecture/)
* **[Arquitectura Agnóstica y HAL](architecture/hal_and_core_architecture.md):** Filosofía de desacoplamiento, Ley de Pureza de `core/`, contratos C++ y tabla maestra de módulos HAL.
* **[Análisis de Modularización](architecture/modularization_analysis.md):** Historial y decisiones de desacoplamiento y modularidad.

---

### 3. 🔌 [Hardware y Pinouts (`docs/hardware/`)](hardware/)
* **[Mapa Completo de Pines y Puertos](hardware/pinouts_and_ports.md):** Pinouts GPIO de placas ESP32-P4 (JC4880) y ESP32-S3 (JC3248), pines del conector JP1, buses I2C/I2S/SPI y tabla de pines UART/Flasheador.
* **[Resumen de Hardware y Backups](hardware/hardware_summary.md):** Especificaciones de memoria Flash, PSRAM y periféricos soportados.

---

### 4. 💾 [Almacenamiento y Persistencia (`docs/storage/`)](storage/)
* **[Especificación SPIFFS y MsgPack](storage/spiffs_and_msgpack_spec.md):** Almacenamiento Flash interno estructurado con serialización MessagePack.
* **[Persistencia NVS y FastBoot](storage/nvs_persistencia_fastboot.md):** Almacenamiento de preferencias del usuario y arranque rápido.
* **[Listas de Reproducción y Almacenamiento Portable](storage/playlists_and_portable_storage.md):** Gestión de música y archivos multimedia en MicroSD.

---

### 5. 🌐 [Red, Mesh y Conectividad (`docs/network/`)](network/)
* **[Bitácora de Validación del Gateway-Router y Pseudo-ARP](network/bitacora_validacion_gateway_router_y_pseudo_arp.md):** Resultados de pruebas reales en vivo de navegación TLVGL sobre ESP-NOW.
* **[Especificación de Direccionamiento IPv4 Mesh y Pseudo-ARP](network/especificacion_direccionamiento_ipv4_mesh_y_pseudo_arp.md):** Jerarquía `10.x.y.z`, Short IDs y tabla Pseudo-ARP en SQLite3.
* **[Análisis Exhaustivo de Modelos de Identidad Mesh](network/analisis_exhaustivo_modelos_identidad_y_direccionamiento_mesh.md):** Estudio matemático de colisiones y escalabilidad de direccionamiento.
* **[Arquitectura de Pool Dinámico y Traducción UUID](network/arquitectura_pool_dinamico_short_id_y_traduccion_uuid.md):** Modelo de Pseudo-NAT asimétrico (Aire 3B $\leftrightarrow$ WAN 4B).
* **[Pruebas de Fragmentación y MicroChunking Mesh](network/pruebas_fragmentacion_microchunking_mesh.md):** Protocolo de micro-chunks de 2B para fragmentación L2 en ESP-NOW.
* **[Guía de Coprocesador C6 Hosted](network/esp32_p4_c6_hosted_wifi.md):** Arquitectura de red WiFi 6 / Bluetooth con ESP32-C6 vía SDIO.
* **[Investigación de Firmware SDIO](network/investigacion_firmware_c6_sdio.md):** Análisis del firmware esclavo embebido.

---

### 6. 📝 [Borradores y Planes de Fase (`docs/drafts/`)](drafts/)
* Planes de desarrollo de cartuchos Lua/Pico8, emuladores y sintetizador de audio.
