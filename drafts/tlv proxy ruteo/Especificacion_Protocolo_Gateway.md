# ESPECIFICACIÓN: Protocolo TLV ESP32 ↔ Gateway (Proxy Web + Servidor TLVGL)

**Fecha:** 2026-08-11
**Proyecto:** espOS32 / pseudohtml
**Objetivo:** Definir el protocolo binario único que conecta el firmware del ESP32 con los dos servidores gateway (proxy web y servidor de contenido TLVGL). **El ESP32 y el gateway hablan TLV y punto.** El HTTP/HTTPS solo existe internamente en el proxy, hacia internet; nunca en el lado del ESP32.

## ÍNDICE

- **§1** — Principio fundamental (el gateway es un router ABR/ASBR)
- **§1.1** — Agnostiidad de transporte (WiFi/TCP-IP vs LoRa/MeshHeader)
- **§2** — Trama uplink (ESP32 → Gateway)
- **§3** — Trama downlink (Gateway → ESP32)
- **§3.3** — Control byte híbrido de 3 niveles (prioridad: ruteo → útil → futuro)
- **§3.4** — TLV y MeshHeader: el mismo patrón de 3 niveles en capas distintas
- **§4 / §4.1** — Handler del gateway / funciones del nodo router
- **§5** — Generación de la página TLV por servidor (TLVGL + proxy web)
- **§6** — Capa de red en el gateway (Python)
- **§6.1** — Interfaces de ruteo: radio (local) vs trunk WiFi/VPN (externo)
- **§6.2** — Tabla de clientes locales (equivalente a tabla ARP)
- **§6.3** — Esquema dual de identidad (UUID 4B + Short ID 2B)
- **§7** — Conflictos detectados en la especificación
- **§8 / §8.1** — Plan de implementación / estado de pruebas
- **§9** — Verificación (sin hardware)
- **§10** — Preguntas abiertas

---

## 1. PRINCIPIO FUNDAMENTAL

```
┌──────────────┐   TCP (WiFi) o radio    ┌─────────────────────────────────────┐
│   ESP32-S3   │ ◀─────────────────────▶ │      GATEWAY = NODO ROUTER (ABR/ASBR) │
│   (cliente)  │    MeshHeader + TLV     │          de la red mesh              │
│              │                         │                                     │
│  tlv_parser  │                         │  Función PRINCIPAL del gateway:      │
│ TlvNetwork   │                         │  ├─ Ruteo mesh (Pseudo-BGP/OSPF)     │
└──────────────┘                         │  ├─ Puente a otras redes: VPN,       │
                                         │  │   enlaces de ruteo (links OSPF)   │
                                         │  └─ Salida a internet como proxy     │
                                         │                                     │
                                         │  Servicios EXTRA (pueden ser otro    │
                                         │  servidor que rutea con MeshHeader):│
                                         │  ├─ Proxy web (BiDi → TLV)          │
                                         │  └─ Contenido propio .tlvgl          │
                                         └─────────────────────────────────────┘
```

- **El MeshHeader lo gestiona el gateway como router (ABR/ASBR)** — es la capa de red de la mesh (`plan_red_mesh_routing.md`): lee el Control byte, decide el servicio y entrega o reenvía. El router **no sabe qué es TLVGL**, solo ve el servicio (`0x07 REQ` / `0x08 RESP`) y despacha.
- **Función principal del gateway = ruteo:** Pseudo-BGP entre ASN y Pseudo-OSPF entre torres/zonas, además de **puente a otras redes por VPN o enlaces de ruteo**. La salida a internet como proxy es una de sus capacidades.
- **Los servidores TLVGL son EXTRA:** pueden vivir en el mismo nodo o en **otro servidor** de la red que rutea con MeshHeader; el ESP32 los alcanza igual porque todo habla el mismo protocolo binario.
- **Hacia el ESP32:** SOLO tramas binarias `MeshHeader + TLV`. No hay `GET`, no hay líneas ASCII, no hay HTML de ningún tipo. Ese concepto no existe en este proyecto.
- **Dentro del nodo:** el HTTP/HTTPS real (Playwright/BiDi) queda confinado a los `renderer*.py`; el proxy es una de las funciones del nodo, no un servidor aislado.
- **El firmware no cambia:** ya habla `MeshHeader + TLV` (`TlvNetworkClient.cpp`). El gateway-router ya habla ese mismo lenguaje por diseño.

---

## 1.1. AGNOSTICIDAD DE TRANSPORTE

El **payload TLV es el mismo** sin importar cómo viaje. La capa de transporte decide el envoltorio:

| Transporte | Envoltorio de la trama | Cabecera | Servicio |
|:---|:---|:---|:---|
| **WiFi / TCP-IP** | HTTP `GET` o TCP puro | Cabecera IP/TCP estándar | URL/etiqueta del servicio |
| **LoRa / radio mesh** | `MeshHeader + TLV` | Control byte + Short ID (3-21B) | Control byte (`0x07`/`0x08`) |

- **El ESP32 y el gateway pueden comunicarse por HTTP y TCP-IP vía WiFi**, o por LoRa usando nuestro `MeshHeader+TLV`. El contenido (nodos TLV) no cambia.
- El `tlvgl_server.py` ya soporta el modo TCP-IP (y su protocolo `GET /archivo W= H=` es una forma de HTTP simplificado sobre TCP); el modo radio usa la capa de red MeshHeader.
- El ESP32 decide según su configuración de transporte activo (WiFi o LoRa), pero siempre produce el mismo payload TLV.

---

## 2. TRAMA UPLINK (ESP32 → Gateway)

### 2.1. Cabecera MeshHeader — nivel local ultra-ligero (3 bytes)

```
┌─────────┬──────────────┐
│ Control │ Short ID dst │
│   1B    │     2B       │
└─────────┴──────────────┘
```

| Campo | Valor | Nota |
|-------|-------|------|
| `Control` | `0x0F` | `MESH_CTRL_DST_ONLY` (0x08) + `MESH_SVC_TLVGL_REQUEST` (0x07) |
| `dst_id`  | `0x00FE` | Short ID local del gateway |

Fuente: `TlvBrowserView::onUplinkFrameGenerated()` en `firmware/src/UI/Views/TlvBrowserView.cpp:73`.

### 2.2. Payload TLV uplink (eventos del usuario)

Formato de cada nodo: `[Tag:1B][Len:2B BE][Value...]`. **Sin** magic `PH`.

| Tag | Nombre | Value | Origen firmware |
|:---:|:---|:---|:---|
| `0x01` | `REQ_URL` | URL en bytes (navegación) | `tlv_build_req_url` (`tlv_parser.c:36`) |
| `0x21` | `REQ_LINK_CLICK` | `[link_id:1B]` | `tlv_build_req_click` (`tlv_parser.c:62`) |
| `0x20` | `REQ_INPUT_SUBMIT` | `[element_id:1B][texto:NB]` | `tlv_build_req_submit` (`tlv_parser.c:48`) |

Ejemplo de trama completa que recibe el gateway:

```
0F 00 FE | 01 00 1D | http://noticias.mesh
└─mesh──┘ └─TLV──┘
```

---

## 3. TRAMA DOWNLINK (Gateway → ESP32)

### 3.1. Cabecera MeshHeader — respuesta local (3 bytes)

```
┌─────────┬──────────────┐
│ Control │ Short ID dst │
│   1B    │     2B       │
└─────────┴──────────────┘
```

| Campo | Valor | Nota |
|-------|-------|------|
| `Control` | `0x08` | `MESH_CTRL_DST_ONLY` (0x08) + `MESH_SVC_TLVGL_RESPONSE` (0x08) |
| `dst_id`  | `0x0001` | Short ID del ESP32 (o el `src` que venga en la petición) |

El ESP32 filtra por `service = hdr.control & 0x3F == 0x08` (`TlvBrowserView.cpp:95`).

### 3.2. Payload TLV downlink (la página)

El payload **DEBE** empezar con el magic `PH` (`0x50 0x48`), seguido de nodos TLV:

| Tag | Widget LVGL | Value | Fuente |
|:---:|:---|:---|:---|
| `0x10` | PAGE | vacío | `tlvgl_compiler.py` |
| `0x11` | TEXT | `[X:2][Y:2][W:2][H:2][style:1][texto...]` | ídem |
| `0x12` | LINK | `[X:2][Y:2][W:2][H:2][link_id:1][texto...]` | ídem |
| `0x13` | INPUT | `[X:2][Y:2][W:2][H:2][action\0name\0placeholder...]` | ídem |
| `0x16` | SWITCH | `[id:1][estado:1]` | `tlv_parser.c` |
| `0x17` | SLIDER | `[id:1][min:2][max:2][val:2]` | ídem |
| `0x18` | PROGRESS | `[min:2][max:2][val:2]` | ídem |
| `0x19` | DROPDOWN | `[id:1][opciones\n...]` | ídem |
| `0xFE` | END | vacío (cierre) | ídem |

---

## 3.3. BYTE DE CONTROL HÍBRIDO DE 3 NIVELES (propuesta)

Siguiendo el **Esquema de Tags Híbridos** de `pseudohtml_tags_plan.md` §1 (similar a UTF-8), el Control byte se expande en 3 niveles sin desperdiciar ancho de banda en las tramas comunes:

### Nivel 1 — Compacto (1 Byte): `0x00`–`0xFE`

El control normal que ya existe. **Prioridad máxima al ruteo y a lo crítico/común** que necesita velocidad y despacho inmediato. Bits:

```
bit 7  GLOBAL_BIT   → inter-ASN (ruteo, despacho máx.)
bit 6  SRC_SHORT    → el ORIGEN es Short ID de 2B        ┐ dos bits de ID,
bit 5  INTRA_ZONE   → OSPF intra-zona (ruteo)            ├ por PUNTA, para
bit 4  DST_SHORT    → el DESTINO es Short ID de 2B       ┘ expresar los 4 modos
bit 3  DST_ONLY     → ultra-ligera local 3B
bits 2-0  servicio  → TLVGL, chat, proxy (8 servicios base)
```

Los dos bits de ID (`SRC_SHORT` bit6 / `DST_SHORT` bit4) indican **cada extremo por separado** si es UUID (4B) o Short ID (2B). Es exactamente el pedido de "estados que indiquen si ambos son UUIDs o IDs cortos":

| SRC_SHORT | DST_SHORT | identidad                | cabecera GLOBAL | cabecera INTRA |
|:---:|:---:|:---|:---:|:---:|
| 0 | 0 | src UUID 4B + dst UUID 4B | 21B | 13B |
| 0 | 1 | src UUID 4B + dst SHORT 2B | 19B | 11B |
| 1 | 0 | src SHORT 2B + dst UUID 4B | 19B | 11B |
| 1 | 1 | src SHORT 2B + dst SHORT 2B | **17B** | 9B |

- El modo 11 (ambos Short) es el "arranque" de identidad: cada nodo se representa como **ASN+Zona+Torre (6B) + Short ID (2B) = 8B**, y la cabecera global queda **17B** (src 8B + dst 8B + control) — en vez de 21B. Ida de bytes que se pierde en radio, admisible en el trunk WiFi.
- `SIGNAL_BIT` (señalización) **ya no está en Nivel 1**: pasa al Nivel 2 porque no requiere despacho a máxima velocidad.
- Sirve para el 99% del tráfico: TLVGL (`0x07`/`0x08`), chat (`0x01`), proxy local (`0x05`).

### Nivel 2 — Extendido (2 Bytes): `0xFF` + Control Extendido

Lo **útil y frecuente** que no cupo en el Nivel 1:

```
┌─────────┬─────────────────────┐
│  0xFF   │ Control extendido   │
│  1B     │     1B              │
└─────────┴─────────────────────┘
```

- Señalización OSPF/BGP, VPN/puentes, telemetría mesh, etc.
- Hasta **256** controles extendidos.

### Nivel 3 — Ultra-Extendido (3 Bytes): `0xFF 0xFF` + Control

**Reservado para futuras necesidades**, no para lo común:

```
┌─────────┬─────────┬─────────────────────┐
│  0xFF   │  0xFF   │ Control ultra       │
│  1B     │  1B     │     1B              │
└─────────┴─────────┴─────────────────────┘
```

- Servicios ultra-específicos: diccionarios masivos, futuros protocolos.
- Hasta **65,536** controles ultra.

### Regla de parseo (crítica)

**El `0xFF` se debe chequear ANTES que cualquier flag.** El parser actual (`mesh_header.c`) interpretaría `0xFF` como cabecera global (todos los bits en 1) y lo rompería.

```
if (control == 0xFF):
    if (siguiente_byte == 0xFF):  → nivel 3 (3 bytes)
    else:                          → nivel 2 (2 bytes)
else:
    parseo normal nivel 1 (flags + servicio)
```

### 3.4 TLV y MeshHeader: el MISMO patrón de 3 niveles, en capas DISTINTAS

Ambos protocolos usan un esquema híbrido de 3 niveles con `0xFF` como marcador de extensión, pero **no son el mismo campo ni se interpretan entre sí** — es el mismo patrón de compresión reutilizado en dos capas distintas del stack:

| | **TLV (la Tag)** | **MeshHeader (el Control byte)** |
|:---|:---|:---|
| ¿Qué es híbrido? | La **Tag** de dimensionamiento de datos | El **Control byte** de la cabecera de red |
| Nivel 1 | tag 1B (`0x00`–`0xFE`) | control 1B (§3.3) |
| Nivel 2 | `0xFF` + 1B (tag extendido) | `0xFF` + control extendido |
| Nivel 3 | `0xFF 0xFF` + 1B (tag rara) | `0xFF 0xFF` + control ultra |
| Referencia | `pseudohtml_tags_plan.md` §5.1 | esta especificación §3.3 |
| Semántica | Qué **tipo de dato** es (texto, imagen, link) | Qué **ruta/servicio** sigue el paquete |
| Estado código | en diseño (compilador TLV Python) | implementado en `gateway_router.py`; en firmware `mesh_header.c` aún NO |

- El TLV es la **capa de contenido/datos** (dimensiona tags como URL, texto, página). El MeshHeader es la **capa de red/ruteo** (Control byte). Cada capa resuelve su propia ala.
- Se parecen porque comparten la filosofía `0xFF` = "next bytes son extensión" (similar a UTF-8), ahorrando bytes en los casos comunes.
- Requisito de implementación: **no confundirlos**. El router consume el Control híbrido del MeshHeader; el compilador TLV consume el tag híbrido. Ambos chequearán `0xFF` ANTES de interpretar, pero en campos diferentes.

### Estado actual (discrepancia detectada)

- El `plan_red_mesh_routing.md` define: `bit 7 local/global, bit 6 señalización, bits 5-0 servicio`.
- El código real (`mesh_header.h`): 5 flags en bits 3-7, servicio en bits 2-0 (máx **8 servicios**).
- El comentario del header dice "bits 4..0" pero `TlvBrowserView.cpp:95` usa `& 0x3F` (bits 5-0). **El plan y el código no coinciden; hay que unificarlos al implementar el híbrido.**
- **Layout adoptado en el router** (`gateway_router.py`): el diseño de arriba (bits 7/6/5/4/3 + servicio), con codigo que parsea los 3 niveles y los 4 modos de ID. El Nivel 1 prioriza ruteo; ID por punta; `SIGNAL` en Nivel 2.

---

## 4. HANDLER DEL GATEWAY (flujo por conexión)

```
1. Recibe la trama (hasta 1024B).
2. Capa de red (router): lee byte 0 = Control.
   - service = Control & 0x3F  →  identifica la función del nodo.
   - cabecera: si (Control & 0x08) → 3B local; si no → 9/13/21B según flags (mesh_header.c).
3. Dispatch por servicio (el nodo router decide qué función atiende):
   ├─ 0x07 TLVGL_REQUEST → función contenido TLVGL o proxy web (según URL).
   ├─ 0x01 chat, 0x05 proxy legacy → funciones de ruteo/mensajería (futuro).
   └─ Otro dst/ASN → el router REENVÍA la trama sin abrir el payload (opaco).
4. Función atendida: procesa el primer nodo TLV uplink:
   ├─ 0x01 REQ_URL      →  página pedida = url.
   ├─ 0x21 LINK_CLICK   →  resolver link_id contra el mapa de la página servida (link_id → url/acción).
   └─ 0x20 INPUT_SUBMIT →  ejecutar submit (formulario) y derivar a la siguiente página.
5. Genera la página TLV (ver §5).
6. Responde: [Control=0x08][dst 2B] + b"PH" + nodos TLV.
```

## 4.1. FUNCIONES DEL NODO ROUTER (ABR / ASBR)

El gateway es **principalmente un router**. Su prioridad no es servir páginas, sino ruteo y conectividad.

| Función | Descripción | ¿Principal? |
|:---|:---|:---|
| **Ruteo mesh** | Parsear/reenviar MeshHeader, mantener tabla ASN→next-hop (Pseudo-BGP) y mapa de torres/zonas (Pseudo-OSPF). El payload TLV es opaco para el ruteo. | ✅ Principal |
| **Puente a otras redes** | Por **VPN** o **enlaces de ruteo (links OSPF)**: re-empaqueta/reenvía tramas hacia otros ASN/redes sin abrir el payload. | ✅ Principal |
| **Salida a internet (proxy)** | Termina la trama local, pide la URL con Playwright/BiDi y devuelve el resultado transcodiado a TLV. | ✅ Principal |
| **Contenido propio** | Sirve `.tlvgl` propios desde `content/` (sin internet). | ⭐ Extra |
| **Proxy web (BiDi)** | Trae páginas reales de internet → TLV. | ⭐ Extra |

> Los servicios TLVGL (contenido propio + proxy web) son **extra**: pueden correr en el mismo nodo o en **otro servidor** de la red que también rutea con MeshHeader. El ESP32 los alcanza de forma transparente.

---

## 5. GENERACIÓN DE LA PÁGINA TLV POR SERVIDOR

### 5.1. Servidor de contenido TLVGL (`tlvgl_server.py`)

Sirve páginas **propias** desde `content/`. No usa internet.

| Evento uplink | Comportamiento |
|:---|:---|
| `0x01 REQ_URL` `"http://galeria.mesh"` | resolver la URL a un `.tlvgl` de `content/` y servirlo |
| `0x21 LINK_CLICK` id=N | resolver mapa link_id→URL interna → servir esa página |
| `0x20 INPUT_SUBMIT` | redirección/acción interna si aplica |

- Resolución por defecto: 480×640 (o la que negocie el ESP32, ver §9).
- Cache LRU por `(página, W, H)` (ya implementado).

### 5.2. Proxy web (`server/main.py`)

Trae páginas reales de internet y las transcode a TLV.

| Evento uplink | Comportamiento |
|:---|:---|
| `0x01 REQ_URL` | `render_page(url, w, h)` → elementos → TLV |
| `0x21 LINK_CLICK` id=N | link_id → href real → `render_page(href)` |
| `0x20 INPUT_SUBMIT` | rellenar form con el texto y navegar |

- El motor (Playwright `renderer.py` o BiDi raw `renderer_raw.py`) extrae elementos visibles con sus bounding boxes; `encoder.py` los convierte a TLV downlink.
- El HTTP/HTTPS queda **confinado** a `renderer*.py`. La interfaz con el ESP32 es TLV puro.

---

## 6. CAPA DE RED EN EL GATEWAY (Python)

Implementación Python de la capa de red del router (`gateway_router.py`, clase `GatewayRouter`), compartida por las funciones del nodo. Es la contraparte de `mesh_header.c` del firmware, **TLV puro**, sin ASCII.

```python
def parse_header(data): ...        # capa de red: (offset, control, service, dst)
def route_or_forward(parsed): ...  # devuelve deliver|forward|drop (BGP/OSPF/ARP)
def _route_ospf(parsed): ...
def forward(data): ...             # decide y entrega/reenvía (router)
def _tx(peer, data): ...           # reenvía la trama cruda a un peer (sin abrir payload)
```

Reglas de la capa de red:
- El Control byte es **híbrido de 3 niveles** (§3.3): se chequea `0xFF` ANTES de interpretar cualquier flag.
- Cabeceras por modos de ID (§3.3): 3B (local), 9/11/13B (intra-zona OSPF), 17/19/21B (global inter-ASN).
- `service = control & 0x0F` (bits 0-3).
- El payload TLV es **opaco** para el ruteo: solo se abre si el paquete termina en este nodo (servicio `0x07`/`0x08`). Si el destino es otro ASN, se reenvía **sin tocar** el payload (`_tx`).

### 6.1. INTERFACES DE RUTEO: RADIO (LOCAL) vs TRUNK WIFI/VPN (EXTERNO)

Cada ruta (BGP/OSPF) apunta a un **nombre de interfaz**, y cada interfaz define su enlace físico y su cabecera. Es la táctica central del diseño:

| Interfaz | Enlace | Velocidad | Cabecera | Uso |
|:---|:---|:---|:---|:---|
| `radio-local` | radio 2.4G FLRC/LoRa | cientos de kbps | Short ID 2B (3B/9B) | entrega a los clientes locales y torre propia |
| `trunk-*` | WiFi (decenas/cientos de Mbps) o VPN | Mbps | cabecera completa (13B/21B) | enlace troncal a futuras zonas/ASN |

**Principio:** *lo corto para la radio (ahorra aire), lo largo para el trunk (el ancho de banda sobra).* El ID corto de 2B se usa en lo LOCAL; hacia el exterior se envían los bytes completos (ASN+Zona+Torre + Short). `INTERFACES` define los peers físicos reales:

```python
INTERFACES = {
    "radio-local": {"type": "radio", "peer": ...},
    "trunk-a":     {"type": "wifi",  "peer": ("192.168.1.50", 8765)},
    "trunk-b":     {"type": "wifi",  "peer": ("192.168.1.60", 8765)},
    "trunk-c":     {"type": "vpn",   "peer": ("203.0.113.20", 8765)},
}
```

`radio-local` es la interfaz de los clientes y la torre propia; `trunk-*` son los enlaces de alta velocidad a otras zonas/ASN.

### 6.2. TABLA DE CLIENTES LOCALES (equivalente a la tabla ARP)

El router necesita saber a qué cliente local corresponde cada ID, igual que un router Ethernet usa la tabla ARP de MAC. `CLIENT_TABLE` mapea **Short ID (2B)** y **UUID (4B)** → interfaz + peer real:

```python
CLIENT_TABLE = {
    0x0001: {"short": 0x0001, "uuid": 0x00000001, "iface": "radio-local", "peer": (...)},
    0x1234: {"short": 0x1234, "uuid": 0x00001234, "iface": "radio-local", "peer": (...)},
}
```

- Un cliente puede ser visto por Short ID (sesión activa) o por UUID (descubrimiento / Fase 1→2 de registrarse).
- Lookups: `_client_by_uuid()`, `_client_by_short()`.
- **Regla de entrega:** si la trama llega con destino a un cliente local, se busca en la tabla ARP y se reenvía por `_tx()` al `peer` de ese cliente. Si no está, se re-difunde por la radio local.
- ¿Por qué a la torre le basta el Short ID de 2B? Porque ya registró al nodo (le asignó un lease/Short ID en el handshake). Es una optimización de aire: 2B vs 4B de UUID.
- **Escalado:** en la mesh, los routers de la misma zona comparten la tabla de Short IDs; los de distinto ASN solo se conocen por ASN+Torre, no por cliente.

### 6.3. ESQUEMA DUAL DE IDENTIDAD (UUID 4B + SHORT 2B)

El sistema **evoluciona** y soporta ambos tipos a la vez (§3.3, modo por punta):

- **UUID (4B):** identidad permanente del nodo, usada en descubrimiento y para nodos no registrados. Cabeceras 13B/21B.
- **Short ID (2B):** identidad comprimida que asigna la torre al registrar al nodo. Cabeceras 9B/17B. Se da de baja/alta de la tabla ARP.

Un nodo es identificable tanto por su UUID (4B) como por su Short ID (2B); la sesión promueve de UUID a Short ID una vez establecida, y la tabla ARP guarda ambos.

---

## 7. CONFLICTOS DETECTADOS EN LA ESPECIFICACIÓN ACTUAL

1. **Tag `0x15` colisiona:** el compilador (`tlvgl_compiler.py`) usa `0x15 = TYPE_ABS_PANEL`, pero el firmware (`tlv_parser.h`) usa `0x15 = TYPE_ABS_CHECKBOX`. Un ESP32 interpretaría cada `PANEL` como `CHECKBOX`. → **Resolver: renombrar PANEL a otro tag o eliminarlo.**
2. **Magic `PH` ausente en la salida del nodo:** el firmware lo tolera como opcional (`tlv_parser.c:79`), pero la especificación de `pseudohtml_tags_plan.md` exige `PH` al inicio del downlink. Las funciones del nodo no lo emiten. → **Deben añadirlo.**
3. **Implementaciones actuales incompletas:** `tlvgl_server.py` y `server/main.py` esperan una línea `GET ...` (protocolo viejo), no implementan aún la capa de red del router (MeshHeader). Deben usar la capa de red según §2-§4.
4. **Respuesta sin MeshHeader:** hoy responden `[4B size]+TLV`; el firmware espera `MeshHeader + TLV`. → Resuelto en §3-§4.
5. **Falta negociación de resolución:** en modo mesh el firmware no manda `W=`/`H=`. → Usar MAX (480×640) o añadir un nodo TLV de resolución en el handshake (pendiente de definir).
6. **Discrepancia del Control byte:** el plan (`plan_red_mesh_routing.md`) define servicio en bits 5-0; el código real usa 5 flags (bits 3-7) y servicio en bits 2-0. Inconsistente también entre el comentario del header ("bits 4..0") y el uso real (`& 0x3F`). → Unificar e implementar híbrido 3 niveles (§3.3).

---

## 8. PLAN DE IMPLEMENTACIÓN

| # | Tarea | Dónde | Prioridad |
|:--|:---|:---|:---|
| 1 | Crear `mesh_proto.py` (capa de red del router, Python) | `pseudohtml/gateway/` | Alta |
| 2 | `tlvgl_server.py`: integrar capa de red (MeshHeader) + `PH` | ídem | Alta |
| 3 | `server/main.py`: ídem, HTTP solo dentro de `renderer*.py` | ídem | Alta |
| 4 | Resolver conflicto tag `0x15` (PANEL vs CHECKBOX) | ambos lados | Media |
| 5 | Unificar Control byte (plan vs código) + implementar híbrido de 3 niveles (§3.3) | firmware + gateway | Media |
| 6 | Negociación de resolución ESP32→gateway | firmware | Baja |

**Criterio de aceptación:** el ESP32 flasheado navega por una página propia (función contenido TLVGL) y por una página real de internet (función proxy web) **sin modificar una sola línea del firmware**, solo apuntando la IP en `TlvNetworkClient.cpp:7`.

---

## 8.1. ESTADO DE PRUEBAS (2026-08-11) — verificadas hoy

| # | Prueba | Resultado |
|:--|:---|:---|
| 1 | `tlvgl_server.py` escucha TCP 8765 | ✅ Conecta y responde |
| 2 | `GET /index.html W=240 H=320\r\n` (modo legado) | ✅ 186 bytes TLV válidos: PAGE + 4 TEXT + 2 LINK |
| 3 | `tlvgl_server.py` contra trama binaria `MeshHeader+TLV` del firmware | ❌ Timeout: espera `\n` ASCII, no parsea Control byte |
| 4 | `gateway_prototype.py` (mock, MeshHeader+TLV) contra trama del firmware | ✅ 152 bytes: `Control=0x08`, magic `PH`, tag PAGE |
| 5 | Unit tests `test_server.py` (5 casos) | ✅ Todos OK (0.023s) |
| 6 | Decodificación del TLV de `content/index.html` | ✅ PAGE/TEXT/LINK con coords correctas (240×320) |
| 7 | `gateway_router.py` parsea los 3 niveles de Control + 4 modos de ID | ✅ Cabeceras 3B/9B/17B/19B/21B según bits |
| 8 | `gateway_router.py` ruteo: global→ASN 0x42 sale por `trunk-a` (wifi) | ✅ forward trunk con peer |
| 9 | `gateway_router.py` OSPF: torre vecina 0xA3 → trunk wifi | ✅ forward trunk |
| 10 | `gateway_router.py` torre local / uuid propio | ✅ deliver |
| 11 | `gateway_router.py` DST_ONLY 3B a Short 0x0001 → tabla ARP → peer ESP32 | ✅ forward por `_tx` a `radio-local` |

**Conclusión:** el **protocolo MeshHeader+TLV ya funciona** de extremo a extremo con `gateway_prototype.py` (mismo framing que el firmware), y **el router `gateway_router.py` ya decide ruteo correctamente** (BGP/OSPF/ARP local) con el Control de 3 niveles y los 4 modos de ID. Lo que falta es llevar esa capa de red al `tlvgl_server.py`/`server/main.py` reales (tarea 1-3 de §8). El prototipo valida que el diseño es correcto.

---

## 9. VERIFICACIÓN (sin hardware)

```bash
# 1. Simular la trama exacta del ESP32 contra el gateway real
python3 - <<'EOF'
import socket
# 0F 00 FE | 01 00 1D http://noticias.mesh
frame = bytes([0x0F,0x00,0xFE, 0x01,0x00,0x1D]) + b"http://noticias.mesh"
s = socket.create_connection(("127.0.0.1", 8765), timeout=5)
s.sendall(frame)
resp = s.recv(4096)
s.close()
assert resp[0] & 0x3F == 0x08, "service debe ser TLVGL_RESPONSE"
assert resp[3:5] == b"PH", "debe llevar magic PH"
print("OK", len(resp), "bytes")
EOF
```

---

## 10. PREGUNTAS ABIERTAS

- [ ] ¿Puerto único del proxy web? (8765 para ambos, o mantener 8080 y configurar el firmware)
- [ ] ¿Resolución negociada por TLV o fija a 480×640?
- [ ] ¿`0x15` se asigna a PANEL (compilador) o a CHECKBOX (firmware)? ¿Y SWITCH/SLIDER dónde?
- [ ] ¿El Short ID del ESP32 (`0x0001`) se fija estático o se asigna en handshake (según `plan_red_mesh_routing.md` Fase 1→2)?
