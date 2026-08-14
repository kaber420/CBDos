# Plan de Arquitectura e Implementación: Sistema de Wallpaper (espOS32)

## 📌 Objetivo
Implementar un sistema híbrido de fondos de pantalla (wallpapers) para el launcher/dashboard de espOS32 que combine:
1. **Fondo Predeterminado Rápido (Flash - RGB565):** Carga instantánea a 60 FPS sin latencia de disco, activo por defecto si no hay tarjeta SD o si el usuario no ha seleccionado uno propio.
2. **Fondos Personalizados del Usuario (MicroSD - JPG):** Decodificación eficiente usando `TJpgDec` desde la MicroSD (`S:/wallpapers/*.jpg`), configurable desde la UI de Ajustes o la Galería.
3. **Estilo Translúcido (*Glassmorphism*):** Modificación del tema (`DefaultTheme`) para que los contenedores y tarjetas del Dashboard tengan fondo semitransparente (`bg_opa`) manteniendo los textos, iconos y bordes con nitidez y contraste 100% sólidos.

---

## 🏗️ 1. Análisis de Rendimiento y Memoria

* **Consumo de RAM del Decodificador JPEG (`TJpgDec` en LVGL v9):**
  * Búfer de trabajo de descompresión: **~3.5 KB a 4 KB**.
  * Código en Flash: **~12 KB**.
* **Memoria de Imagen en PSRAM:**
  * Resolución objetivo (ej. 480 × 280 / 480 × 320 píxeles a 16-bit RGB565): **~268 KB** a **307 KB**.
  * En el ESP32-S3 (8 MB / 16 MB PSRAM), 270 KB representa solo el **3.3% de la PSRAM total**, dejando más de 7.7 MB libres para emuladores, red y aplicaciones.

---

## 🧩 2. Componentes y Flujo de Datos

```mermaid
graph TD
    A[Boot / Inicio Dashboard] --> B[WallpaperManager::applyWallpaper]
    B --> C{¿Hay ruta en NVS y existe en SD?}
    C -- Sí (ej. S:/wallpapers/custom.jpg) --> D[Cargar JPG desde SD vía TJpgDec a PSRAM]
    C -- No / SD no montada --> E[Usar Array C RGB565 en Flash (default_wallpaper)]
    D --> F[Establecer como fondo de pantalla de la raíz LVGL]
    E --> F
    F --> G[Dashboard renderiza tarjetas translúcidas sobre el fondo]
```

### Componentes Involucrados:
1. **`WallpaperManager` (Nuevo servicio en Core/UI):**
   * Gestiona la ruta actual del wallpaper en `Preferences` (NVS).
   * Proporciona funciones: `setWallpaperFromSD(path)`, `restoreDefault()`, `getWallpaperSrc()`.
2. **`default_wallpaper.c` (Nuevo Asset en Flash):**
   * Imagen predeterminada optimizada en formato LVGL RGB565 compilada directamente en Flash.
3. **`DefaultTheme` (Actualización de Estilos):**
   * Ajuste de `applyRaisedCard` y `applyButton` para soportar `lv_obj_set_style_bg_opa(obj, LV_OPA_70, 0)` o configurable según haya o no wallpaper activo.
4. **`DashboardView` (Integración Visual):**
   * La vista principal del sistema se dibuja sobre el contenedor/pantalla con el wallpaper como fondo inferior.
5. **`ConfigView` / `GalleryView` (Selector de Wallpaper):**
   * Opción en la app de Configuración o botón contextual en la Galería ("*Establecer como Fondo de Pantalla*").

---

## 🎨 3. Especificación de Estilos (Transparencias sin Afectar Hijos)

Para que los iconos y etiquetas permanezcan perfectamente visibles y contrastados sobre cualquier imagen de fondo:

```cpp
// Regla fundamental de LVGL v9:
// NO usar lv_obj_set_style_opa (haría transparente el texto e iconos).
// SÍ usar lv_obj_set_style_bg_opa:
lv_obj_set_style_bg_color(card, lv_color_hex(0x1B1E29), 0);
lv_obj_set_style_bg_opa(card, LV_OPA_70, 0); // 70% opacidad en el fondo

// Bordes y textos 100% sólidos:
lv_obj_set_style_border_color(card, lv_color_hex(0x3B4252), 0);
lv_obj_set_style_border_width(card, 1, 0);
lv_obj_set_style_border_opa(card, LV_OPA_COVER, 0);
```

---

## 📋 4. Pasos Propuestos para Implementación Futura

1. **Fase 1: Motor y Asset por Defecto**
   * Crear asset `default_wallpaper.c` en formato RGB565.
   * Crear `WallpaperManager` con integración a NVS (`Preferences`).
2. **Fase 2: Adaptación del Tema y Dashboard**
   * Actualizar `DefaultTheme` con opacidades translúcidas elegantes.
   * Probar el renderizado fluido en el Dashboard.
3. **Fase 3: Selector de Fondos desde MicroSD**
   * Crear carpeta `/wallpapers/` en la tarjeta SD.
   * Añadir acción en `ConfigView` o `GalleryView` para seleccionar imágenes JPG y previsualizarlas/guardarlas.
