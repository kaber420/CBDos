# 🛰️ Bitácora Técnica: Validación en Vivo del Gateway-Router, Direccionamiento IPv4 Mesh y Tabla Pseudo-ARP (CBDos v0.2.1)

**Fecha:** 25 de Agosto, 2026  
**Estado:** ✅ VALIDADO EN HARDWARE REAL Y EN AIRE  
**Dispositivos Involucrados:**
1. **Cliente:** ESP32-S3 (JC3248W535 - 320x480 QSPI ST7796 / GT911) corriendo CBDos v0.2.1 (LVGL 9.5).
2. **Dongle / Puente Radio:** ESP32-C3 USB Bridge a 115200 bps (`/dev/ttyACM0`).
3. **Gateway-Router:** Servidor Python (`gateway_router.py` + `tlvgl_server.py` + `pseudo_arp.py`).

---

## 📋 1. Resumen de la Arquitectura Validada

Se ha demostrado con éxito la pila completa de navegación y enrutamiento por malla en tiempo real:

```
 ┌──────────────────────────────────────────────────────────────────┐
 │                     ESP32-S3 (Cliente CBDos)                     │
 │ • IP Mesh RFC 1918 (4B): 10.MAC[3].MAC[4].MAC[5]                 │
 │ • Short ID Candidato (2B): 0xMAC[4]MAC[5]                        │
 │ • Navegador: TlvBrowserView (LVGL 9.5)                           │
 └────────────────────────────────┬─────────────────────────────────┘
                                  │
      (Aire ESP-NOW: Canal 1 | Petición Micro-Chunk 1/1 de 18 Bytes)
                                  │
                                  ▼
 ┌──────────────────────────────────────────────────────────────────┐
 │                DONGLE USB ESP-NOW (ESP32-C3 Bridge)              │
 │ • Enmarcado HDLC/CRC8 hacia PC sobre USB CDC                     │
 └────────────────────────────────┬─────────────────────────────────┘
                                  │
                                  ▼
 ┌──────────────────────────────────────────────────────────────────┐
 │             GATEWAY-ROUTER FRONTAL (gateway_router.py)           │
 │                                                                  │
 │  1. Tabla Pseudo-ARP / Pseudo-NAT (clients.json):                │
 │     Mapeo: IPv4 (4B) <---> Short ID (2B) <---> MAC (6B) <---> ACL│
 │  2. Enrutamiento por Service ID:                                 │
 │     • ServiceId::TlvglRequest (0x07)                             │
 │     • ServiceId::RoutingControl (0x0F)                           │
 │     • ServiceId::WebProxy (0x05)                                 │
 │  3. Compilador TLVGL:                                            │
 │     HTML Dinámico ──> Bytecode Binario Ultra-Denso (~326B - 370B)│
 └────────────────────────────────┬─────────────────────────────────┘
                                  │
      (Aire ESP-NOW: Respuesta Micro-Chunk Fragmentada de 326B - 370B)
                                  │
                                  ▼
 ┌──────────────────────────────────────────────────────────────────┐
 │                     ESP32-S3 (Renderizado)                       │
 │ • Reensamblado en MeshEngine mediante Bitmask                    │
 │ • Despacho asíncrono con lv_async_call()                         │
 │ • Renderizado instantáneo en pantalla con LVGL 9.5               │
 └──────────────────────────────────────────────────────────────────┘
```

---

## 🔬 2. Registro de Tráfico Real en Consola (Captura de Producción)

```text
📻 [Dongle ESP-NOW] Conectado exitosamente en /dev/ttyACM0 @ 115200 bps
🚀 Gateway-Router TLVGL activo en TCP puerto 8080 | 🟢 MODO DESARROLLO (Telemetría Detallada)
📻 Puente Dongle USB ESP-NOW activo en /dev/ttyACM0
📁 Directorio de contenido: /home/kaber420/Documentos/proyectos/cbdos/tools/tlvgl_gateway/content

📻 [Dongle ESP-NOW] Trama recibida (chunk 1/1, 18B)
🌐 [Router -> TLVGL] Nodo [0x0001] pide: 'bento.mesh'
📻 [Dongle ESP-NOW] Emitiendo respuesta (370B) por radio...

📻 [Dongle ESP-NOW] Trama recibida (chunk 1/1, 18B)
🌐 [Router -> TLVGL] Nodo [0x0001] pide: 'clima.mesh'
📻 [Dongle ESP-NOW] Emitiendo respuesta (326B) por radio...

📻 [Dongle ESP-NOW] Trama recibida (chunk 1/1, 18B)
🌐 [Router -> TLVGL] Nodo [0x0001] pide: 'bento.mesh'
📻 [Dongle ESP-NOW] Emitiendo respuesta (370B) por radio...
```

---

## 🛠️ 3. Módulos Implementados y su Función

| Módulo | Ruta | Propósito |
| :--- | :--- | :--- |
| **Tabla Pseudo-ARP** | [`tools/tlvgl_gateway/pseudo_arp.py`](file:///home/kaber420/Documentos/proyectos/cbdos/tools/tlvgl_gateway/pseudo_arp.py) | Deriva `10.MAC[3].MAC[4].MAC[5]`, asigna Short ID con DAD, persiste en `clients.json` y controla ACL. |
| **Gateway-Router** | [`tools/tlvgl_gateway/gateway_router.py`](file:///home/kaber420/Documentos/proyectos/cbdos/tools/tlvgl_gateway/gateway_router.py) | Enrutador frontal por `ServiceId`, transcodificador Proxy Web y generador de telemetría. |
| **Servidor Dual** | [`tools/tlvgl_gateway/tlvgl_server.py`](file:///home/kaber420/Documentos/proyectos/cbdos/tools/tlvgl_gateway/tlvgl_server.py) | Servidor concurrente TCP (Wi-Fi) + Serial (Dongle ESP-NOW) con soporte de `gateway.conf` y `--debug`. |
| **Configuración** | [`tools/tlvgl_gateway/gateway.conf`](file:///home/kaber420/Documentos/proyectos/cbdos/tools/tlvgl_gateway/gateway.conf) | Parámetros del gateway (`PORT`, `SERIAL_PORT`, `DEBUG=true`, `CONTENT_DIR`). |
| **Firmware S3** | [`core/src/mesh/MeshEngine.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/mesh/MeshEngine.cpp) | Auto-cálculo de IP `10.x.y.z` y Short ID desde eFuses al iniciar la radio. |

---

## 🎯 4. Conclusiones y Próximos Hitos

1. **Rendimiento Comprobado:** Las páginas completas de UI (`clima.mesh` y `bento.mesh`) pesan entre 326 y 370 Bytes gracias a la compresión semántica TLVGL y se transmiten por el aire en milisegundos.
2. **Identidad Sólida:** El esquema `10.MAC[3].MAC[4].MAC[5]` elimina la necesidad de DHCP y permite interoperabilidad directa con OSPF y BGP en routers Linux/FreeBSD.
3. **Siguiente Paso:** Implementación de la persistencia de Wallpapers en partición Flash SPIFFS interna del S3/P4 sin depender de MicroSD.
