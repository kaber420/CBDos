# 📟 Especificación Técnica: Subsistema Serial de CBDos, Control de Mochilas y Gestión Dinámica de Puertos

**Versión:** 2.0.0 (RFC-CBDOS-SERIAL-SYSTEM)  
**Estado:** Aprobado / Especificación de Arquitectura  
**Target:** ESP32-P4 (JC4880P443C) / ESP32-S3 (JC3248W535)  
**Módulos Afectados:** `core/src/ui/views/SerialTerminalView.*`, `core/include/cbdos/serial.hpp`, `bsp/esp32_p4_jc4880/hal/*`, `bsp/esp32_s3_jc3248/hal/*`

---

## 📌 1. Motivación y Diagnóstico de Fallas Previas

La implementación anterior de la aplicación `SerialTerminalView` y el backend UART presentaba limitaciones estructurales que no correspondían a un sistema operativo moderno:

1. **Auto-inicialización a Ciegas (`Blind Auto-Init`):**
   - Al entrar a la aplicación, se ejecutaba de forma automática la inicialización del hardware en pines no verificados y se iniciaba un temporizador de sondeo periódico (`lv_timer`) a 25 ms sin autorización explícita del usuario, desperdiciando CPU y energía.
2. **Presets Artificiales y Opciones Fantasma:**
   - Se mostraban opciones arbitrarias como `"JP1 Alt 1 (TX:50 RX:49)"` o puertos USB fijos en pantalla incluso cuando no había ningún periférico conectado.
3. **Líneas Flotantes y Ruido Parásito:**
   - La falta de pull-up interno en RX provocaba que, al abrir la UART sin carga conectada, el pin captara ruido ambiental, saturando el buffer con ráfagas infinitas de bytes corruptos y *framing errors*.
4. **Colapso del Renderizador de LVGL 9:**
   - Se volcaba el buffer completo mediante `lv_textarea_set_text()`, forzando a LVGL a recalcular layout de kilobytes de texto decenas de veces por segundo. Además, bytes binarios corruptos bloqueaban el decodificador de fuentes tipográficas.
5. **Falta de Abstracción Multi-Puerto:**
   - No se trataba al puerto USB y a los puertos UART físicos bajo una misma abstracción unificada de flujo serie bidireccional (*Stream*), impidiendo la alternancia limpia entre ellos.

---

## 🎯 2. Principios de Diseño del Subsistema Serial

```
                             ┌─────────────────────────────────────────────────────────┐
                             │           SerialTerminalView (UI LVGL 9.5)              │
                             │  - Selector de Puerto Dinámico (Solo puertos reales)    │
                             │  - Selector Baudrate: [9600 ... 115200 ... 921600]     │
                             │  - Botón Conectar / Desconectar (Control manual)        │
                             │  - Control GPIO 34 (Pulso Reset / Nivel lógico)         │
                             └────────────────────────────┬────────────────────────────┘
                                                          │ Control Explícito
                                                          ▼
                             ┌─────────────────────────────────────────────────────────┐
                             │                 ISerialPort (Core HAL)                  │
                             │  - open(config) / close()                               │
                             │  - read(buf, len) / write(buf, len)                     │
                             │  - setControlPin(level)                                 │
                             │  - onDataReceivedCallback                               │
                             └────────────────────────────┬────────────────────────────┘
                                                          │
                                     ┌────────────────────┴────────────────────┐
                                     ▼                                         ▼
                 ┌───────────────────────────────────────┐ ┌───────────────────────────────────────┐
                 │          UartHardwarePort             │ │            UsbCdcPort                 │
                 │   - ESP32-P4: TX:32, RX:28, Ctrl:34   │ │   - Detección Hotplug por hardware    │
                 │   - ESP32-S3: TX:15, RX:16            │ │   - Aparece SOLO si está enchufado    │
                 │   - Pull-up interno en RX             │ │   - Se retira al desconectar          │
                 │   - Modo Manual (pines libres)        │ │   - RingBuffer DMA                    │
                 └───────────────────────────────────────┘ └───────────────────────────────────────┘
```

### 2.1. Cero Auto-Arranque y Control Manual Obligatorio
- **Reposo Inicial:** Al abrir la vista `SerialTerminalView`, ningún periférico se reserva, polariza ni inicializa.
- **Acción Explícita:** La conexión se establece **únicamente** cuando el usuario selecciona los parámetros y presiona **"Conectar"**.
- **Desconexión Limpia:** Al presionar **"Desconectar"** (o al salir de la aplicación), se ejecuta inmediatamente `close()`, liberando los GPIOs del chip y deteniendo cualquier recepción de fondo para evitar consumo parásito.

### 2.2. Detección Dinámica por Hardware (Hotplug USB CDC Real)
- **Cero puertos USB fantasma:** El puerto USB CDC **NO está hardcodeado** en la lista si no hay nada conectado.
- **Conexión en Caliente:** Cuando el usuario enchufa un módem o microcontrolador al conector USB Host (USB 2), el stack hardware emite un evento nativo (`CDC_CONNECTED`). En ese instante, `USB CDC` aparece disponible en el selector de puertos.
- **Desconexión en Caliente:** Si el dispositivo USB se desconecta, el stack genera el evento de desconexión, la terminal cierra el canal automáticamente y retira la opción de la interfaz.

### 2.3. Puertos Físicos y Pines Reales Soportados

El sistema arranca con los puertos y pines reales ya probados:

#### Target ESP32-P4 (JC4880P443C)
| Puerto en UI | TX Pin | RX Pin | Pin Control Aux | Comportamiento |
| :--- | :--- | :--- | :--- | :--- |
| **UART JP1 (Mochila / C6)** | **GPIO 32** | **GPIO 28** | **GPIO 34** | Siempre disponible. Hardware probado. Pull-up en RX. |
| **USB CDC** | USB D+ / D- | USB D+ / D- | N/A | **Dinámico:** Solo visible si hay dispositivo CDC conectado. |
| **Manual / Custom** | Configurable | Configurable | Opcional | Permite ingresar pines TX/RX libres reales. |

#### Target ESP32-S3 (JC3248W535)
| Puerto en UI | TX Pin | RX Pin | Pin Control Aux | Comportamiento |
| :--- | :--- | :--- | :--- | :--- |
| **UART Externa** | **GPIO 15** | **GPIO 16** | N/A | Siempre disponible. Hardware probado. |
| **USB CDC** | USB D+ / D- | USB D+ / D- | N/A | **Dinámico:** Solo visible si hay dispositivo CDC conectado. |
| **Manual / Custom** | Configurable | Configurable | N/A | Permite seleccionar pines libres del S3. |

### 2.4. Control de Línea Auxiliar (GPIO 34 en ESP32-P4)
En la cabecera JP1, el **GPIO 34** está verificado y disponible junto al puerto serie:
- La interfaz de la terminal incluye controles dedicados para este pin:
  - **Botón "Pulso Reset":** Envía un pulso en nivel bajo (`LOW` durante 100 ms) y retorna a nivel alto (`HIGH`). Permite reiniciar microcontroladores o mochilas conectadas sin tocar cables.
  - **Toggle Nivel (HIGH / LOW):** Permite fijar el estado lógico del pin para colocar módulos en modo bootloader o activar habilitadores de energía.
- En placas sin pin de control (ej. S3), este widget se oculta automáticamente.

### 2.5. Blindaje de Hardware (Pull-Up en RX)
- Al abrir cualquier puerto UART sobre un pin RX físico, el driver activa la resistencia interna de pull-up (`gpio_pullup_en(rxPin)`).
- Esto mantiene la línea en nivel lógico alto (`IDLE`) en caso de que la mochila conectada esté apagada o en reposo, erradicando *framing errors* y tormentas de bytes parásitos.

### 2.6. Principio Reactivo (Cero Polling Innecesario - Regla 12)
- Se erradica el temporizador ciego de 25 ms.
- El backend serie opera mediante eventos de recepción (interrupción/cola FreeRTOS o callback de recepción).
- La UI se actualiza únicamente cuando existen nuevos datos por renderizar (`m_hasNewData = true`), manteniendo el procesador en reposo durante periodos de silencio.

### 2.7. Renderizado Incremental y Filtro de Bytes
- Se utiliza exclusivamente `lv_textarea_add_text()`, insertando bloques de texto al final del buffer visual sin recalcular el documento completo.
- Sanitización obligatoria antes de pasar a LVGL:
  - Caracteres ASCII válidos (`0x20` a `0x7E`), tabuladores, saltos de línea (`\r`, `\n`).
  - Caracteres binarios o secuencias UTF-8 truncadas se descartan o se formatean en modo hexadecimal seguro para evitar corrupciones en el motor de tipografías.

---

## 📋 3. Especificación de la Interfaz de Usuario (UI)

### 3.1. Barra de Herramientas
1. **Selector de Puerto (Dropdown Dinámico):**
   - Muestra exclusivamente los puertos físicos válidos reportados por el BSP (`UART JP1`, `USB CDC` solo si está conectado, o `Manual`).
2. **Selector de Baudrate:**
   - Opciones: `9600`, `19200`, `38400`, `57600`, `115200`, `230400`, `460800`, `921600`.
3. **Botón Conectar / Desconectar:**
   - **Desconectado:** Verde / Etiqueta "Conectar" (`LV_SYMBOL_PLAY`).
   - **Conectado:** Rojo / Etiqueta "Desconectar" (`LV_SYMBOL_STOP`).
4. **Controles de Línea Auxiliar (GPIO 34 en P4):**
   - Botón `[ ⚡ Reset / Pulso ]`: Genera pulso LOW de 100 ms en GPIO 34.
   - Botón `[ Pin 34: H/L ]`: Alterna el estado estático de la línea.
5. **Botón Pausa (Hold):** Congela el scroll y renderizado sin cerrar el canal de comunicación.
6. **Botón Limpiar (Clear):** Vacía el área visual de la terminal.
7. **Botón Guardar en SD:** Vuelca el buffer actual a `/sdcard/logs/serial_terminal_YYYYMMDD_HHMMSS.txt`.

### 3.2. Ciclo de Vida del Puerto Serie
```cpp
void onConnectClicked() {
    if (!m_isConnected) {
        SerialConfig cfg;
        cfg.portId = getSelectedPortId();
        cfg.baudrate = getSelectedBaudrate();
        
        if (cfg.portId == "manual") {
            cfg.txPin = getManualTxPin();
            cfg.rxPin = getManualRxPin();
        }

        if (m_serialBackend->open(cfg)) {
            m_isConnected = true;
            updateUiConnectedState(true);
            lv_textarea_add_text(m_taTerminal, "[SYS] Puerto serie conectado.\n");
        } else {
            lv_textarea_add_text(m_taTerminal, "[ERR] Error al abrir puerto serie.\n");
        }
    } else {
        m_serialBackend->close();
        m_isConnected = false;
        updateUiConnectedState(false);
        lv_textarea_add_text(m_taTerminal, "[SYS] Puerto cerrado. Hardware liberado.\n");
    }
}
```

---

## 💻 4. Contratos HAL (Core C++ Puro)

### 4.1. Descriptores de Puerto (`core/include/cbdos/serial.hpp`)
```cpp
namespace cbdos {
namespace serial {

enum class PortType {
    HardwareUart,
    UsbCdcAcm,
    ManualUart
};

struct SerialPortDescriptor {
    std::string id;            // "jp1", "usb0", "ext_s3", "manual"
    std::string displayName;   // "UART JP1 (TX:32 RX:28)", "USB CDC-ACM"
    PortType type;
    int defaultTx;             // -1 para USB
    int defaultRx;             // -1 para USB
    int controlPin;            // 34 en JP1 P4, -1 si no aplica
    bool isAvailable;          // true para UART, dinámico para USB
};

class ISerialPort {
public:
    virtual ~ISerialPort() = default;
    virtual bool open(const SerialConfig& config) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    virtual size_t available() = 0;
    virtual size_t read(uint8_t* buffer, size_t maxLen) = 0;
    virtual size_t write(const uint8_t* data, size_t len) = 0;
    virtual void flush() = 0;
    virtual bool setControlPin(bool level) = 0;
    virtual bool pulseControlPin(uint32_t durationMs) = 0;
};

class ISerialBackend {
public:
    virtual ~ISerialBackend() = default;
    virtual std::vector<SerialPortDescriptor> getAvailablePorts() = 0;
    virtual ISerialPort* getPort(const std::string& portId) = 0;
    virtual void setHotplugCallback(std::function<void(bool connected, const std::string& portId)> cb) = 0;
};

} // namespace serial
} // namespace cbdos
```

---

## 🛠️ 5. Plan de Implementación por Fases

1. **Fase 1: Desacoplamiento y Descriptores en Core**
   - Definir `ISerialPort` y `SerialPortDescriptor` en `core/include/cbdos/serial.hpp`.
   - Limpiar `SerialTerminalView` eliminando auto-inicialización y temporizador ciego a 25 ms.
   - Implementar el estado `m_isConnected` y poblar el desplegable únicamente con `getAvailablePorts()`.

2. **Fase 2: Backend Hardware ESP32-P4 (`bsp/esp32_p4_jc4880`)**
   - Declarar el puerto `UART JP1` con pines reales: TX 32, RX 28, y soporte de control en GPIO 34.
   - Implementar la activación de Pull-Up en el pin RX dentro de `open()`.
   - Implementar `setControlPin()` y `pulseControlPin()` para GPIO 34.
   - Conectar el callback de inserción/extracción USB CDC de `usb_host` para agregar o remover dinámicamente `USB CDC` de los puertos disponibles.

3. **Fase 3: Backend Hardware ESP32-S3 (`bsp/esp32_s3_jc3248`)**
   - Declarar el puerto `UART Externa` con TX 15 y RX 16.
   - Habilitar soporte USB CDC dinámico si el target lo enumera.
   - Asegurar compilación limpia sin advertencias en PlatformIO.

4. **Fase 4: Verificación y Validación**
   - Verificar que al entrar a la terminal los pines no se modifican hasta presionar "Conectar".
   - Conectar y desconectar un dispositivo USB y comprobar que la lista de puertos en pantalla se actualiza en caliente sin polling.
   - Enviar pulso de reset sobre GPIO 34 y verificar el reinicio del coprocesador C6 o mochila conectada.
