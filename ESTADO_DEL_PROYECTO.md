# espOS32 — Estado del Proyecto

**Fecha:** 2026-08-11
**Sistema:** Navegador TLVGL en ESP32-S3 (JC3248W535) + Router Mesh en Go (`espOS32-router`) + Servidor de Hosting Python + Red Mesh (Protocolo Unificado 3 Capas)

---

## 1. QUÉ FUNCIONA HOY (verificado en vivo)

### Router Mesh en Go (`espOS32-router`) + Servidor TLVGL (E2E Verificado) ✅

El Router en Go (`gateway/router-go/build/router`) está **implementado, compilado y funcionando en vivo en TCP :8765** y Unix Socket `/tmp/espos32_router.sock`:

```
ESP32 (192.168.66.248) / Cliente Test
   │  Trama binaria MeshHeader + TLV  (TCP 8765)
   ▼
Router Mesh en Go (gateway/router-go/build/router)  (0.0.0.0:8765)
   │  Parsea Control byte (3B DST_ONLY / 9B)
   │  Auto-registra cliente en Tabla Pseudo-ARP (NodeID 0x0001 / 0x00FE)
   │  Rutea tramas de ServiceID 0x07 hacia backend de Hosting
   ▼
Servidor de Hosting TLVGL (gateway/tlvgl/tlvgl_server.py)  (127.0.0.1:8766)
   │  Resuelve URL .mesh → compila HTML de content/ → TLV (b"PH" + TLV + 0xFE)
   ▼
Respuesta devuelta por Router Go a la conexión TCP del ESP32
   │
   ▼
ESP32: renderiza interfaz TLVGL en pantalla
```

* **Especificación Maestra Oficial:** [PROTOCOLO_UNIFICADO_MASTER.md](file:///home/kaber420/Documentos/proyectos/espOS32/drafts/tlv%20proxy%20ruteo/PROTOCOLO_UNIFICADO_MASTER.md)


Evidencia en `tlvgl_server.log`: el ESP32 (`UUID=C0A842F8` = `192.168.66.248`)
envía `REQ_URL 'http://clima.mesh'` y `LINK_CLICK id=N`, y recibe 191B TLV
por cada petición.

> **Terminología (ver `drafts/tlv proxy ruteo/GLOSARIO_Inventario_Piezas.md`):**
> - **Gateway = router** de la red mesh (Pseudo-BGP/OSPF). Lee el MeshHeader,
>   decide si entregar o reenviar. **No sirve contenido.** Aún NO está integrado.
> - **Server de hosting** (`tlvgl_server.py`) = sirve páginas `.tlvgl`. Es lo
>   que se probó hoy.

### Piezas que lo componen

| Pieza | Ruta | Rol | Estado |
|:---|:---|:---|:---|
| Servidor TLVGL (hosting) | `gateway/tlvgl/tlvgl_server.py` | Sirve páginas `.tlvgl` desde `content/`. Modo legacy ASCII `GET` + modo binario MeshHeader + invalidación por `mtime` | ✅ funciona |
| Capa de red MeshHeader (Python) | `gateway/tlvgl/mesh_proto.py` | Espejo de `mesh_header.c`: parseo 3B/9B/13B/21B, tags TLV, construcción de respuesta | ✅ funciona |
| Compilador HTML→TLV | `gateway/tlvgl/tlvgl_compiler.py` | Convierte HTML a nodos TLV (PAGE/TEXT/LINK/INPUT/PANEL/CHART) | ✅ funciona (arreglado filtro de tags sin texto) |
| Soporte de Gráficas (LVGL Charts) | `firmware/src/Core/tlv_parser.c` | Renderiza `TYPE_ABS_CHART` (`0x1B`) (líneas y barras) con auto-escalado del eje Y | ✅ funciona (probado y flasheado en vivo) |
| Editor Visual / Preview Web | `gateway/tlvgl/editor.js` & `tlvgl_preview.html` | WYSIWYG drag-and-drop con soporte para `<chart>` | ✅ funciona |
| Firmware navegador | `firmware/src/UI/Views/TlvBrowserView.cpp` | Arma trama DST_ONLY(3B)+TLV, renderiza respuesta | ✅ funciona |
| Cliente de red (firmware) | `firmware/src/Network/TlvNetworkClient.cpp` | TCP al gateway (IP hardcodeada, ver §3) | ✅ funciona |

### Tabla Maestra de Etiquetas PseudoHTML / TLVGL

| Tag Hex | Nombre PseudoHTML | Equivalente HTML | Widget LVGL | Payload (Value) Estructurado | Estado |
| :---: | :--- | :--- | :--- | :--- | :---: |
| `0x10` | **PAGE** | `<body>` | `lv_screen` | `[bg_color: 2B]` | ✅ Implementado |
| `0x11` | **TEXT** | `<h1>` .. `<p>` | `lv_label` | `[x:2][y:2][w:2][h:2][style:1][Texto]` | ✅ Implementado |
| `0x12` | **LINK** | `<a>` / `<button>` | `lv_btn` | `[x:2][y:2][w:2][h:2][link_id:1][Texto]` | ✅ Implementado |
| `0x13` | **INPUT** | `<input>` | `lv_textarea` | `[x:2][y:2][w:2][h:2][Act\0Name\0Placeholder]` | ✅ Implementado |
| `0x14` | **IMAGE** | `<img>` | `lv_image` | `[x:2][y:2][w:2][h:2][Image_Bytes]` | ✅ Implementado |
| `0x1A` | **PANEL** | `<div>` / `<panel>` | `lv_obj` (Bento) | `[x:2][y:2][w:2][h:2][bg_color:2]` | ✅ Implementado |
| `0x1B` | **CHART** | `<chart>` | `lv_chart` | `[x:2][y:2][w:2][h:2][type:1][pts:2][vals:2*N]` | ✅ Implementado |
| `0x15` | **CHECKBOX** | `<input type="checkbox">` | `lv_checkbox` | `[x:2][y:2][w:2][h:2][id:1][state:1][Texto]` | ⏳ Pendiente |
| `0x16` | **SWITCH** | `<input type="checkbox" class="toggle">` | `lv_switch` | `[x:2][y:2][w:2][h:2][id:1][state:1]` | ⏳ Pendiente |
| `0x17` | **SLIDER** | `<input type="range">` | `lv_slider` | `[x:2][y:2][w:2][h:2][id:1][min:2][max:2][val:2]` | ⏳ Pendiente |
| `0x18` | **PROGRESS** | `<progress>` | `lv_bar` | `[x:2][y:2][w:2][h:2][min:2][max:2][val:2]` | ⏳ Pendiente |
| `0x19` | **DROPDOWN** | `<select>` | `lv_dropdown` | `[x:2][y:2][w:2][h:2][id:1][Options\0]` | ⏳ Pendiente |
| `0x1C` | **ARC** | `<input type="range" class="radial">` | `lv_arc` (Perilla) | `[x:2][y:2][w:2][h:2][id:1][min:2][max:2][val:2]` | ⏳ Pendiente |
| `0x1D` | **SPINNER** | `<spinner>` | `lv_spinner` | `[x:2][y:2][w:2][h:2][spin_time:2][arc_length:2]` | ⏳ Pendiente |
| `0x1E` | **ROLLER** | `<select mode="wheel">` | `lv_roller` | `[x:2][y:2][w:2][h:2][id:1][Options\0]` | ⏳ Pendiente |
| `0x1F` | **MSGBOX** | `<dialog>` / `<modal>` | `lv_msgbox` (Modal) | `[title\0msg\0buttons\0]` | ⏳ Pendiente |
| `0x22` | **TABVIEW** | `<nav>` / `<tab>` | `lv_tabview` (Tabs) | `[tab_count:1][Titles\0]` | ⏳ Pendiente |
| `0x23` | **TILEVIEW** | `<div class="carousel">` | `lv_tileview` (Gestos) | `[rows:1][cols:1]` | ⏳ Pendiente |
| `0x24` | **ANIMIMG** | `<img src="*.gif">` | `lv_animimg` / `lv_gif` | `[x:2][y:2][w:2][h:2][frames:1][Gif_Bytes]` | ⏳ Pendiente |
| `0x25` | **SOUND** | `<audio src="beep">` | Tone / RTTTL / Beep | `[sound_id:1][rtttl_string\0]` (Ultraligero < 50B) | ⏳ Pendiente |

> **Nota Teclado (`lv_keyboard`):** El teclado en pantalla no requiere tag TLV; el firmware del ESP32 se encarga nativamente de desplegarlo al tocar cualquier campo `<input>`.

### Formato `.enc` de provisionamiento (lado firmware) ✅ implementado

`ConfigManager::importGateway()` (`firmware/src/Network/ConfigManager.cpp:235`)
ya descifra archivos de gateway cifrados:

- **Algoritmo:** AES-256-GCM + PBKDF2-HMAC-SHA256
- **Parámetros exactos:**
  - `PBKDF2_SALT_LEN = 8` bytes
  - `GCM_IV_LEN = 12` bytes
  - `GCM_TAG_LEN = 16` bytes
  - `PBKDF2_ITERATIONS = 10000`
- **Layout del archivo `.enc`:**

```
┌──────────┬─────────────┬───────────────────────────┬──────────┐
│ salt 8B  │ iv 12B      │ ciphertext (JSON)         │ tag 16B  │
└──────────┴─────────────┴───────────────────────────┴──────────┘
```

- **JSON descifrado** (es lo que se serializa a `gateways.bin`):
  `name`, `address`, `domain`, `mqtt_port`, `mqtt_tls`, `auth_token`,
  `auth_type`, `discovery`, `notes`
- Se guarda la lista en SD: `/config/gateways.bin` (MessagePack)

**El firmware ya puede IMPORTAR gateways desde `.enc`. Lo que falta es el
GENERADOR del `.enc` del lado servidor (ver §4).**

---

## 2. QUÉ FALTA / EN DESARROLLO

| # | Pendiente | Estado | Dónde |
|:--|:---|:---|:---|
| 1 | **Integrar el gateway-router real** (ABR/ASBR: lee MeshHeader, rutea BGP/OSPF/ARP, no sirve contenido). El ESP32 debe pasar por él antes de llegar al server de contenido | ❌ no integrado | prototipo en `drafts/.../gateway_router.py` |
| 2 | Generador de `.enc` (endpoint `/api/v1/provision/generate`) | ❌ no existe | gateway (servidor) |
| 3 | Integrar capa MeshHeader en `server/main.py` (proxy web real) | ❌ | `gateway/server/main.py` |
| 4 | Unificar Control byte en firmware (híbrido 3 niveles) | ⏳ diseño en drafts | `mesh_header.c` |
| 5 | Resolver tag `0x15` (PANEL vs CHECKBOX) | ⏳ conflicto | firmware + compilador |
| 6 | Router mesh real (BGP/OSPF/ARP) fuera de drafts, como server accesible | ⏳ prototipo | `drafts/.../gateway_router.py` |
| 7 | Negociación de resolución ESP32→gateway | ❌ | firmware |
| 8 | Pruebas automatizadas del modo binario | ⏳ manual | `test_server.py` |

---

## 3. PROBLEMA CONOCIDO: IP HARDCODEADA

El firmware **aún tiene la IP del gateway hardcodeada**:

- `firmware/src/Network/TlvNetworkClient.cpp:7` → `"192.168.66.254"`
- `firmware/src/Network/TlvNetworkClient.h:19` → `"192.168.66.254"` (default de `init()`)

Además, `TlvNetworkClient::init()` **no se llama en ningún sitio** — el
firmware usa la IP estática del código, ignorando la config del gateway
guardada en NVS/SD (`ConfigManager`).

**Consecuencias:**
- Cambiar de red → hay que recompilar y reflashear.
- La IP es la de una PC con DHCP: puede cambiar al reconectar el WiFi.
- No se aprovecha la lista de gateways que el firmware YA sabe gestionar.

**Solución planificada (ver §4):** dejar de hardcodear y usar el gateway
configurado (provisionado por `.enc` o por la UI `AddGatewayModal`).

---

## 4. PLAN: PROVISIONAR GATEWAYS CON EL ENCODER (SIN HARDCODEAR)

Objetivo: que el ESP32 tome el gateway de su configuración (SD/NVS) en vez
de la IP del código.

### 4.1. Generar el `.enc` desde el servidor

Falta implementar el generador. Los parámetros ya los define el firmware
(§1). El generador debe:

1. Recibir los datos del gateway (name, address, domain, mqtt_port, token…).
2. Generar `salt` (8B) aleatorio + `iv` (12B) aleatorio.
3. Derivar clave: `PBKDF2-HMAC-SHA256(pin, salt, iter=10000, dklen=32)`.
4. Cifrar el JSON con **AES-256-GCM** → ciphertext + tag(16B).
5. Escribir `salt || iv || ciphertext || tag` en el `.enc`.

Se propone exponerlo como endpoint en el servidor gateway:
`POST /api/v1/provision/generate` (diseño en `drafts/tlv proxy ruteo/`).

### 4.2. El firmware usa el gateway configurado

En vez de la IP hardcodeada de `TlvNetworkClient`:

```
TlvNetworkClient::setGatewayConfig(g_activeGateway.address, 8765)
```
llamado al arrancar con el gateway activo de `ConfigManager::loadActiveGateway`
(ya implementado en `main.cpp:313`). Así el ESP32 habla con el gateway
provisionado, no con el del código.

### 4.3. Flujo completo de provisionamiento

```
[servidor] POST /api/v1/provision/generate
    → .enc (salt+iv+ciphertext+tag)   (falta implementar)
[PC] copia el .enc a la SD del ESP32
[ESP32] ConfigManager::importGateway("x.enc", PIN)   ✅ ya existe
    → guarda en /config/gateways.bin  (MessagePack)  ✅ ya existe
[ESP32] arranca → usa g_activeGateway.address        ⏳ falta conectar
```

---

## 5. ARQUITECTURA DEL SISTEMA (estado real)

### 5.1. HOY (probado y funcionando): ESP32 → server de contenido directo

```
┌─────────────────┐   WiFi TCP   ┌──────────────────────────────────┐
│   ESP32-S3      │◀────────────▶│  server de contenido  (Python)   │
│  Navegador TLV  │  MeshHeader  │                                  │
│                 │    + TLV     │  gateway/tlvgl/tlvgl_server.py   │
│  TlvBrowserView │              │    (sirve páginas .tlvgl)        │
│  TlvNetworkClient│             │  mesh_proto.py  (capa de red)    │
└─────────────────┘              └──────────────────────────────────┘
   NO hay gateway/router de por medio (LAN misma subred)
```

### 5.2. OBJETIVO: el gateway-router se interpone y rutea

```
┌──────────┐  MeshHeader  ┌──────────────────┐  reenvía  ┌───────────────────┐
│  ESP32   │─────────────▶│  GATEWAY-ROUTER  │──────────▶│ server contenido  │
│ (nodo    │              │  rutea, NO sirve │           │  tlvgl_server.py  │
│  cliente)│◀─────────────│  BGP/OSPF/ARP    │◀──────────│  (otro nodo)      │
└──────────┘   respuesta   └──────────────────┘   resp.   └───────────────────┘
                              (drafts/gateway_router.py, a integrar)
```

- **El gateway es un router ABR/ASBR:** lee el Control byte, decide si entregar
  o reenviar (Pseudo-BGP inter-ASN, Pseudo-OSPF intra-zona, tabla ARP local).
  **No sirve contenido él mismo** — reenvía tramas al server TLVGL que sí lo hace.
- **Hoy el gateway no está en el camino:** el ESP32 habla directo con el server.
  Integrarlo (pendiente #1 de §2) es poner el `gateway_router.py` delante.
- **El TLV es agnóstico al transporte:** mismo payload por WiFi (TCP-IP) o
  por radio mesh (MeshHeader+TLV). El router decide por el MeshHeader.
- Documentos de diseño y prototipos: `drafts/tlv proxy ruteo/`.
