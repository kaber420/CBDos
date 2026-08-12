# ESPECIFICACIÓN MAESTRA: Protocolo Unificado espOS32 (Mesh, Ruteo, TLV y Cifrado)

**Estado:** DEFINITIVO Y OFICIAL  
**Fecha:** 2026-08-11  
**Ámbito:** Firmware ESP32-S3 (C++), Router Mesh (Go `espOS32-router`), Servidor Hosting (Python), Proxy Web.

---

## 1. PRINCIPIO ARQUITECTÓNICO DE 3 CAPAS (SEPARACIÓN ESTRICTA)

Para garantizar máximo rendimiento tanto en la Radio (LoRa/FLRC) como en WiFi/TCP, el sistema se divide en **3 capas independientes**:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ 1. CAPA DE TRANSPORTE Y RUTEO (MeshHeader)                                 │
│    - Ruteo opaco entre nodos y torres. El Router NUNCA lee el payload.     │
│    - Tamaño de cabecera determinado por el 1 Byte de Control (3B, 9B, 21B).  │
├─────────────────────────────────────────────────────────────────────────────┤
│ 2. CAPA DE INDICADORES DE PAYLOAD (Presentación y Seguridad)                │
│    - Indica el tipo de datos: 0x07 (TLVGL Req), 0x08 (Resp), 0x09 (AES-GCM).│
│    - Bloque de tamaño estirable (1 a 3 Bytes).                              │
├─────────────────────────────────────────────────────────────────────────────┤
│ 3. CAPA DE DATOS ULTRA-COMPRIMIDOS (Payload de Aplicación)                   │
│    - Texto y elementos comprimidos por Prefijos de Bits (1B, 2B, 3B).       │
│    - Palabras complejas ("otorrinolaringologo") comprimidas en 1 a 3 bytes. │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. CAPA DE RUTEO Y TRANSPORTE (`MeshHeader`)

### 2.1. Control Byte (Byte 0)
- `bit 7` `MESH_CTRL_GLOBAL_BIT` (`0x80`) $\rightarrow$ Nivel 3: Cabecera Global Inter-ASN (**21 Bytes**)
- `bit 6` `MESH_CTRL_SIGNAL_BIT` (`0x40`) $\rightarrow$ Señalización de red
- `bit 5` `MESH_CTRL_INTRA_ZONE`  (`0x20`) $\rightarrow$ Nivel 2: Intra-Zona OSPF (**9 Bytes** o **13 Bytes**)
- `bit 4` `MESH_CTRL_SHORT_ID`    (`0x10`) $\rightarrow$ IDs de 2 Bytes (Short) vs 4 Bytes (UUID)
- `bit 3` `MESH_CTRL_DST_ONLY`    (`0x08`) $\rightarrow$ Nivel 1: Local Ultra-Ligero (**3 Bytes**)
- `bits 0-2` `SERVICE_BITS`       $\rightarrow$ Identificador de servicio de red (`0x07` REQ, `0x08` RESP)

### 2.2. Tamaños Fijos por Nivel
- **Nivel 1 (DST_ONLY - 3 Bytes):** `[Control 1B] + [DstShortID 2B]`
- **Nivel 2 (Intra/Local - 9 Bytes):** `[Control 1B] + [DstID 4B] + [SrcID 4B]` (o 9B con ShortIDs)
- **Nivel 3 (Inter-ASN - 21 Bytes):** `[Control 1B] + [SrcAddr 10B] + [DstAddr 10B]`

---

## 3. TABLA PSEUDO-ARP Y CICLO DE VIDA DE IDENTIDAD

1. **Fase 1 (Handshake Inicial / Registro):**
   El ESP32 transmite usando su **UUID permanente de 4 Bytes** (o dirección de 10B) para registrarse en el Router sin riesgo de colisión.
2. **Fase 2 (Sesión Activa):**
   El Router le asigna un **Short ID de 2 Bytes** (`0x0001`) en su **Tabla Pseudo-ARP**. A partir de este momento, el ESP32 transmite usando tramas de **3 Bytes (`DST_ONLY`)** o **9 Bytes**, logrando un ahorro del 67% en la cabecera.
3. **Mapeo de Tabla Pseudo-ARP (Router RAM):**
   `ShortID (2B)` / `UUID (4B)` $\rightarrow$ `net.Conn` (Socket TCP) o `Radio Peer`.

---

## 4. SISTEMA DE COMPRESIÓN DE PAYLOAD POR PREFIJOS DE BITS (1B, 2B, 3B)

El contenido útil (nodos TLV, texto, URLs) se codifica usando el esquema de diccionarios por prefijos de bits (`0x80` a `0xFF`):

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ Rango VIP / Ultra-Corto (0x80 - 0xBF) ──▶ 1 Byte Total                      │
│   64 Atajos de 1 byte para lo más frecuente ("https://", ".com", "www.")    │
├─────────────────────────────────────────────────────────────────────────────┤
│ Rango Core Local (0xC0 - 0xDF)       ──▶ 2 Bytes Totales                    │
│   32 Bloques x 256 = 8,192 atajos de 2 bytes para vocabulario común local.  │
├─────────────────────────────────────────────────────────────────────────────┤
│ Rango Diccionarios (0xE0 - 0xFF)     ──▶ 3 Bytes Totales                    │
│   32 Seleccionadores x 65,536 = ¡2.1 MILLONES DE COMBINACIONES!             │
└─────────────────────────────────────────────────────────────────────────────┘
```

* **Rendimiento:** Palabras largas ("otorrinolaringologo", "cocacola", "veterinario") se traducen en el servidor a **tokens de 1 a 3 bytes máximo**.
* **Descompresión SIMD:** El ESP32-S3 desasocia los tokens usando instrucciones vectoriales de 128-bits del procesador Xtensa LX7 sin gastar RAM ni impactar los 60 FPS de LVGL.

---

## 5. SEGURIDAD Y TÚNEL VPN MESH (AES-256-GCM POR HARDWARE)

1. **Indicador en Payload:** Si el byte de control del payload es `0x09` (`PAYLOAD_ENCRYPTED`), el Router sabe que los datos están cifrados de extremo a extremo.
2. **Opacidad en el Router:** El Router **no descifra el payload**. Rutea únicamente el `MeshHeader` visible.
3. **Aceleración por Hardware ESP32-S3:** El ESP32-S3 pasa el ciphertext directamente por su **motor dedicado HW AES-256-GCM** sin consumir ciclos de CPU de pantalla.
4. **Identidad de Silicio (eFuses):** El chip guarda su llave única en sus eFuses de silicio.
5. **Aprovisionamiento Web Serial:** El navegador web del instalador se conecta por cable USB vía **Web Serial API**, lee la identidad del chip del eFuse, registra el dispositivo en la BD y descarga su archivo `.enc` cifrado (AES-256-GCM + PBKDF2).

---

## 6. DNS INTEGRADO EN GATEWAY / PROXY

* El ESP32 **nunca hace búsquedas DNS UDP** ni conexiones mbedTLS/SSL directas a internet.
* El ESP32 envía la URL deseada (`REQ_URL` `"http://clima.mesh"` o `"http://noticias.com"`).
* El Gateway resuelve dominios internos `.mesh` a servicios locales (`0x07` Hosting) y realiza las peticiones HTTPS pesadas hacia internet como Proxy Web, devolviendo los datos compilados en TLV.
