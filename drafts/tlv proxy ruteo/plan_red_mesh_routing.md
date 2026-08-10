# Plan Futuro: Red Mesh con Pseudo-OSPF y Pseudo-BGP

## Principio Central: Leer Solo los Bytes Necesarios

Cada nodo de la red solo lee los bytes de cabecera que le corresponden
según su rol en la jerarquía. El resto del paquete pasa sin tocarlo.

```
Paquete completo (hasta 21-22 bytes de cabecera):
┌────────┬────────┬────────┬──────────────────┬─────────┐
│Control │  ASN   │  Zona  │      Torre       │  UUID   │
│  1B    │  2B    │  2B    │       2B         │   4B    │  × src y dst
└────────┴────────┴────────┴──────────────────┴─────────┘
   ↑         ↑        ↑           ↑                ↑
Router    Router    Router      Router           Nodo
frontera  de zona   de torre    local           final
(BGP)     (OSPF)    (OSPF)
lee 1+2B  lee +2B   lee +2B    lee +2B         lee +4B
```

Cada capa solo procesa su fragmento — los bytes de capas superiores
son opacos y se reenvían sin parsear.

---

## Cabeceras por Alcance

### Comunicación Local (misma torre)
El destino está en la misma torre. Se omiten ASN, Zona y Torre — se asume local.

```
┌─────────┬───────────┬───────────┐
│ Control │ UUID dst  │ UUID src  │
│   1B    │    4B     │    4B     │
└─────────┴───────────┴───────────┘
Total: 9 bytes → payload útil: 244 bytes (de 253 MTU del SX1280)
```

**Compresión Ultra-Ligera (Tabla Local Cacheada):**
Cuando los nodos locales ya están registrados en la torre (tienen una sesión o lease activo), se les asigna un identificador local (Short ID) de solo 2 bytes en lugar del UUID completo de 4 bytes. 
La cabecera se reduce a su mínima expresión:

```
┌─────────┬──────────────┐
│ Control │ Short ID dst │
│   1B    │      2B      │
└─────────┴──────────────┘
Total: 3 bytes de cabecera!
Payload útil: 250 bytes (de 253 MTU)
```
- **Airtime mínimo:** Transmitir 3 bytes de cabecera toma una fracción de milisegundo en FLRC.
- **Procesamiento nulo:** La torre lee el Short ID (2 bytes), lo busca en su tabla de RAM (`uint16_t -> slot/MAC`), y despacha. 
- **Ceros si no hay tabla:** Si un nodo aún no tiene Short ID, usa el UUID completo de 4 bytes, pero el resto de los campos (ASN, Zona, Torre) no se envían (ahorrando procesamiento).

### Comunicación Intra-Zona (distinta torre, mismo ASN+Zona)
```
┌─────────┬────────┬────────┬───────────┬───────────┐
│ Control │  Zona  │ Torre  │ UUID dst  │ UUID src  │
│   1B    │   2B   │   2B   │    4B     │    4B     │
└─────────┴────────┴────────┴───────────┴───────────┘
Total: 13 bytes → pseudo-OSPF lee Zona+Torre
```

### Comunicación Global (distinto ASN)
```
┌─────────┬───────┬────────┬────────┬───────────┬───────────┐
│ Control │  ASN  │  Zona  │ Torre  │ UUID dst  │ UUID src  │
│   1B    │  2B   │  2B    │  2B   │    4B     │    4B     │
└─────────┴───────┴────────┴────────┴───────────┴───────────┘
         ×2 (src + dst)
Total: 1 + (2+2+2+4)×2 = 21 bytes → pseudo-BGP lee solo ASN
Payload útil: 232 bytes
```

---

## Capas de Enrutamiento

### Pseudo-BGP (Inter-ASN, frontera de red)
- Lee **solo los 2 bytes de ASN destino** del paquete
- Su tabla de ruteo solo contiene entradas de 2 bytes: `ASN → next-hop`
- Es completamente ignorante de Zona, Torre y UUID — esos bytes son opacos para él
- Solo conoce sus vecinos directos y hacia qué enlace mandar cada ASN
- No necesita saber qué hay dentro de cada ASN — eso es problema del router de destino

```
Tabla de un router pseudo-BGP (eso es todo lo que sabe):

┌─────────┬────────────┐
│ ASN dst │  next-hop  │
│  2B     │  (enlace)  │
├─────────┼────────────┤
│ 0x0001  │  enlace A  │
│ 0x0042  │  enlace B  │
│ 0x00FF  │  enlace A  │
│ 0x0100  │  enlace C  │
└─────────┴────────────┘

Paquete llega:
  Lee bytes [1..2] → ASN = 0x0042
  Busca en tabla → enlace B
  Reenvía. Fin. No abre nada más.
```

### Pseudo-OSPF (Intra-Zona, entre torres)
- Lee **Zona + Torre destino** (4 bytes)
- Mantiene mapa de torres activas en su zona (link-state)
- Calcula la ruta más corta a la Torre destino
- No lee UUID — ese es trabajo del nodo final

```
Paquete llega al router de zona:
  Lee bytes [3..6] → Zona=0x01, Torre=0x00A3
  Busca ruta a Torre 0x00A3 en el mapa de zona
  Reenvía al siguiente salto
```

### Entrega Local (última milla)
- La torre lee **UUID destino** (4 bytes)
- Si tiene tabla local → puede comprimir a índice de 1-2 bytes
- Entrega directa al nodo cliente

---

## Ventajas del Diseño

**Sin overhead de procesamiento:**
Cada nodo parsea solo su porción de cabecera. Un router BGP
que procesa 10,000 paquetes/segundo nunca toca los 16 bytes
de UUID — los reenvía opacos.

**Cabeceras variables sin campo de longitud:**
El tamaño de cabecera se infiere del flag de Control:
```
Control byte:
  bit 7: 0 = local (9B cabecera)   1 = global (21B cabecera)
  bit 6: 0 = datos normales        1 = señalización de red
  bits 5-0: tipo de servicio (TLVGL=0x07, Proxy=0x05, Chat=0x01...)
```
No hay campo de longitud de cabecera — el router sabe qué leer
con solo mirar el primer byte.

**Pseudo-IP independiente de IPv4:**
El UUID de 4 bytes (Pseudo-IP) puede mapearse a IPv4 si se quiere
interoperar, o ser completamente independiente.
En redes puramente mesh no hay IPv4 — ahorra toda la pila TCP/IP.

---

## Relación con TLVGL

El payload TLVGL viaja como datos dentro de este protocolo.
El tipo de servicio en el Control byte lo identifica:

```
Control = 0x07 → TLVGL_REQUEST  (ESP32 pide una página)
Control = 0x08 → TLVGL_RESPONSE (gateway entrega el payload)
```

Los routers intermedios no saben qué es TLVGL — solo ven el
Control byte y reenvían. El contenido les es opaco.

---

## Estado

- [x] Especificación de cabeceras en `custom_mesh_protocol.md`
- [ ] Implementación del router pseudo-BGP (Python en SBC)
- [ ] Implementación del router pseudo-OSPF (C en ESP32-S3)
- [ ] Tabla local con índices comprimidos (2-3 bytes)
- [ ] Integración con adaptador de transporte TLVGL
- [ ] Protocolo de descubrimiento de vecinos (hello packets)
