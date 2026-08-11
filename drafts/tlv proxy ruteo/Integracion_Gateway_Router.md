# Integración del Gateway-Router (diseño + pasos)

**Fecha:** 2026-08-11
**Estado:** plan de integración — el router ES prototipo (`drafts/`), aún no está en el camino real

---

## 1. EL PROBLEMA: hoy no hay gateway en el camino

El flujo probado hoy funciona directo, **sin router de por medio**:

```
ESP32 ──► tlvgl_server.py      (server de contenido: sirve páginas)
```

- El `tlvgl_server.py` recibe la trama y sirve el contenido. **No rutea.**
- El gateway-router (`gateway_router.py`) es la pieza que **sí rutea**
  (BGP/OSPF/ARP) y **no sirve contenido** — pero solo existe como prototipo
  en `drafts/` y nadie lo usa todavía.

**Lo que hay que lograr:** poner el gateway-router en medio del ESP32 y el
server de contenido, manteniendo el ESP32 sin tocar.

```
ESP32 ──► GATEWAY-ROUTER ──► tlvgl_server.py   (objetivo)
          (rutea, no sirve)    (sirve contenido)
```

---

## 2. QUÉ ES CADA PIEZA (no confundir)

| Pieza | Rol | Estado |
|:---|:---|:---|
| **Gateway-router** (`gateway_router.py`) | Router ABR/ASBR: lee MeshHeader, decide entregar/reenviar/drop (Pseudo-BGP inter-ASN, Pseudo-OSPF intra-zona, tabla ARP local). **No sirve contenido.** | ⏳ prototipo en `drafts/tlv proxy ruteo/` |
| **Server de contenido** (`tlvgl_server.py`) | Hosting: sirve páginas `.tlvgl` desde `content/`. Habla MeshHeader+TLV + legacy `GET`. **No rutea.** | ✅ funciona (`gateway/tlvgl/`) |
| **Proxy web** (`server/main.py`) | Transcodifica internet→TLV. | ❌ no integrado aún |
| **Firmware** | Navegador TLVGL. Envía `MeshHeader+TLV` vía TCP:8765. | ✅ funciona |

---

## 3. EL GATEWAY-ROUTER (estado del prototipo)

`drafts/tlv proxy ruteo/gateway_router.py` — clase `GatewayRouter`:

- `parse_header()`: cabecera variable según flags (3B/9B/13B/19B/21B) +
  **Control byte híbrido de 3 niveles** (`0xFF` → nivel 2/3).
- `route_or_forward(parsed)`: devuelve `deliver | forward | drop`.
- `_route_ospf()`: torre local → deliver; torre vecina → forward por trunk WiFi.
- `_tx(peer, data)`: reenvía la trama cruda a un peer TCP **sin abrir el payload**.
- Tablas: `BGP_TABLE` (ASN→interfaz), `OSPF_TABLE` (torre→salto),
  `CLIENT_TABLE` (Short ID/UUID→interfaz+peer, equivalente a tabla ARP),
  `INTERFACES` (radio-local kbps vs trunk WiFi/VPN Mbps).
- `MY_NODE`: identidad del router (asn/zone/tower/uuid/short).

**Verificado (pruebas en unidad):**
- GLOBAL→ASN 0x42 → `trunk-a` (wifi) ✅
- OSPF torre vecina 0xA3 → trunk wifi ✅
- torre local / uuid propio → deliver ✅
- DST_ONLY 3B a Short 0x0001 → forward por ARP al peer del ESP32 ✅
- Control 3 niveles (0xFF) y 4 modos de ID por punta ✅

**Limitaciones actuales:** es standalone (TCP server propio en :8765), las
interfaces/tablas son ejemplo, y el `forward` a un server de contenido real
aún no está encadenado — el reenvío lógico funciona, pero no hay un
`tlvgl_server` conectado detrás en la prueba end-to-end.

---

## 4. CÓMO INTEGRARLO (dos opciones)

### 4.1. Opción A — Router FRONTAL (recomendada, sin tocar ESP32)

El gateway-router escucha en el puerto que usa el ESP32 (8765), y cuando la
decisión es `deliver` con servicio TLVGL_REQUEST, **reenvía la petición al
server de contenido** en otro puerto/IP y devuelve la respuesta.

```
ESP32 (8765) ──► gateway_router.py  ──► tlvgl_server.py (8766)
                 (recibe, rutea)        (sirve contenido)
                      └──► devuelve la respuesta TLV al ESP32
```

Pasos:

1. **Renombrar/sacar el router de drafts** a producción,
   p.ej. `gateway/gateway_router.py`, e importar `mesh_proto`
   (`gateway/tlvgl/mesh_proto.py`) para parseo compartido con el server.
2. **Añadir un peer "server-contenido"** a `INTERFACES` con la IP/puerto del
   `tlvgl_server` (p.ej. `("127.0.0.1", 8766)`).
3. **En `forward()`**, cuando el destino es local (deliver) y el servicio es
   TLVGL_REQUEST, el router debe **abrir el payload para ver la URL** solo
   si es el propio servicio terminal — en la práctica reenvía la trama
   completa al server de contenido (el server ya sabe parsear MeshHeader).
4. **El server de contenido escucha en un puerto distinto** (ej. 8766) para
   no chocar con el router.
5. Devolver la respuesta del server al ESP32.

### 4.2. Opción B — Router como servicio opaco (puente)

El router reenvía la trama **sin abrir el payload** hacia el server de
contenido como un peer más (igual que un trunk). El server de contenido
decide el servicio. Menos acoplado pero el ruteo TLVGL queda en el server.

---

## 5. CAMBIOS NECESARIOS EN CADA PIEZA

| Pieza | Cambio | Detalle |
|:---|:---|:---|
| `gateway_router.py` | Sacar de drafts, importar `mesh_proto`, añadir peer server-contenido, chain del forward→server | §4.1 |
| `tlvgl_server.py` | Escuchar en puerto no-8765 (ej. 8766) cuando el router frontal lo usa | solo config |
| `mesh_proto.py` | Ya compartible (vive en `gateway/tlvgl/`) | — |
| `server/main.py` | Igual que tlvgl_server (capa MeshHeader) para servir proxy web detrás del router | futuro |
| firmware | **Ninguno** — el ESP32 sigue apuntando al router (mismo puerto 8765) | — |

---

## 6. DECISIÓN DE RUTEO EN EL ROUTER (referencia)

```
PROTOCOLO (DST_ONLY / INTRA_ZONE / GLOBAL):
1. Parsear Control byte (3 niveles, 0xFF antes que flags).
2. Leer destino (short / torre / ASN según flags).
3. ¿Asn_dst == MY_NODE.asn?
     Sí  → bajar a OSPF (mirar torre dst)
     No  → BGP_TABLE[asn_dst] → forward (trunk)
4. ¿Torre_dst == MY_NODE.tower?  → deliver (servicio local)
     No → OSPF_TABLE[tower_dst] → forward (trunk)
5. ¿Short/UUID dst es cliente de CLIENT_TABLE? → forward al peer del cliente
6. ¿Short/UUID dst es MY_NODE? → deliver
```

Para TLVGL: cuando `deliver` y `service == 0x07`, el router **reenvía al
server de contenido** (que es quien sirve la página). El router no compila
HTML ni gestiona páginas — solo enruta.

---

## 7. PLAN DE PRUEBAS DE LA INTEGRACIÓN

1. Server de contenido en :8766 (config), router en :8765.
2. Trama ESP32 real (`0x0F | TLV REQ_URL`) → router → responde el server.
3. Verificar en el log de `tlvgl_server` que llega la petición y responde TLV.
4. Verificar respuesta end-to-end: control 0x08 + magic PH + TLV.
5. DST_ONLY con short fuera de la tabla ARP → drop/re-difusión (radio-local).
6. GLOBAL con ASN que no está en BGP_TABLE → drop con motivo claro.

---

## 8. QUÉ QUEDA FUERA (futuro)

- Router en C (ESP32) para OSPF real entre nodos (hoy el router es Python en SBC).
- Descubrimiento de vecinos (hello packets) para poblar tablas automáticamente.
- Radio LoRa/FLRC como interfaz real (`radio-local`) — hoy las interfaces
  son TCP/WiFi.
- Provisionamiento de gateways sin hardcodear (`/api/v1/provision/generate`,
  formato `.enc` — ver `ESTADO_DEL_PROYECTO.md` §4).