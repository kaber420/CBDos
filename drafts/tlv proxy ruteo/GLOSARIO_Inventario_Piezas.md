# GLOSARIO & INVENTARIO: QUÉ ES CADA PIEZA (con nombres reales)

**Fecha:** 2026-08-11
**Proyecto:** espOS32 / pseudohtml
**Objetivo:** Terminar de una vez con la confusión de nombres. Aquí se define **qué es** cada componente según lo que **hace**, no según el nombre que le pusieron al archivo. Si un archivo tiene un nombre engañoso, se anota y se propone renombrarlo.

> **Regla de oro:** un **server** sirve contenido. Un **gateway/proxy** rutea o intermedia tráfico. **Nunca llamar "gateway" a un server solo porque cuelga de internet o recibe tramas.**

---

## 1. LA JERARQUÍA (3 CAPAS DISTINTAS)

```
┌─────────────────────────────────────────────────────────────┐
│  CAPA 1 · TRANSPORTE/RUTEADO · "GATEWAY"                    │
│  Qué es: puerta de enlace + router                          │
│  Hace: leer/re-enrutar MeshHeader, ruteo (Pseudo-BGP/OSPF), │
│        puente (VPN/enlaces), inyectar salida a internet     │
│  Estado: ❌ NO IMPLEMENTADO — solo diseño                    │
└─────────────────────────────────────────────────────────────┘
        ▲
      (rutea/hace de puente; es OPCIONAL en LAN directa)
┌─────────────┐        ┌──────────────────────────────────────┐
│  CAPA 2 ·   │        │  CAPA 3 · APLICACIÓN / CONTENIDO     │
│  INTERMEDIO │        │  "SERVER" (hosting / proxy de página)│
│  RENDER     │──▶     │  Sirve o transcodea páginas a TLV    │
└─────────────┘        └──────────────────────────────────────┘
```

- **Si es LAN WiFi local:** ESP32 llega directo al SERVER por IP → el gateway (capa 1) **no interviene**, no se necesita para que funcione.
- **Si es LoRa mesh:** el ESP32 manda MeshHeader+TLV; el **gateway (capa 1)** debe enrutar la trama hasta el server (capa 3). Ahí el gateway es imprescindible.

---

## 2. INVENTARIO CON NOMBRE REAL

### 2.1. SERVER · Hosting de contenido TLVGL
| | |
|:---|:---|
| **Archivo** | `pseudohtml/gateway/tlvgl/tlvgl_server.py` |
| **Qué es** | Server (hosting local) que sirve contenido propio |
| **Qué hace** | Escucha TCP 8765, toma un archivo de `content/`, lo compila a TLV con el viewport indicado, lo devuelve |
| **¿Rutea?** | ❌ No |
| **¿Proxy?** | ❌ No |
| **Nombre correcto** | `tlvgl_content_server.py` (hoy se llama `tlvgl_server.py`, correcto — era "gateway" lo confuso) |
| **Estado** | ✅ Funcional (tests 5/5) |
| **Nota** | ⚠️ Sigue el protocolo ASCII legado `GET ...`, NO entiende MeshHeader+TLV del firmware todavía |

### 2.2. SERVER · Generator/Proxy de páginas web ("false proxy")
| | |
|:---|:---|
| **Archivo** | `pseudohtml/gateway/server/main.py` |
| **Qué es** | Server que **renderiza/transcodea** páginas web reales a TLV |
| **Qué hace** | Escucha TCP 8080, recibe una URL, lanza un **navegador Chromium local por WebSocket CDP** para que descargue la página, extrae elementos visibles y los convierte a TLV |
| **¿Es proxy?** | ✅ **SÍ, es un proxy — el original de la idea.** Actúa de **puente entre dos mundos**: el mundo TLVGL (ESP32) y el mundo internet-HTML. Igual que la navegación "wap/Opera" vertical de antes, traduce de un mundo al otro. **No es** un proxy de red que reenvía paquetes IP, es un **proxy de pasarela/transcodificación** entre protocolos/mundos: recibe una URL (TLV), usa Chromium para traer el HTML real de internet, y devuelve la página transcodiada a TLV para el ESP32. |
| **¿Rutea?** | ❌ No rutea (el ruteo mesh es otra capa, el gateway) — pero sí hace de **pasarela TLVGL ↔ internet** |
| **Nombre correcto** | `web_to_tlv_proxy.py` / `tlvgl_internet_proxy.py` — **sí es un proxy**, solo que de transcodificación (no de red) |
| **Estado** | Parcial (usa Chromium BiDi raw; requiere el navegador instalado) |

### 2.3. SERVER · Prototipo de prueba (hosting TLV)
| | |
|:---|:---|
| **Archivo** | `espOS32/tlv proxy ruteo/gateway_prototype.py` **("Pasarela Servidor Prototipo", nombre engañoso)** |
| **Qué es** | Server de hosting TLV de prueba |
| **Qué hace** | Escucha TCP 8765, **sí habla MeshHeader+TLV** (el único que hoje, junto con el firmware, entiende la trama binaria del ESP32). Según la URL devuelve páginas hardcodeadas (galeria/clima/noticias) |
| **¿Rutea?** | ❌ No — abre la trama y sirve él mismo, no reenvía |
| **¿Proxy?** | ❌ No |
| **Nombre correcto** | `tlv_mock_server.py` / `esp32_tlv_test_server.py` |
| **Estado** | ✅ Clave para pruebas: verificado que responde `Control=0x08`+`PH` al firmware |

### 2.4. GATEWAY · (NO EXISTE todavia)
| | |
|:---|:---|
| **Qué debería ser** | Capa de ruteo MeshHeader: lee Control, decide servicio, termina o **reenvía** hacia otro nodo/server. Pseudo-BGP (ASN→next-hop), Pseudo-OSPF (zona/torre→salto), puente VPN, salida internet. **No sirve contenido él mismo.** |
| **Archivos** | Ninguno. Solo diseño en `plan_red_mesh_routing.md` |
| **Posible implementación** | `mesh_proto.py` (capa de red Python, ver `Especificacion_Protocolo_Gateway.md` §6) |
| **Estado** | ❌ **A IMPLEMENTAR** — es la pieza que falta |

---

## 3. TABLA RÁPIDA DE DECISIÓN

| Lo que NECESITO | Qué CONVIENE usar | Cómo se llama bien |
|:---|:---|:---|
| Probar el ESP32 ↔ TLV por WiFi **ya** | `gateway_prototype.py` (`drafts/tlv proxy ruteo/`) | `tlv_mock_server.py` (server de prueba) |
| Publicar contenido propio hosteado (~20MB) | `tlvgl_server.py` | `tlvgl_content_server.py` (server hosting) |
| Navegar páginas reales de internet → TLV | `server/main.py` | `web_to_tlv_proxy.py` (proxy de transcodificación TLVGL↔internet) |
| Ruteo mesh real (LoRa, VPN, BGP/OSPF) | NO EXISTE | **gateway** (a implementar) |

---

## 4. CONFUSIONES QUE YA NO DEBEN REPETIRSE

1. **"Gateway real" ≠ `gateway/server/`.** La carpeta `gateway/server/` de pseudohtml contiene **servers/proxy**, no un gateway-router. `gateway_prototype.py` tampoco es un gateway.
2. **`server/main.py` SÍ es un proxy — pero de transcodificación, no de red.** Traduce entre el mundo TLVGL y el mundo internet-HTML (estilo navegación vertical Opera/wap). No reenvía paquetes IP; usa Chromium para traer el HTML real y lo devuelve como TLV al ESP32.
3. **Hosting ≠ gateway.** Que un server escuche en un puerto y devuelva TLV no lo hace gateway. El gateway rutea/reenvía.
4. **`tlvgl_server.py` está bien nombrado** (es un server). Lo confuso era solo que coexistía en una carpeta llamada `gateway/`.

---

## 5. RENOMBRAMIENTOS PROPUESTOS (para futura limpieza)

| Actual | Propuesto | Razón |
|:---|:---|:---|
| `gateway/server/main.py` | `server/web_to_tlv_proxy.py` | Es un proxy de transcodificación TLVGL↔internet (llamarlo bien) |
| `gateway/server/` (carpeta) | `server/` | No es un gateway |
| `drafts/.../gateway_prototype.py` | `drafts/.../tlv_mock_server.py` | No es un gateway, es un server de prueba |
| `drafts/.../Especificacion_Protocolo_Gateway.md` | mantener | Es la especificación del protocolo del **futuro gateway** + servers |

---

## 6. QUÉ FALTA IMPLEMENTAR (para tener el sistema completo)

| # | Pieza | Tipo | Prioridad |
|:--|:---|:---|:---|
| 1 | **Gateway-router** (`mesh_proto.py`): leer/reenviar MeshHeader, ruteo, puente | GATEWAY | Alta |
| 2 | Que `tlvgl_server.py` entienda MeshHeader+TLV (no solo ASCII) | SERVER | Alta |
| 3 | Endpoint `/api/v1/provision/generate` (descarga `.enc`) | GATEWAY/SERVER | Media |
| 4 | Renombramientos de §5 | — | Baja/limpieza |