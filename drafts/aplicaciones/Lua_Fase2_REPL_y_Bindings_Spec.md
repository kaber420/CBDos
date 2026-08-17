# 📜 Borrador Detallado: Fase 2 - Hardware Bindings y Consola Interactiva (REPL) en CBDos

## 🎯 1. Objetivos de la Fase 2
La Fase 2 conecta el motor de **Lua 5.4** recién integrado con el hardware real del ESP32-S3 y habilita una **consola interactiva en vivo (REPL)** para que el usuario pueda escribir código y controlar el dispositivo al instante.

---

## 🏗️ 2. Arquitectura de Componentes

```mermaid
graph TD
    subgraph Entrada_Salida [Entrada / Salida]
        SerialPort[Puerto Serie USB CDC / UART]
        UIRunner[Futura App Terminal en Pantalla]
    end

    subgraph REPL_Module [Módulo LuaREPL]
        LineBuffer[Buffer de Entrada de Línea]
        AutoPrint[Auto-evaluador de expresiones: '2+2' -> 'print(2+2)']
        CommandParser[Comandos internos: .help, .mem, .reset]
    end

    subgraph Lua_Engine [Lua 5.4 Core]
        Engine[LuaEngine: lua_State]
    end

    subgraph Bridge_Module [Módulo LuaBridge: cbdos.*]
        AudioAPI[Audio: beep, play_wav, stop_audio]
        SystemAPI[Sistema: delay, millis, free_mem, battery]
        GPIOAPI[Hardware: pinMode, digitalWrite, digitalRead]
        FSAPI[Archivos: read_file, write_file, exists]
    end

    subgraph Drivers_ESP32 [Drivers de Hardware]
        AudioDrv[NativeAudioDriver]
        SysDrv[ESP-IDF / Arduino HAL]
        SDDrv[StorageManager / SDCard]
    end

    SerialPort <-->|Líneas de texto / Respuestas| LineBuffer
    UIRunner -.->|Comandos desde UI| LineBuffer
    LineBuffer --> CommandParser
    CommandParser --> AutoPrint
    AutoPrint --> Engine
    
    Engine --> Bridge_Module
    AudioAPI --> AudioDrv
    SystemAPI --> SysDrv
    GPIOAPI --> SysDrv
    FSAPI --> SDDrv
```

---

## 🔌 3. Catálogo de Funciones de Hardware (`cbdos.*`)

Todas las funciones estarán agrupadas dentro del objeto global `cbdos` en Lua:

### A. Módulo de Audio (`cbdos.audio`)
| Función en Lua | Parámetros | Descripción |
| :--- | :--- | :--- |
| `cbdos.beep(freq, ms)` | `freq` (Hz), `ms` (duración) | Emite un tono cuadrado/senoidal en el altavoz nativo vía `NativeAudioDriver`. |
| `cbdos.play_wav(path)` | `path` (string, ej: `"/sd/beep.wav"`) | Reproduce un archivo de audio WAV en segundo plano. |
| `cbdos.stop_audio()` | Ninguno | Detiene inmediatamente cualquier sonido en reproducción. |
| `cbdos.set_volume(vol)` | `vol` (0 - 100) | Ajusta el volumen del amplificador I2S. |

### B. Módulo de Sistema e Información (`cbdos.sys`)
| Función en Lua | Retorno | Descripción |
| :--- | :--- | :--- |
| `cbdos.delay(ms)` | Ninguno | Pausa la ejecución del script sin bloquear el resto del sistema (FreeRTOS delay). |
| `cbdos.millis()` | `integer` | Milisegundos transcurridos desde que se encendió el dispositivo. |
| `cbdos.free_psram()` | `integer` | Bytes libres disponibles en la memoria PSRAM (8 MB). |
| `cbdos.free_heap()` | `integer` | Bytes libres en la memoria interna SRAM. |
| `cbdos.get_battery()` | `integer` (0-100) | Porcentaje estimado de la batería. |

### C. Módulo de Pines y Hardware (`cbdos.gpio`)
| Función en Lua | Parámetros | Descripción |
| :--- | :--- | :--- |
| `cbdos.pin_mode(pin, mode)` | `pin` (número), `mode` (`"input"`, `"output"`, `"pullup"`) | Configura el modo de un pin GPIO del ESP32. |
| `cbdos.digital_write(pin, val)` | `pin`, `val` (`0` o `1` / `true` o `false`) | Establece nivel bajo o alto en el pin. |
| `cbdos.digital_read(pin)` | `pin` ➔ Retorna `0` o `1` | Lee el estado lógico de un pin. |
| `cbdos.analog_read(pin)` | `pin` ➔ Retorna `0 - 4095` | Lee el valor analógico del ADC de 12 bits. |

### D. Módulo de Archivos y SD (`cbdos.fs`)
| Función en Lua | Parámetros / Retorno | Descripción |
| :--- | :--- | :--- |
| `cbdos.read_file(path)` | `path` ➔ Retorna `string` o `nil` | Lee el contenido completo de un archivo de texto en la SD/Flash. |
| `cbdos.write_file(path, data)` | `path`, `data` ➔ Retorna `bool` | Guarda o sobrescribe texto en un archivo de la SD. |
| `cbdos.file_exists(path)` | `path` ➔ Retorna `bool` | Comprueba si un archivo o directorio existe. |

---

## 💻 4. Diseño del Módulo REPL (`LuaREPL`)

El REPL escucha de forma **no-bloqueante** en el puerto serie (`Serial` / USB CDC) dentro del ciclo principal del firmware o en una tarea auxiliar.

### Flujo de interacción:
1. Al iniciar o enviar un comando, el REPL muestra el prompt: `lua> `
2. **Auto-detección de Expresiones:**
   * Si el usuario escribe una expresión como `25 * 4`, el REPL detecta que no es una sentencia completa e intenta ejecutar internamente `print(25 * 4)`, mostrando:
     ```text
     lua> 25 * 4
     100
     ```
3. **Comandos Especiales de Sistema (`.`):**
   * `.help`: Muestra la lista de funciones disponibles de `cbdos`.
   * `.mem`: Muestra el uso actual de memoria del recolector de basura de Lua y de la PSRAM.
   * `.clear`: Limpia la pantalla de la terminal.
   * `.reset`: Reinicia el estado de Lua (`lua_close` + nuevo `init`), limpiando variables globales.
   * `.run <archivo.lua>`: Ejecuta directamente un script guardado en la SD.

---

## 🧪 5. Ejemplos de Sesiones Interactivas en el REPL

### Ejemplo 1: Prueba de Audio y Matemáticas
```text
CBDos Lua 5.4.7 REPL
Escribe '.help' para ver los comandos disponibles.
lua> a = 440
lua> cbdos.beep(a, 200)
[Lua] Emitiendo tono 440 Hz por 200 ms...
lua> cbdos.beep(a * 2, 200)
[Lua] Emitiendo tono 880 Hz por 200 ms...
```

### Ejemplo 2: Lectura de Sensores y Sistema en Bucle
```text
lua> for i=1, 3 do print("Batería: " .. cbdos.get_battery() .. "% | PSRAM libre: " .. cbdos.free_psram() .. " bytes") cbdos.delay(1000) end
Batería: 88% | PSRAM libre: 7856128 bytes
Batería: 88% | PSRAM libre: 7856128 bytes
Batería: 88% | PSRAM libre: 7856128 bytes
```

---

## 📋 6. Archivos que se crearán en la Fase 2

1. **`firmware/src/Core/LuaBridge.h` / `LuaBridge.cpp`:**
   * Declaración y registro de todas las funciones nativas bajo la tabla `cbdos`.
2. **`firmware/src/Core/LuaREPL.h` / `LuaREPL.cpp`:**
   * Lógica de lectura serie no-bloqueante, auto-evaluación y formateo de respuestas.
3. **Integración en `main.cpp`:**
   * Inicialización de `LuaBridge::registerAll(LuaEngine::getInstance().getRawState())` y llamada a `LuaREPL::update()` en el loop.
