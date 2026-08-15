# Borrador de Ajustes de Layout: Radio Online (espOS32)

## 1. Problemas Identificados en la UI Actual
1. **Scroll Horizontal Innecesario en Barras de Botones:**
   - La fila de pestañas principales y la fila de tríos de géneros (`trioRow`) tienen activada por defecto la bandera de scroll de LVGL (`LV_OBJ_FLAG_SCROLLABLE`).
   - Los anchos de los botones no dejaban suficiente margen respecto al ancho de pantalla (320px), activando desplazamiento horizontal al tocarlos.
2. **Doble Scroll Vertical (Reproductor + Lista):**
   - El contenedor principal de la pantalla (`mainCont` y `exploreContainer`) permitía desplazamiento vertical, haciendo que al deslizar la lista de emisoras se moviese también la barra del reproductor y el HeaderBar, ocultando los controles superiores.

---

## 2. Propuesta de Arquitectura de Layout Fijo (Resolución 320x480)

### Distribución Vertical en Pantalla Completa (Total: 480px)

```
┌──────────────────────────────────────────────────────────┐  ───
│ [ < Volver ]            Radio Online            [ 12:00 ] │   44px  (HeaderBar - FIJO, disableScroll)
├──────────────────────────────────────────────────────────┤  ───
│ 📻 [ Nombre Emisora / Estado ]                 [ ▶ Play ] │   58px  (PlayerBar - FIJO, disableScroll)
├──────────────────────────────────────────────────────────┤  ───
│ [ ⭐ Favoritas (94px) ] [ 🌐 Explorar ] [ ➕ Añadir ]     │   34px  (SegmentedNav - FIJO, disableScroll)
├──────────────────────────────────────────────────────────┤  ───
│                                                          │
│  ZONA DINÁMICA SEGÚN PESTAÑA (334px restantes):          │
│                                                          │
│  A) FAVORITAS:                                           │
│     - favContainer (334px, disableScroll)                │
│       └─ favList: ÚNICO componente con scroll vertical   │
│                                                          │
│  B) EXPLORAR:                                            │
│     - exploreContainer (334px, disableScroll)            │
│       ├─ trioRow (32px, FIJO, disableScroll):            │
│       │  [ ◀ 28px ] [ G1 70px ] [ G2 70px ] [ G3 70px ] [ ▶ 28px ]  (Total: 266px en 312px)
│       └─ exploreList (290px): ÚNICO componente con       │
│          scroll vertical                                 │
│                                                          │
│  C) AÑADIR:                                              │
│     - addContainer (334px, FIJO, disableScroll):         │
│       Formulario Nombre + URL + Género + Guardar         │
│                                                          │
└──────────────────────────────────────────────────────────┘  ─── Total: 480px
```

---

## 3. Especificaciones Técnicas y Dimensiones Exactas

### A. Eliminación de Scrolls Parásitos
Para garantizar que **NADA** se desplace excepto la lista de canciones:
1. `DefaultTheme::disableScroll(mainCont);`
2. `DefaultTheme::disableScroll(playerBar);`
3. `DefaultTheme::disableScroll(navRow);`
4. `DefaultTheme::disableScroll(trioRow);`
5. `DefaultTheme::disableScroll(exploreContainer);`
6. `DefaultTheme::disableScroll(addContainer);`

### B. Ajuste de Anchos en Pestañas Superiores (navRow)
- **Ancho Disponible:** 312px (320px - 8px de márgenes laterales).
- **3 Botones:** 94px de ancho cada uno (94 × 3 = 282px).
- **Espaciado:** 2 gaps de 15px = 30px.
- **Suma:** 282px + 30px = **312px exactos**. Cero scroll horizontal.

### C. Ajuste de Anchos en Tríos de Géneros (trioRow)
- **Ancho Disponible:** 312px.
- **Botón Anterior [ ◀ ]:** 28px.
- **3 Botones de Género:** 70px cada uno (70 × 3 = 210px).
- **Botón Siguiente [ ▶ ]:** 28px.
- **Espaciado:** 4 gaps de 6px = 24px.
- **Suma:** 28 + 210 + 28 + 24 = **290px** (22px de margen de seguridad). Cero scroll horizontal.

### D. Lista de Emisoras con Scroll Vertical Único
- `favList` y `exploreList` serán los **únicos** objetos con la bandera:
  ```cpp
  lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(list, LV_DIR_VER);
  ```
- Al deslizar el dedo en la lista, las emisoras se mueven con total suavidad mientras el reproductor, el botón Play y las pestañas permanecen anclados y visibles arriba en todo momento.

---

## 4. Archivos a Modificar
- `src/UI/Views/RadioView.cpp`: Aplicar los anchos de 94px y 70px, y desactivar el scroll en todos los contenedores padre.
