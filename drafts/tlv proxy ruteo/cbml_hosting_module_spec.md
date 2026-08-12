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

### 2. Cuota y Límites del Plan de Micro-Hosting
- **20 MB de Almacenamiento en Disco:** Dado que cada pantalla CBML binaria pesa `< 1 KB`, 20 MB permiten alojar hasta **20,000 pantallas / micro-páginas** por usuario.
- **5 MB de RAM Concurrente por Usuario:** Cuota holgada para ejecutar decenas de micro-scripts Lua en paralelo (~32 KB a 64 KB por script).
- **Aislamiento de Directorio (Directory Jailing):** Cada usuario está confinado estrictamente a su carpeta `/content/<usuario_id>/`. El servidor bloquea cualquier intento de navegación fuera de su directorio (`../` path traversal guard).

### 3. Hardware Objetivo del Servidor SBC (Single Board Computer)
- **Nodos Estándar (4 Núcleos, 2 GB a 4 GB RAM):** Capacidad estimada para albergar **entre 400 y 800 usuarios activos simultáneos** por SBC.
- **Nodos Premium (8 Núcleos, 8 GB RAM - e.g. RK3588 / Orange Pi 5 / Rock 5B):** Capacidad masiva para albergar **hasta 1,500+ usuarios activos simultáneos** en una sola plaquita SBC sin saturación de CPU/RAM.

### 4. Separación de Capas (Router vs. Hosting Server)
- **Mesh Router (Transporte L3/L4 Puro - Go):**
  - **100% Agnóstico al Payload:** El ruteador solo inspecciona la cabecera `MeshHeader` (`SrcID`, `DstID`, `ServiceID`). Le es totalmente indiferente el contenido del payload.
  - **Despacho:** Simplemente entrega los bytes de los paquetes con `ServiceID = 0x07` al backend del Servidor de Hosting (puerto `:8766`).
- **Servidor de Hosting / Proxy (Capa de Aplicación L7 - Python/Go Backend):**
  - **Interpretación de URIs:** Este módulo es el que decodifica el payload TLV de la petición, lee la URI solicitada (`/user_123/blog/index.cbml W=240 H=320`), resuelve la ruta dentro del directorio del usuario (`/content/user_123/`), compila el CBML/Lua a TLV binario y devuelve el payload de respuesta al Router para su envío.

### 4. Motor de Micro-Scripting en Sandbox (Lua)
> [!IMPORTANT]
> **Decisión de Arquitectura:** Se utilizará **Lua** como el motor de scripting ultra-ligero y aislado (sandbox) en el servidor/gateway.

- **Consumo Mínimo de Recursos:** Memoria RAM acotada (~32 KB a 50 KB por entorno de ejecución).
- **Tres Capas de Seguridad Infranqueables (Sandboxing):**
  1. **Aislamiento de Entorno (Environment Isolation):** Se eliminan `os.execute`, `os.system`, `io.open` y módulos de disco/red libre. El script no puede leer archivos del servidor ni ejecutar comandos del sistema operativo.
  2. **Cuota Estricta de RAM (Memory Allocator Limit):** Cada máquina virtual de Lua tiene asignado un límite de RAM rígido (ej. máx 64 KB). Si el script intenta desbordar la memoria, la asignación se cancela de inmediato sin afectar la RAM del servidor.
  3. **Protección Anti-Bucle Infinito (CPU Instruction Hook):** Se utiliza `lua_sethook` para limitar la ejecución a un máximo de 10,000 instrucciones de reloj. Si se detecta un bucle `while true do`, el motor aborta el script por timeout en milisegundos, protegiendo la CPU del servidor.
- **API Segura Exportada:** Únicamente se exponen funciones aisladas del ecosistema Mesh:
  - `mesh.send_gpio(node_id, pin, state)`
  - `mesh.send_command(node_id, cmd)`
  - `ui.update_value(elem_id, value)`
  - `timer.delay(ms)`
- **Modo No-Code Complementario:** El editor visual generará automáticamente scripts simples `.lua` desde una lista desplegable de acciones para usuarios no programadores.

---

## 🛠️ Estructura Futura de Componentes

Cuando se unan las piezas del hosting, la estructura en `gateway/tlvgl/` se organizará de la siguiente manera:

```text
gateway/tlvgl/
├── content/                     # Almacenamiento de usuarios (Cuota 20MB por usuario)
│   ├── usuario_01/
│   │   ├── index.cbml           # Código fuente CBML/HTML
│   │   ├── index.bin            # Bytecode TLVGL precompilado
│   │   ├── logic.lua            # Micro-script de lógica en Lua (Sandbox)
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
4. **Catálogo de Acciones en Editor:** Implementar selector de acciones predefinidas (Mesh/GPIO/Webhook) en el editor visual ([`tlvgl_preview.html`](file:///home/kaber420/Documentos/proyectos/espOS32/gateway/tlvgl/tlvgl_preview.html)).
5. **Botonera de Publicación Directa:** Agregar un botón de **"Publicar a mi espacio"** en el editor visual.
