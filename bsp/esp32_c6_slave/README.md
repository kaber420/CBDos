# Target Coprocesador: ESP32-C6 SDIO Slave (CBDos)

Este directorio contiene el firmware oficial esclavo **ESP-Hosted SDIO** para el coprocesador inalámbrico **ESP32-C6** en la placa Guition JC4880P443C.

---

## ⚙️ Características Técnicas
* **SoC Coprocesador:** ESP32-C6FH4 (4MB Flash interna, RISC-V 160 MHz).
* **Funciones:** Wi-Fi 6 (802.11ax) 2.4 GHz + Bluetooth 5 (LE).
* **Bus de Comunicación con ESP32-P4:** SDIO Slave 4-bit (`GPIO 18..23`).
* **Velocidad de Reloj SDIO:** 20 MHz (80 Mbps de ancho de banda).

---

## 🔨 Compilación del Firmware

```bash
# Cargar entorno ESP-IDF 5.5
source /home/kaber420/esp/esp-idf/export.sh

# Compilar
cd bsp/esp32_c6_slave
idf.py build
```

El binario resultante se genera en:
`bsp/esp32_c6_slave/build/network_adapter.bin`

---

## 🔌 Guía de Conexión de Jumpers y Flasheo

Para las instrucciones detalladas paso a paso de los puentes físicos en el conector JP1 y el comando de `esptool`:
👉 **Ver [`tools/c6_flasher_bridge/README.md`](file:///home/kaber420/Documentos/proyectos/cbdos/tools/c6_flasher_bridge/README.md)**
