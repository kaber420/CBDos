# 📋 PLAN DE TRABAJO: Optimización, Arquitectura, UI y Guía Integral de MeshCore

**Estado:** Borrador de Trabajo / Especificación de Arquitectura  
**Módulos Afectados:** `core/src/apps/meshcore/`, `bsp/esp32_s3_jc3248/hal/`, `bsp/esp32_p4_jc4880/`  
**Objetivo:** Especificar el funcionamiento técnico de MeshCore (sin internet), estructurar las fases para resolver cuellos de botella de memoria y rediseñar la interfaz táctil en pantallas de 3.5" (320x480).

---

## 📡 1. Guía Integral: ¿Cómo Funciona MeshCore en CBDos (Sin Internet)?

MeshCore es un protocolo de red en malla (*Ad-Hoc Mesh*) de capa 2/3 diseñado para operar de forma 100% autónoma sobre enlaces de radio sin enrutadores, servidores ni conexión a internet (ESP-NOW 2.4 GHz, LoRa SX1262 Sub-GHz y módems USB).

```
 ┌────────────────┐          ESP-NOW / LoRa           ┌────────────────┐
 │  CYBERDECK S3  │ ◄───────────────────────────────► │  LAPTOP BASE   │
 │   ID: 0x1337   │                                   │   ID: 0xCAFE   │
 └───────┬────────┘                                   └────────────────┘
         │
         │ Reenvío Multi-salto (Hops)
         ▼
 ┌────────────────┐
 │ NODO REPETIDOR │
 │   ID: 0x88AA   │
 └────────────────┘
```

---

### 1.1. Identidad de los Nodos (Short ID y Nombre)
- **Short ID (2 Bytes):** Cada dispositivo tiene una dirección hexadecimal única (ej. `0x1337`, `0xCAFE`). No se necesita IP ni DHCP.
- **Node Name (Cadena UTF-8):** Nombre legible del nodo (ej. `"Cyberdeck-S3"`, `"Base-Laptop"`).

---

### 1.2. Descubrimiento de Nodos y Radar (Sin Adivinar IDs)
1. **Emisión de Baliza (`PKT_BEACON`):** Al encender la radio, abrir la app o pulsar **"Emitir Beacon"**, el dispositivo transmite un paquete de presentación al aire (broadcast `0xFFFF`).
2. **Captura Automática en Radar:** Cualquier nodo en el mismo canal que escuche la baliza guarda automáticamente:
   - Short ID del emisor.
   - Nombre legible.
   - Intensidad de señal (RSSI en dBm).
   - Conteo de saltos (`0` = directo en alcance visual, `1+` = a través de repetidores).
   - Interfaz de radio por donde se escuchó.
3. **Resultado:** Los usuarios no necesitan adivinar nada ni pasarse códigos por internet; los dispositivos cercanos aparecen solos en la pestaña **Radar**.

---

### 1.3. Estructura de las Tramas Binarias MeshCore

Todas las tramas inician con el número mágico `0x4D43` (`'MC'`).

#### A. Trama de Baliza (`PKT_BEACON` = 0x01):
```
┌───────────┬──────────┬──────────┬──────────┬──────────┬──────────┬─────────────┬──────────────┐
│ Magic(2B) │ Type(1B) │ Hops(1B) │ SrcId(2B)│ DstId(2B)│ Reserv(2)│ NameLen(1B) │ Nombre (N B) │
│  0x4D43   │   0x01   │  0..7    │  0x1337  │  0xFFFF  │   0x00   │     12      │ Cyberdeck-S3 │
└───────────┴──────────┴──────────┴──────────┴──────────┴──────────┴─────────────┴──────────────┘
```

#### B. Trama de Mensaje de Chat (`PKT_CHAT` = 0x02):
```
┌───────────┬──────────┬──────────┬──────────┬──────────┬───────────┬──────────┬──────────┬────────────┬─────────────┐
│ Magic(2B) │ Type(1B) │ Hops(1B) │ SrcId(2B)│ DstId(2B)│ ChanId(2B)│ MsgId(4B)│ Flags(1B)│ TextLen(1B)│ Payload (N) │
│  0x4D43   │   0x02   │  0..7    │  0x1337  │ 0xFFFF/ID│     0     │ 0x000001 │ Encrypt? │     14     │ Hola Mesh!  │
└───────────┴──────────┴──────────┴──────────┴──────────┴───────────┴──────────┴──────────┴────────────┴─────────────┘
```

---

### 1.4. Modos de Comunicación: Broadcast vs P2P Unicast

1. **Mensaje a Canal / Broadcast (`DstId = 0xFFFF`):**
   - El mensaje se entrega a todos los nodos en el canal seleccionado (`#general`, `#tactico`).
2. **Mensaje Directo Privado P2P (`DstId = 0xCAFE`):**
   - El emisor fija el `DstId` al Short ID del destinatario.
   - Los nodos intermedios de la malla pueden reenviar el paquete si están en rango, pero **solo el nodo con ID `0xCAFE` lo abre y muestra en pantalla**.
3. **Cifrado de Canal (PSK):**
   - Si un canal tiene clave (`#tactico` con PIN `1234`), el payload se cifra con RC4/AES simétrico. Los nodos sin la clave solo ven texto bloqueado `🔒 [Mensaje Cifrado]`.

---

### 1.5. Ruteo Multi-Salto y Prevención de Bucles (*Managed Flooding*)
1. Cuando un nodo recibe un paquete ajeno con `DstId != m_localShortId` y `Hops < 7`:
2. Verifica si el `MsgId` ya fue visto en su tabla anti-duplicados `m_seenPacketIds`.
3. Si ya lo vio, **lo descarta** (evita tormentas de broadcast infinitas).
4. Si es nuevo, incrementa `Hops + 1` y lo retransmite por sus otras interfaces de radio activas.

---

## 🎯 2. Objetivos del Plan de Trabajo

1. **Estabilidad Absoluta ante Ráfagas:** Evitar que ráfagas de paquetes saturen el heap o generen *Kernel Panics*.
2. **Experiencia de Usuario Fluida (UI LVGL 9.5 en 3.5" y 4.3"):**
   - Eliminar saltos de scroll y reconstrucción destructiva.
   - Solucionar textos encimados en las tarjetas del Radar mediante layout de 2 renglones con marquesina.
3. **Control Determinista de Radio:**
   - Selección manual con botón explícito `[ Guardar ]`.
   - Registro automático de peer broadcast en HAL.
4. **Libreta de Contactos Persistente:**
   - Guardar nodos descubiertos en almacenamiento no volátil.
   - Click táctil en un nodo para abrir chat privado P2P.

---

## 🧩 3. Desglose de Fases de Implementación

```
┌────────────────────────────────────────────────────────────────────────┐
│                        FASES DEL PLAN DE TRABAJO                       │
├────────────────────────────────────────────────────────────────────────┤
│  FASE 1: Arquitectura Thread-Safe (Buzón C++ Radio ↔ UI)               │
│  FASE 2: Optimización del Chat (Inserción Incremental sin Scroll Y=0)  │
│  FASE 3: Control Explícito de Radios (Botón Guardar + Peer HAL)        │
│  FASE 4: Rediseño Visual de Tarjetas Radar (Fix de Texto Encimado)     │
│  FASE 5: Libreta de Contactos Persistente y Chat Privado P2P           │
│  FASE 6: Límites de Memoria y Ring Buffers                             │
│  FASE 7: Batería de Pruebas de Estrés y Validación Multi-Target        │
└────────────────────────────────────────────────────────────────────────┘
```

---

### 🔹 FASE 1: Arquitectura Thread-Safe (Radio ↔ UI)
- [x] **Buzón Intermedio en C++:** Cola de mensajes entrantes (`std::vector<MeshMessage>`) con `std::mutex` en `MeshCoreView`.
- [x] **Desacoplamiento Estricto de Tareas:** La tarea Wi-Fi (Core 0) solo inserta en cola; cero llamadas a `lv_async_call()` desde interrupciones.
- [x] **Consumo en `BaseView::onUpdate()`:** La UI vacía la cola dentro del ciclo de render en Core 1.

---

### 🔹 FASE 2: Optimización del Chat (Chat View)
- [x] **Inserción Incremental:** Método `onMessageReceived` que crea únicamente la nueva burbuja al fondo.
- [x] **Eliminación de `lv_obj_clean` en recepción:** Prohibido el borrado masivo de widgets al recibir paquetes.
- [x] **Scroll Instantáneo (`LV_ANIM_OFF`):** Reemplazo de `LV_ANIM_ON` por scroll directo sin animación.
- [x] **Límite de Widgets en Pantalla:** Eliminar las burbujas más antiguas al superar 50 elementos.

---

### 🔹 FASE 3: Control Explícito de Radios (Pestaña "Radios")
- [x] **Eliminación de Auto-Aplicación Involuntaria:** Los dropdowns de Modo y Canal no disparan cambios al hacer scroll.
- [x] **Botón Táctil `[ Guardar ]`:** Botón de ancho completo para aplicar conscientemente la configuración.
- [x] **Sincronización de Canal en HAL:**
  - Registro de peer `FF:FF:FF:FF:FF:FF` en `S3NetworkInterface::sendPacket`.
  - Canal predeterminado en 13.
  - Toast corto y limpio: `Slot 0: ESP-NOW (CH 13)`.

---

### 🔹 FASE 4: Rediseño Visual de Tarjetas en Radar (Fix de Texto Encimado)
- [ ] **Problema Actual:** En pantallas de 3.5" (320x480), el nombre, ID, RSSI y saltos se intentan meter en una sola fila horizontal, provocando que el texto se encime y quede ilegible.
- [ ] **Solución de 2 Renglones por Tarjeta:**
  ```
  ┌─────────────────────────────────────────────────────────────┐
  │ 📡 [0xCAFE] Base-Laptop-Estacion-Taller                    │ ◄── Renglón 1: Nombre + ID (Marquesina circular)
  │ 📶 -45 dBm  •  0 saltos (Directo)  •  ESP-NOW Slot 0        │ ◄── Renglón 2: Telemetría (Texto secundario fijo)
  └─────────────────────────────────────────────────────────────┘
  ```
- [ ] **Marquesina en Renglón 1:** Configurar `LV_LABEL_LONG_SCROLL_CIRCULAR` en el nombre para nombres largos.
- [ ] **Telemetría en Renglón 2:** Fuente compacta `montserrat_12` en color secundario (`#94A3B8`).

---

### 🔹 FASE 5: Libreta de Contactos en MessagePack (`contacts.msgpack`) y Chat P2P
- [ ] **Almacenamiento Estructurado en MessagePack (MicroSD / Filesystem):**
  - **Ubicación:** `/sdcard/apps/meshcore/contacts.msgpack` (o fallback a almacenamiento local `/data/meshcore/`).
  - **Regla de Oro de Almacenamiento:** La NVS queda estrictamente reservada para configuración básica del sistema (Canal RF y Modo de radio). Todo dato de contactos, historial y libreta se serializa en binario compacto **MessagePack**.
  - **Estructura binaria del contacto:**
    ```cpp
    struct MeshContactRecord {
        uint16_t shortId;
        std::string name;
        std::string customAlias;
        bool isFavorite;
        uint32_t lastSeenEpoch;
        uint8_t preferredInterface;
    };
    ```
- [ ] **Click en Tarjeta para Chatear:** Al tocar una tarjeta en el Radar, abrir una conversación directa P2P (`targetId = node.shortId`).
- [ ] **Indicador de Destinatario en Chat:** Mostrar en la barra de entrada a quién se le está enviando (ej: `Destino: [0xCAFE] Base-Laptop` o `#general`).
- [ ] **Gestión de Contactos Guardados:** Opción en la tarjeta para añadir a "Favoritos / Guardar Contacto" y asignarle un alias personalizado.

---

### 🔹 FASE 6: Límites de Memoria y Ring Buffers
- [x] **Ring Buffer en `MeshCoreEngine`:**
  - Historial en RAM (`m_messages`) limitado a 100 mensajes.
  - Lista de nodos (`m_nodes`) limitada a 32 nodos.
- [x] **Tabla Anti-Duplicados:** `m_seenPacketIds` ampliado a 128 elementos circulares.
- [x] **Debouncing en Radar:** Redibujar solo en `onUpdate` si `m_nodesDirty == true`.

---

### 🔹 FASE 7: Procedimiento de Pruebas y Criterios de Aceptación

| Prueba | Procedimiento | Criterio de Éxito |
| :--- | :--- | :--- |
| **1. Estrés de Recepción** | Enviar 20 mensajes seguidos desde la laptop con `meshcore_container`. | Los mensajes aparecen al fondo fluidamente; 0 reinicios, 0 saltos al inicio. |
| **2. Transmisión Saliente** | Escribir un mensaje en el Cyberdeck y pulsar Enviar. | El mensaje aparece en la terminal de la laptop en < 100 ms. |
| **3. Legibilidad de Radar** | Abrir la pestaña Radar con nodos descubiertos. | Tarjetas de 2 renglones legibles sin texto encimado; marquesina en nombres largos. |
| **4. Chat Directo P2P** | Tocar un nodo en Radar y enviarle un mensaje. | El paquete viaja con `DstId = node.shortId`; solo ese nodo lo procesa. |
| **5. Persistencia tras Reinicio** | Reiniciar el Cyberdeck y abrir Radar. | Los contactos descubiertos previamente permanecen en la lista. |
| **6. Compilación Cruzada** | `pio run -d bsp/esp32_s3_jc3248` y `idf.py build` (P4). | Ambas plataformas compilan con 0 errores y 0 warnings. |
