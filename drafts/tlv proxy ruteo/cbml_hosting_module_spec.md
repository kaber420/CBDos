# Especificación de Diseño: Módulo de Hosting CBML & Editor Web

## 📌 Visión General

**CBML** (*Code Byte Markup Language*) es un subconjunto simplificado de HTML/CSS (`<div>`, `<h1>`, `<p>`, `<button>`, `<i>` FontAwesome) optimizado para tener **0 curva de aprendizaje**. 

Este lenguaje se compila en tiempo real a **bytecode TLVGL binario** ultra-compacto (200 Bytes - 2 KB por pantalla), permitiendo ofrecer un servicio de **micro-hosting gratuito de 20 MB** donde los usuarios pueden crear, diseñar y alojar sus propios sitios web, blogs, dashboards o paneles de control para la red ESP32 / Mesh.

---

## 🎯 Arquitectura General

```text
[ Usuario / Diseñador ]
         │
         ▼ (HTML/CBML en Vivo)
┌─────────────────────────────────────────────────────────┐
│ Alternet Studio (Editor Web visual / WYSIWYG)          │
└────────────────────────┬────────────────────────────────┘
                         │
                         ▼ (POST /compile)
┌─────────────────────────────────────────────────────────┐
│ Compilador CBML -> TLVGL Bytecode (tlvgl_compiler.py)   │
└────────────────────────┬────────────────────────────────┘
                         │
                         ▼ (Guardado en cuota de 20MB)
┌─────────────────────────────────────────────────────────┐
│ Módulo de Hosting (content/<user_id>/)                  │
└────────────────────────┬────────────────────────────────┘
                         │
                         ▼ (Solicitudes de Red / Mesh)
┌─────────────────────────────────────────────────────────┐
│ Gateway / Mesh Router (Ruteo de paquetes a Nodos ESP32) │
└─────────────────────────────────────────────────────────┘
```

---

## 💡 Conceptos Clave

### 1. CBML (Code Byte Markup Language)
- **Formato:** HTML declarativo simplificado con estilos inline o posicinamiento en grid/bento.
- **Ventaja:** Cero curva de aprendizaje. Cualquier persona que sepa etiquetas HTML básicas o use el editor drag-and-drop puede maquetar interfaces para pantallas de ESP32-S3.
- **Tamaño de salida:** Mientras una página web tradicional pesa megabytes, un archivo binario `.bin` generado desde CBML pesa típicamente **< 1 KB**.

### 2. Cuota de Micro-Hosting (20 MB Gratis)
- Dado el peso minúsculo de las páginas compiladas en binario:
  - 1 Pantalla CBML compilada ≈ 1 KB
  - **20 MB de almacenamiento** = Capacidad para alojar hasta **20,000 pantallas / micro-páginas por usuario**.
- Permite ofrecer planes gratuitos altamente eficientes sin saturar el servidor ni el almacenamiento del Gateway.

### 3. Integración con el Sistema de Ruteo (Paso Previo Requerido)
> [!IMPORTANT]
> **Prioridad Actual:** Antes de unir las piezas del hosting, se debe finalizar la capa de **ruteo** (Gateway Mesh Router).

- **Direccionamiento de Rutas:** El ruteador interpretará peticiones con formato URI (ej. `GET /user_123/blog/index.cbml W=240 H=320`).
- **Despacho Dinámico:** El router consultará el directorio de hosting del usuario, compilará/servirá el binario TLV correspondiente y lo despachará al nodo ESP32 solicitante mediante el transporte Mesh (Socket / Serial / Lora / Wi-Fi).

---

## 🛠️ Estructura Futura de Componentes

Cuando se unan las piezas del hosting, la estructura en `gateway/tlvgl/` se organizará de la siguiente manera:

```text
gateway/tlvgl/
├── content/                     # Almacenamiento de usuarios (Cuota 20MB por usuario)
│   ├── usuario_01/
│   │   ├── index.cbml           # Código fuente CBML/HTML
│   │   ├── index.bin            # Bytecode TLVGL precompilado
│   │   └── blog/                # Entradas del micro-blog
│   └── usuario_02/
├── tlvgl_compiler.py            # Motor de compilación CBML -> TLV
├── tlvgl_preview_server.py      # Servidor de previsualización en vivo (Puerto 8766)
├── hosting_manager.py           # [FUTURO] Gestión de usuarios, cuotas de 20MB y auth
└── editor/                      # Editor Web Alternet Studio (HTML/JS/CSS)
```

---

## 📋 Lista de Tareas Futuras (Roadmap)

1. **[En Proceso] Sistema de Ruteo:** Finalizar la especificación e implementación del proxy y ruteador de mensajes (`gateway_router.py` / Mesh Router).
2. **Mapeo de Rutas de Hosting:** Conectar el ruteador para que resuelva rutas del tipo `/usuario/sitio/pantalla`.
3. **Módulo de Cuotas (20MB):** Implementar validación de espacio de disco por directorio de usuario antes de guardar cambios en `content/`.
4. **Botonera de Publicación Directa:** Agregar un botón de **"Publicar a mi espacio"** en el editor visual ([`tlvgl_preview.html`](file:///home/kaber420/Documentos/proyectos/espOS32/gateway/tlvgl/tlvgl_preview.html)).
