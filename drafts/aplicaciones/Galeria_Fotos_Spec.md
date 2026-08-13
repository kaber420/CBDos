# Especificación: Galería de Fotos

Documento de especificación de la Galería de Fotos pura (sin residuos de la app de restaurante).

## 1. Estructura de Datos Limpia (`GalleryItem`)
- `id`: Identificador único de la foto.
- `name`: Nombre de la foto (sin campos comerciales ni precios).
- `path`: Ruta del archivo en la memoria/tarjeta SD.

## 2. Ajustes de la Barra de Cabecera (HeaderBar)
- **Botón Volver:** Posicionado a la izquierda (`84px` de ancho).
- **Posición del Nombre:** Inicia **después del botón Volver** (offset `~100px` desde la izquierda) para que **NUNCA** se empalme ni quede encima del botón.
- **Marquesina Circular:** Activa si el nombre no cabe en el área restante.
- **Iconos:** Ocultar completamente Wi-Fi y Reloj/Hora.

## 3. Visor de Imagen a Tamaño Completo con Desplazamiento (Scroll)
- Cobertura a pantalla completa con scroll multidireccional habilitado (`LV_OBJ_FLAG_SCROLLABLE`, `LV_DIR_ALL`).
- **Visualización a Tamaño Real:** La imagen se despliega en sus dimensiones reales (`LV_SIZE_CONTENT`). Si supera las dimensiones de la pantalla, el contenedor permite desplazarse libremente (pan/scroll) horizontal y verticalmente para navegar por toda la imagen.
