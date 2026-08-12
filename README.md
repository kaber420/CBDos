# espOS32 — Ecosistema Mesh & Navegador TLVGL / CBML

**espOS32** es una plataforma completa de micro-hosting, ruteo Mesh y navegador binario para microcontroladores ESP32-S3 con pantalla táctil (AMOLED/LCD QSPI). 

Permite maquetar y servir aplicaciones e interfaces gráficas dinámicas usando un lenguaje declarativo súper ligero (**CBML / PseudoHTML**) que se compila en tiempo real a bytecode binario **TLVGL**, ofreciendo experiencias aceleradas por hardware a 60 FPS sobre redes de radio de bajo ancho de banda.

---

## 🎯 Arquitectura General

```text
[ Usuario / Diseñador ] ──(HTML/CBML)──> [ Editor Visual Alternet Studio ]
                                                    │
                                                    ▼ (Compilador CBML -> TLV)
[ Nodo ESP32 Touch ] <──(Red Mesh / Radio)── [ Router Mesh Go ] <── [ Servidor de Hosting ]
```

---

## 📁 Estructura del Proyecto

* **`firmware/`**: Código fuente C/C++ para la placa ESP32-S3 (PlatformIO).
  * `src/main.cpp`: Inicialización de pantalla, touch, PSRAM OPI (8MB) y bucle principal.
  * `src/Core/tlv_parser.c`: Decodificador binario TLVGL y renderizador nativo de LVGL v9 (`lv_chart`, `lv_btn`, `lv_textarea`, `lv_label`, etc.).
  * `include/lv_conf.h`: Configuración de memoria dinámica custom asignada a PSRAM.
* **`gateway/tlvgl/`**: Servidor de Aplicaciones y Compilador.
  * `tlvgl_server.py`: Servidor de Hosting TCP/Mesh (Puerto `:8766`).
  * `tlvgl_compiler.py`: Compilador CBML/HTML a Bytecode TLV binario.
  * `tlvgl_preview.html` & `editor.js`: Editor Web WYSIWYG en vivo.
  * `content/`: Páginas HTML/CBML de usuario (`clima.html`, `noticias.html`, `index.html`).
* **`gateway/router-go/`**: Ruteador de red Mesh escrito en Go (Capa de Transporte L3/L4 pura agnóstica al payload).
* **`drafts/`**: Especificaciones técnicas, RFCs del protocolo de ruteo y planes de diseño.

---

## 🚀 Comandos Rápidos

### 1. Firmware ESP32-S3
```bash
# Compilar Firmware
cd firmware
pio run -e esp32

# Flashear a la Placa (ESP32-S3)
cd firmware
pio run -e esp32 -t upload

# Monitorear Puerto Serie
cd firmware
pio device monitor -b 115200 --filter esp32_exception_decoder
```

### 2. Servidores y Gateway
```bash
# Iniciar Servidor de Hosting (Python)
python3 gateway/tlvgl/tlvgl_server.py --port 8766

# Iniciar Router Mesh (Go)
./gateway/router-go/build/router -tcp :8765 -hosting 127.0.0.1:8766

# Iniciar Servidor de Previsualización Web
python3 gateway/tlvgl/tlvgl_preview_server.py --port 8766
```

---

## 📊 Tabla de Componentes PseudoHTML / TLVGL

| Tag Hex | Etiqueta CBML | Widget LVGL Nativo | Descripción |
| :---: | :--- | :--- | :--- |
| `0x10` | `<body>` / `PAGE` | `lv_screen` | Contenedor de pantalla principal y color de fondo. |
| `0x11` | `<h1>`, `<h2>`, `<p>` | `lv_label` | Etiquetas de texto con estilos e iconos FontAwesome (`<i>`). |
| `0x12` | `<a>`, `<button>` | `lv_btn` | Botones de enlace o acciones interactivas. |
| `0x13` | `<input>` | `lv_textarea` | Campos de texto interactivos con teclado en pantalla. |
| `0x1A` | `<div>` / `<panel>` | `lv_obj` | Paneles estilo Bento / tarjetas de UI. |
| `0x1B` | `<chart>` | `lv_chart` | Gráficas de líneas y barras con auto-escalado dinámico de datos. |

---

## 📄 Licencia
Proyecto desarrollado para el ecosistema **espOS32**.
