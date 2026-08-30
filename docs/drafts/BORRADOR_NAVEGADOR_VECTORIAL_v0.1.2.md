# Borrador de Especificación Técnica: Navegador Vectorial Alternet (v0.1.2)

**Estado:** Borrador de Arquitectura (Draft)  
**Versión:** 0.1.2-vector-draft  
**Target:** CBDos (ESP32-P4 / ESP32-S3)  
**Ubicación:** `docs/drafts/BORRADOR_NAVEGADOR_VECTORIAL_v0.1.2.md`  

---

## 📌 1. Visión General y Objetivos

El **Navegador Vectorial Alternet (v0.1.2)** es una evolución desacoplada de la interfaz de navegación de CBDos. Su objetivo principal es permitir la visualización de sitios y portales de la red Alternet mediante un **Modelo de Documento Vectorial (V-DOM)** en lugar de HTML/CSS pesado.

> [!IMPORTANT]
> **Aislamiento Arquitectónico (Zero Breakage):**
> Este borrador define un componente completamente independiente (`VectorBrowserView`). No altera, modifica ni reemplaza el navegador o visor actual (`v0.1.1`) en producción, garantizando la estabilidad del sistema.

---

## 🏗️ 2. Arquitectura del Motor Vectorial

```
+-----------------------------------------------------------------------+
|                        Sitio / Documento Alternet                     |
|                (Formato binario TLV o JSON ligero en Red/SD)          |
+-----------------------------------------------------------------------+
                                   |
                                   v
+-----------------------------------------------------------------------+
|                    Vector DOM Parser (V-DOM v0.1.2)                  |
|          Convierte el diccionario de nodos en objetos en RAM          |
+-----------------------------------------------------------------------+
                                   |
                                   v
+-----------------------------------------------------------------------+
|                  Layout & Coordinate Engine (Flex/Grid)               |
|       Calcula coordenadas relativas (PCT/PX) para 480x800 y 320x480   |
+-----------------------------------------------------------------------+
                                   |
                                   v
+-----------------------------------------------------------------------+
|               LVGL 9.5 Primitive Vector Renderer & Cache              |
|   +-------------------+  +-------------------+  +-------------------+ |
|   |  lv_draw_rect     |  |  lv_draw_line     |  |  lv_draw_label    | |
|   | (Tarjetas/Bordes) |  | (Ondas/Gráficos)  |  | (Tipografía Vector)| |
|   +-------------------+  +-------------------+  +-------------------+ |
|   +-----------------------------------------------------------------+ |
|   |  LVGL Image Cache (Micro-Imágenes PNG/RGB565 en PSRAM)          | |
|   +-----------------------------------------------------------------+ |
+-----------------------------------------------------------------------+
```

---

## 📄 3. Especificación del Modelo de Documento Vectorial (V-DOM Schema)

Las páginas de la red Alternet se estructuran como un diccionario de nodos primitivos.

### Esquema JSON Representativo:

```json
{
  "protocol_version": "0.1.2",
  "meta": {
    "title": "Portal Alternet Mesh",
    "theme": "glassmorphism_dark",
    "refresh_interval_ms": 0
  },
  "body": [
    {
      "type": "card",
      "width": "100%",
      "height": "auto",
      "radius": 16,
      "bg_color": "0x1B1E29",
      "bg_opa": 60,
      "border_color": "0x00E5FF",
      "border_width": 1,
      "children": [
        {
          "type": "text",
          "content": "Servidor de Campo #04",
          "font_size": 18,
          "color": "0xFFFFFF"
        },
        {
          "type": "text",
          "content": "Conectado vía ESP-NOW Mesh (Canal 6)",
          "font_size": 12,
          "color": "0x94A3B8"
        }
      ]
    },
    {
      "type": "vector_chart",
      "width": "100%",
      "height": 120,
      "line_color": "0x9D4EDD",
      "line_width": 2,
      "data_points": [12, 34, 56, 22, 78, 90, 45, 67]
    },
    {
      "type": "grid",
      "columns": 2,
      "children": [
        {
          "type": "button",
          "label": "Telemetría",
          "icon": "LV_SYMBOL_CHARGE",
          "action": "nav://telemetry",
          "accent_color": "0x00F5D4"
        },
        {
          "type": "button",
          "label": "Archivos",
          "icon": "LV_SYMBOL_DIRECTORY",
          "action": "nav://files",
          "accent_color": "0xF77F00"
        }
      ]
    },
    {
      "type": "image",
      "src": "A:/assets/small_badge.png",
      "width": 32,
      "height": 32,
      "align": "center"
    }
  ]
}
```

---

## 🎨 4. Renderizado con Primitivas de LVGL 9.5

1. **Cards & Contenedores:**
   - Se procesan con `lv_obj_create` o primitivas de dibujo directo en capa.
   - Aplican el estilo de acrílico/glassmorphism de CBDos: `bg_color`, `bg_opa` (60%), `border_color` neón y `radius` squircle.

2. **Gráficos y Separadores Vectoriales:**
   - Dibujados con `lv_draw_line` conectando la serie de puntos entregados por el nodo `vector_chart`.
   - Permite visualizar datos de sensores o redes sin necesidad de librerías externas.

3. **Imágenes Pequeñas (Micro-Assets):**
   - Gestión eficiente en PSRAM usando `lv_image_cache`.
   - Limita el tamaño máximo de imágenes a 64x64 píxeles para preservar la velocidad de renderizado a 60 FPS.

---

## 🔗 5. Mapeo de Acciones e Interacción

El navegador soporta eventos táctiles mapeados a esquemas URI:

- `nav://<path>`: Navegación interna a otra página del portal.
- `alternet://<ip_or_mesh_id>/<page>`: Solicitud de documento vectorial a un nodo remoto.
- `lua://<script_name>`: Ejecución de un script interactivo local en el motor Lua de CBDos.

---

## 🚀 6. Plan de Implementación Futura (Fase 0.1.2)

1. **Crear vistas desacopladas:**
   - Header: `VectorBrowserView.hpp`
   - Fuente: `VectorBrowserView.cpp`
   - Parser: `VectorDOMParser.cpp`
2. **Registrar como aplicación opcional en el Dashboard:**
   - Acceso independiente mediante icono "Navegador Vectorial".
3. **Optimización de Caché:**
   - Uso exclusivo de PSRAM para la estructura V-DOM activa y buffers de micro-imágenes.
