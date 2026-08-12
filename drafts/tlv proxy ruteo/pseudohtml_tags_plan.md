# Arquitectura y Expansión de PseudoHTML (Protocolo TLV)

Este documento detalla la estructura actual del formato binario PseudoHTML utilizado para renderizar interfaces web en MicroPythonOS a través de LVGL, y propone la expansión del estándar para soportar controles avanzados interactivos nativos.

## 1. Estructura Híbrida de Etiquetas (Tags de Longitud Variable)

Para resolver el problema del límite de 256 etiquetas sin desperdiciar ancho de banda en controles básicos, el protocolo adopta un **Esquema de Tags Híbridos** (similar a UTF-8).

* **Tags Básicos (1 Byte):** De `0x00` a `0xFE`. Usados para los controles de UI más comunes (Botones, Sliders, Textos). Ocupan el mínimo espacio posible.
* **Tag de Extensión (`0xFF`):** Si el sistema lee un Tag `0xFF`, sabe que la etiqueta real está codificada en los **siguientes 1 o 2 bytes**. Esto permite tener más de 65,000 etiquetas disponibles (para diccionarios masivos de palabras, atajos de URLs, o controles ultra-específicos) sacrificando espacio solo cuando se usan estas funciones avanzadas, manteniendo la eficiencia en el 99% del código.

## 2. ¿Cómo sabe el sistema que está recibiendo PseudoHTML?
Para evitar fallos catastróficos si por error el Gateway envía una descarga de archivo binario (ej. un PDF) en lugar de una página web, el estándar profesional dicta agregar un **Magic Number** (Identificador de Protocolo).
Deberíamos modificar el gateway para que los primeros 2 bytes del cuerpo siempre sean `0x50 0x48` (ASCII para 'PH' - PseudoHTML). Si el C-Module recibe el buffer y no empieza por `0x50 0x48`, aborta el renderizado.

## 2. Etiquetas (Tags) Actuales en Uso

El núcleo del protocolo utiliza **1 Byte de Control (Tag)**, permitiendo hasta 256 elementos distintos. Actualmente utilizamos 6 de la capa de visualización (Downlink) y 2 para interacción (Uplink):

### Downlink (Gateway -> Cliente)
- `0x10` (**PAGE**): Contenedor principal y limpieza de pantalla.
- `0x11` (**TEXT**): Mapeado a `lv_label`. Su primer byte de valor es el Estilo (P, H1, H2, etc).
- `0x12` (**LINK**): Mapeado a `lv_btn` con un `lv_label` hijo.
- `0x13` (**INPUT**): Mapeado a `lv_textarea`. Permite invocar teclado en pantalla.
- `0x14` (**IMAGE**): Mapeado a `lv_image` para imágenes pre-decodificadas.
- `0xFE` (**END**): Instrucción de cierre estructural (equivalente a `</div>` o `</body>`).

### Uplink (Cliente -> Gateway)
- `0x01` (**REQ_URL**): Solicitud de navegación.
- `0x02` (**REQ_INPUT_SUBMIT**): Envío de texto digitado por el usuario.

## 3. Propuesta de Nuevas Etiquetas (Expansión LVGL)

Para explotar el verdadero potencial del motor LVGL sin sobrecargar el ancho de banda LoRa con CSS o HTML complejo, debemos mapear elementos semánticos de HTML5 a los Widgets nativos de LVGL. 

Propongo agregar la siguiente tabla de Widgets a nuestro estándar TLV:

| Tag Hex | Nombre PseudoHTML | Equivalente HTML | Widget LVGL | Payload (Value) Estructurado |
| :---: | :--- | :--- | :--- | :--- |
| `0x15` | **CHECKBOX** | `<input type="checkbox">` | `lv_checkbox` | `[x:2][y:2][w:2][h:2][id:1][state:1][Texto]` |
| `0x16` | **SWITCH** | `<input type="checkbox" class="toggle">`| `lv_switch` | `[x:2][y:2][w:2][h:2][id:1][state:1]` |
| `0x17` | **SLIDER** | `<input type="range">` | `lv_slider` | `[x:2][y:2][w:2][h:2][id:1][min:2][max:2][val:2]` |
| `0x18` | **PROGRESS** | `<progress>` | `lv_bar` | `[x:2][y:2][w:2][h:2][min:2][max:2][val:2]` |
| `0x19` | **DROPDOWN** | `<select>` | `lv_dropdown` | `[x:2][y:2][w:2][h:2][id:1][Options\0]` |
| `0x1A` | **PANEL** | `<div>` / `<panel>` | `lv_obj` (Bento) | `[x:2][y:2][w:2][h:2][bg_color:2]` |
| `0x1B` | **CHART** | `<chart>` | `lv_chart` | `[x:2][y:2][w:2][h:2][type:1][pts:2][vals:2*N]` |
| `0x1C` | **ARC** | `<input type="range" class="radial">` | `lv_arc` (Perilla) | `[x:2][y:2][w:2][h:2][id:1][min:2][max:2][val:2]` |
| `0x1D` | **SPINNER** | `<spinner>` | `lv_spinner` | `[x:2][y:2][w:2][h:2][spin_time:2][arc_length:2]` |
| `0x1E` | **ROLLER** | `<select mode="wheel">` | `lv_roller` | `[x:2][y:2][w:2][h:2][id:1][Options\0]` |
| `0x1F` | **MSGBOX** | `<dialog>` / `<modal>` | `lv_msgbox` (Modal) | `[title\0msg\0buttons\0]` |
| `0x22` | **TABVIEW** | `<nav>` / `<tab>` | `lv_tabview` (Tabs) | `[tab_count:1][Titles\0]` |
| `0x23` | **TILEVIEW** | `<div class="carousel">` | `lv_tileview` (Gestos) | `[rows:1][cols:1]` |
| `0x24` | **ANIMIMG** | `<img src="*.gif">` | `lv_animimg` / `lv_gif` | `[x:2][y:2][w:2][h:2][frames:1][Gif_Bytes]` |
| `0x25` | **SOUND** | `<audio src="beep">` | Tone / RTTTL / Beep | `[sound_id:1][rtttl_string\0]` (Ultraligero < 50B) |

> [!NOTE]
> **Teclado en Pantalla (`lv_keyboard`):** No requiere un tag TLV. El firmware del ESP32 se encarga de manera 100% nativa de desplegar el teclado del sistema al enfocar cualquier campo de texto `<input>` (`TYPE_ABS_INPUT`).

## 4. ¿Cómo funcionaría la integración?

1. **En el Gateway (Parser Python):**
   Al leer un tag HTML como `<input type="range" min="0" max="100" value="50">`, el Gateway ignora el texto por completo. Crea un paquete TLV con el TAG `0x17`, empaca `0x0000` (min), `0x0064` (max) y `0x0032` (valor 50) usando `struct.pack(">HHH", 0, 100, 50)`.
   
2. **En el Cliente (Motor C TLV_Browser):**
   El código en C detecta el case `0x17`. Crea un slider llamando a `lv_slider_create(current_parent)`, establece los rangos llamando a `lv_slider_set_range(slider, min, max)` y asigna el valor con `lv_slider_set_value(slider, val, LV_ANIM_OFF)`.

Este enfoque garantiza que el microcontrolador renderice controles interactivos hermosos, acelerados por hardware y a 60 FPS, recibiendo tan solo unos 6 u 8 bytes de datos a través de la radio LoRa, descartando completamente la necesidad de enviar hojas de estilo (CSS) o scripts (JS).

## 5. Evolución y Arquitecturas de Compresión Evaluadas

Para llegar al estándar definitivo, se evaluaron 3 enfoques de compresión de diccionario:

### 5.1. Plan 1: Esquema de 3 Niveles Variables (1, 2 y 3 Bytes)
- **Concepto:** Asignación jerárquica según la frecuencia de uso.
- **Estructura:**
  - Nivel 1 (1 Byte): 128 atajos universales (`https://`, `.com`).
  - Nivel 2 (2 Bytes): 8,192 palabras cotidianas del idioma local.
  - Nivel 3 (3 Bytes): 2 millones de entradas para la larga cola.
- **Ventaja:** Ahorro extremo de 1 byte en los tokens más repetidos.

---

### 5.2. Plan 2: Selección por Bit-Partitioning (128 Diccionarios Fijos de 3 Bytes)
- **Concepto:** El primer byte del token (`0x80` a `0xFF`) actúa estrictamente como Selector de Diccionario (128 diccionarios posibles), seguido de 2 bytes de ID (16 bits).
- **Capacidad Total:** $128 \times 65,536 = \mathbf{8,388,608 \text{ de atajos}}$.
- **Ventaja:** Aislamiento absoluto por idiomas (`dict_es.bin`, `dict_ru.bin`, `dict_zh.bin`, `dict_urls.bin`), cero colisiones entre escrituras y decodificación directa en $O(1)$.

---

### 5.3. Plan 3 (Estándar Oficial): Arquitectura Híbrida Unificada

Al dividir la mitad superior de 1 byte (`0x80` a `0xFF` = **128 valores en total**), el espacio se reparte matemáticamente de la siguiente manera:

1. **Rango VIP / Ultra-Corto (`0x80` - `0xBF` | 64 Valores | 1 Byte Total):**
   - 64 atajos universales de 1 solo byte (`https://`, `.com`, `www.`, `para`).
2. **Rango Core Local (`0xC0` - `0xDF` | 32 Valores | 2 Bytes Totales):**
   - 32 bloques que se combinan con el segundo byte para dar **8,192 atajos de 2 bytes** para el vocabulario común local.
3. **Rango de Diccionarios Especializados (`0xE0` - `0xFF` | 32 Seleccionadores | 3 Bytes Totales):**
   - Los 32 valores restantes actúan como Selectores de Diccionario. Cada uno da acceso a 65,536 entradas ($32 \times 65,536 = \mathbf{2,097,152 \text{ de atajos aislados}}$) para idiomas (Cirílico, CJK, Tailandés), Macro-Tokens de URLs completas y endpoints de la red Mesh Alternet.

### Cuadro Comparativo

| Enfoque | Capacidad Máxima | Costo Mínimo | Seleccionadores de Diccionario |
| :--- | :---: | :---: | :---: |
| **Plan 1 (3 Niveles)** | 2.0 Millones | 1 Byte | 0 (Compartido) |
| **Plan 2 (128 Diccionarios Puros)** | 8.3 Millones | 3 Bytes | 128 Diccionarios Fijos |
| **Plan 3 (Híbrido Unificado)** | **2.1 Millones + VIP** | **1 Byte** | **32 Diccionarios Especializados** |

---

## 6. Aceleración por Hardware Vectorial (SIMD Xtensa LX7 en ESP32-S3)

Para lograr el máximo rendimiento de decodificación en el cliente sin depender del Heap de MicroPython ni saturar la RAM, el motor nativo en C (`c_mpos/src/tlv_browser.c`) aprovecha las **Extensiones Vectoriales de 128-bits del procesador Xtensa LX7** del ESP32-S3.

### 6.1. Optimización Paralela SIMD (Single Instruction, Multiple Data)

En lugar de recorrer los buffers de memoria byte por byte mediante bucles tradicionales en C (que toman 1 ciclo de reloj por cada caracter), la implementación nativa utiliza intrínsecas SIMD:

1. **Búsqueda Paralela de Marcadores de Compresión (16x Speedup):**
   - El decodificador carga **16 bytes de buffer de radio simultáneamente** en un registro vectorial de 128-bits (`EE.VEC.*`).
   - Se realiza la comparación de los marcadores de extensión (`0xFF`) de los 16 bytes en **1 solo ciclo de reloj**.

2. **Búsqueda de Diccionario y Comparación de Texto a Nivel de Silicio:**
   - La verificación de coincidencia entre fragmentos de texto y entradas de diccionario se ejecuta comparando bloques de 16 caracteres a la vez.

3. **Cero Impacto en el Heap de MicroPython:**
   - Toda la aceleración vectorial se compila a bajo nivel en la capa C nativa utilizando **ESP-DSP**. No consume memoria del Heap de Python ni genera recolección de basura (`GC`), garantizando 60 FPS constantes en la interfaz LVGL.
