# TLVGL: Formato de Contenido Agnóstico al Transporte

## Principio Central

**TLVGL es solo un formato de payload. No le importa cómo viajan los bytes.**

El compilador produce bytes TLV que describen una interfaz LVGL:
```
[TYPE 1B][LEN 2B][X 2B][Y 2B][W 2B][H 2B][datos...]
```
Esos bytes pueden viajar por cualquier canal. El ESP32 los recibe, los parsea con `tlv_parser.cpp` y llama a la API de LVGL — sin saber ni importarle qué radio los transportó.

---

## Capas del Sistema

```
┌─────────────────────────────────────────────┐
│            Capa de Contenido                │
│                                             │
│  HTML  →  [tlvgl_compiler.py]  →  bytes    │
│  bytes →  [tlvgl_server.py]    →  cliente  │
│                                             │
│  Resolución negociada en la petición:       │
│  GET /index.html W=240 H=320\r\n            │
└──────────────────┬──────────────────────────┘
                   │  los mismos bytes, cualquier canal
        ┌──────────┴───────────────────────────────┐
        │          Capa de Transporte               │
        │  (el servidor TLVGL no sabe cuál es)     │
        └───────┬───────────┬────────────┬─────────┘
                │           │            │
           Radio 2.4GHz   LoRa       WiFi / IP
           (SX1280)      sub-GHz
```

---

## Transportes Soportados

El servidor TLVGL no tiene código específico para ningún transporte. Cualquier capa que pueda entregar los bytes al ESP32 funciona:

### Radio 2.4 GHz — SX1280

| Modo | Velocidad | Notas |
|---|---|---|
| **FLRC** | hasta 1.3 Mbps | Mínima latencia, ideal para interfaces gráficas. Modo preferido para el proyecto Alternet |
| **LoRa 2.4GHz** | ~200 kbps | Más alcance que FLRC, misma banda |
| **GFSK/BLE** | variable | Fallback alternativo |

### Radio sub-GHz — LoRa

| Modo | Velocidad | Notas |
|---|---|---|
| **LoRa raw** (SX127x) | ~250 kbps | Alcance kilométrico, latencia mayor |
| **MeshCore** | idem LoRa | Protocolo mesh sobre LoRa (compatible si alguien quiere desplegarlo sobre MeshCore) |
| **Meshtastic** | idem LoRa | Otra opción mesh sobre LoRa, comunidad activa |

### WiFi / ESP (sin hardware adicional)

| Modo | Velocidad | Notas |
|---|---|---|
| **ESP-NOW** | ~1 Mbps | Sin router, peer-to-peer, latencia mínima. Nativo en todos los ESP32 |
| **ESP-NOW LR** | ~250 kbps | Modo 802.11b Long Range. Mayor alcance que ESP-NOW normal, sin hardware adicional |
| **TCP/UDP (WiFi)** | variable | Modo de desarrollo y pruebas local |

### Otros

| Modo | Notas |
|---|---|
| **UART/Serie** | Debug, cables, desarrollo |
| **BLE** | Si alguien lo necesita, las notificaciones GATT pueden tunelizar el payload |
| **Cualquier otro** | El payload son bytes puros, no hay restricción de transporte |

---

## Resolución Dinámica (Browser-Driven)

El browser ESP32 anuncia su tamaño de pantalla real en cada petición.
El servidor compila el layout para esa resolución exacta — o el máximo configurado si no viene.

```
# Protocolo de petición (ASCII, \r\n al final)
GET /index.html W=240 H=320\r\n

# Respuesta
[4 bytes big-endian: tamaño][bytes TLVGL compilados para 240×320]
```

El mismo protocolo aplica al Proxy Web existente (`gateway/server/`):
```
# Proxy para internet externo
GET http://noticias.com W=240 H=320\r\n
```

**Fallback:** si el ESP32 no manda `W=` y `H=`, el servidor usa el máximo configurado (`MAX_W=480, MAX_H=640`).

---

## Lo Implementado Hasta Ahora

| Archivo | Estado |
|---|---|
| `gateway/tlvgl/tlvgl_compiler.py` | ✅ Funcional — HTML → bytes TLV, layout engine, linter |
| `gateway/tlvgl/tlvgl_server.py` | ✅ Funcional — TCP asyncio, cache LRU, W/H dinámico |
| `gateway/tlvgl/tlvgl_preview_server.py` | ✅ Funcional — HTTP preview tool con UI web |
| `gateway/tlvgl/tlvgl_preview.html` | ✅ Funcional — 8 presets, modo LVGL, modo cajas |
| `gateway/server/main.py` | ✅ Fix — parsea W= H= del cliente |
| `gateway/server/renderer.py` | ✅ Fix — propaga viewport a Playwright |
| `src/tlv_parser.cpp` | ✅ Existente — parser C en el ESP32, compatible |

### Preview Tool

```bash
python3 gateway/tlvgl/tlvgl_preview_server.py
# Abre http://localhost:8766
```

### Servidor para ESP32 (modo prueba TCP)

```bash
python3 gateway/tlvgl/tlvgl_server.py --port 8765
```

### Compilar desde consola

```bash
python3 gateway/tlvgl/tlvgl_compiler.py mi_pagina.html --w 240 --h 320
```

---

## Pendiente

- [ ] Adaptador de transporte FLRC (SX1280) — envolver el payload con las cabeceras del `custom_mesh_protocol.md`
- [ ] Adaptador ESP-NOW / ESP-NOW LR — wrapper Python en el gateway
- [ ] Fragmentación — páginas que superen el MTU del transporte (253B en SX1280 LoRa)
- [ ] Compresión del payload (zlib, LZ4) — opcional para transportes lentos
- [ ] Seguridad OTA (ECQV + ECDH + AES-GCM hardware) — ver `plan_servidor_tlvgl.md §4`
