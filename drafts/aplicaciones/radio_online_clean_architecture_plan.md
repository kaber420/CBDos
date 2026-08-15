# Borrador / Plan de Implementación: Radio Online (espOS32)

## 1. Diagnóstico del Problema Anterior
1. **Conflicto de Scroll Táctil con `lv_tabview` (LVGL v9):**
   - El widget `lv_tabview` genera internamente un contenedor con scroll horizontal (`LV_DIR_HOR`) para cambiar de pestañas mediante gestos táctiles.
   - Al contener listas con scroll vertical (`LV_DIR_VER`), el motor de entrada táctil colapsa al intentar procesar ambos scrolls a la vez, congelando el loop principal (`lv_timer_handler()`) en el Core 1.
2. **Escritura Innecesaria en SD al Abrir la Vista:**
   - Intentar guardar archivos en la microSD durante la inicialización de la pantalla provocaba contención en el bus SPI y lentitud innecesaria.

---

## 2. Nueva Arquitectura y Layout Estático (320x480)

### A. Layout Fijo (Cero Scroll Horizontal)

```
┌──────────────────────────────────────────────────────────┐
│ [ < Volver ]            Radio Online            [ 12:00 ] │  <-- HeaderBar estándar
├──────────────────────────────────────────────────────────┤
│ 📻 [ Nombre Emisora / Estado ]                 [ ▶ Play ] │  <-- PlayerBar fijo
├──────────────────────────────────────────────────────────┤
│ [ ⭐ Favoritas (95px) ] [ 🌐 Explorar ] [ ➕ Añadir ]     │  <-- 3 Botones Fijos (Segmented)
├──────────────────────────────────────────────────────────┤
│                                                          │
│  CONTENEDOR ACTIVO (conmutado con LV_OBJ_FLAG_HIDDEN):   │
│                                                          │
│  A) FAVORITAS:                                           │
│     - Lista de emisoras guardadas (scroll VERTICAL)      │
│                                                          │
│  B) EXPLORAR:                                            │
│     - Fila de Tríos con Paginación:                      │
│       [ ◀ ] [ Género A ] [ Género B ] [ Género C ] [ ▶ ] │  <-- Tríos de géneros (ej: Top/Rock/Pop, Chill/Jazz/News, Reggae/Metal/Electro...)
│     - Lista de emisoras de la categoría (scroll VERTICAL)│
│                                                          │
│  C) AÑADIR MANUAL:                                       │
│     - Formulario fijo: Nombre + URL + Botón Guardar      │
│                                                          │
└──────────────────────────────────────────────────────────┘
```

---

## 3. Principios de Funcionamiento

1. **Conmutación Instantánea por Visibilidad:**
   - Se crean 3 contenedores planos: `favContainer`, `exploreContainer`, `addContainer`.
   - Al pulsar un botón superior, se ocultan dos y se muestra el seleccionado usando `lv_obj_add_flag` / `lv_obj_clear_flag` con `LV_OBJ_FLAG_HIDDEN`.
2. **Selector de Géneros por Tríos Paginados (Fila única fija):**
   - Estructura: `[ ◀ ] [ Género 1 ] [ Género 2 ] [ Género 3 ] [ ▶ ]`
   - Ocupa solo 34px de alto, dejando máximo espacio vertical para la lista de emisoras.
   - Tríos incluidos por defecto:
     - **Página 1:** Top, Rock, Pop
     - **Página 2:** Chill, Jazz, Reggae
     - **Página 3:** Metal, Electronic, News
     - **Página 4:** Classical, Latin, Ambient
   - Se puede extender fácilmente a más géneros sin alterar la UI ni requerir scroll horizontal.
3. **Carga Inmediata desde Memoria (0 ms):**
   - Las emisoras predeterminadas para cada categoría y favoritos iniciales provienen directamente de la memoria Flash/RAM del ESP32.
   - Al tocar el botón o cambiar de categoría, la lista se pinta en milisegundos sin tocar la SD.
3. **Escritura en SD Controlada:**
   - La microSD solo se escribe cuando el usuario agrega una emisora en la pestaña "Añadir".
4. **Streaming en Core 0:**
   - La reproducción de audio se delega a `NativeAudioDriver::playStream(url)` en el Core 0.

---

## 4. Archivos Involucrados

- **`src/UI/Views/RadioView.h`**: Definición de contenedores, punteros y métodos de navegación.
- **`src/UI/Views/RadioView.cpp`**: Layout estático de 320px, cuadrícula 3x2 fija y conmutación limpia.
- **`src/UI/Views/Audio/RadioManager.cpp`**: Retorno directo de listas en memoria y persistencia de favoritos.
- **`src/UI/Views/Audio/RadioManager.h`**: Métodos limpios de consulta.

---

## 5. Pasos de Ejecución
1. Actualizar `RadioManager.h` y `RadioManager.cpp` para lectura pura de memoria.
2. Implementar `RadioView.h` y `RadioView.cpp` con la arquitectura estática de 3 botones y contenedores ocultables.
3. Compilar con `pio run -e esp32`.
4. Flashear con `pio run -e esp32 -t upload`.
