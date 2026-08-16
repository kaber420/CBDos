# RFC: Arquitectura de Lanzador OTA Multi-Ranura (Multi-Slot OTA Architecture)

## 1. Visión General
Este documento define la arquitectura para la gestión y lanzamiento de aplicaciones (juegos, emuladores) en el sistema CBDos. 
En lugar de cargar binarios dinámicamente desde la tarjeta SD (lo cual añade complejidad y tiempos de carga inestables), el sistema utilizará **Particiones Fijas (Ranuras)** predefinidas en la memoria Flash (16MB) del ESP32-S3.

El objetivo es proveer a los usuarios y desarrolladores de un esquema tipo "Consola de Cartuchos", donde se pueden flashear aplicaciones directamente a ranuras específicas mediante PlatformIO o herramientas OTA, y el lanzador principal se encarga de validar su existencia antes de ceder el control.

## 2. Tabla de Particiones Propuesta (custom_16MB_ota.csv)

Para acomodar puertos pesados y mantener espacio para el sistema, se reestructura la memoria de 16MB:

| Nombre   | Tipo | Subtipo | Offset   | Tamaño   | Descripción |
|----------|------|---------|----------|----------|-------------|
| nvs      | data | nvs     | 0x009000 | 0x5000   | Non-Volatile Storage |
| otadata  | data | ota     | 0x00E000 | 0x2000   | Control OTA |
| app0     | app  | ota_0   | 0x010000 | 6MB      | **Sistema Base (CBDos)** |
| app1     | app  | ota_1   | 0x610000 | 2MB      | **Ranura 1** (Ej. DOOM, SNES, Ports pesados) |
| app2     | app  | ota_2   | 0x810000 | 1.5MB    | **Ranura 2** (Ej. GBC, NES, Emuladores ligeros) |
| spiffs   | data | spiffs  | 0x990000 | 5MB      | Almacenamiento interno (Reducido de 6MB) |
| fatfs    | data | fat     | 0xE90000 | ~1.3MB   | Almacenamiento FAT |

*Nota:* Se reduce SPIFFS en 1MB para permitir que la Ranura 1 crezca a 2MB y la Ranura 2 a 1.5MB.

## 3. Entornos de Desarrollo (`platformio.ini`)

Para facilitar el desarrollo y el flasheo por parte de terceros, se definirán entornos (`[env]`) semánticos. Cada entorno compilará el código de su respectivo juego/emulador y lo subirá al *offset* correcto de memoria correspondiente a la ranura deseada.

Ejemplos de entornos que se incluirán en el `platformio.ini`:

```ini
; -----------------------------------------------------
; RANURA 1 (Offset 0x610000) - 2MB Max
; -----------------------------------------------------
[env:slot1_doom]
; Configuración para compilar DOOM...
upload_command = "$PYTHONEXE" "$PROJECT_PACKAGES_DIR/tool-esptoolpy/esptool.py" --chip esp32s3 --port $UPLOAD_PORT --baud 921600 write_flash 0x610000 $SOURCE

; -----------------------------------------------------
; RANURA 2 (Offset 0x810000) - 1.5MB Max
; -----------------------------------------------------
[env:slot2_gbc]
; Configuración para compilar Game Boy Color...
upload_command = "$PYTHONEXE" "$PROJECT_PACKAGES_DIR/tool-esptoolpy/esptool.py" --chip esp32s3 --port $UPLOAD_PORT --baud 921600 write_flash 0x810000 $SOURCE

[env:slot2_nes]
; Plantilla para un futuro emulador de NES en la ranura 2
; upload_command = "$PYTHONEXE" "$PROJECT_PACKAGES_DIR/tool-esptoolpy/esptool.py" --chip esp32s3 --port $UPLOAD_PORT --baud 921600 write_flash 0x810000 $SOURCE
```

## 4. Lógica del Lanzador (Smart Launcher)

El archivo `UI/Views/DoomView.cpp` (que podría renombrarse a `LauncherView.cpp`) se actualizará para dejar de ser un lanzador "ciego".

### 4.1. Prevención de Bootloops
Actualmente, si se pulsa el botón de un cartucho y no hay firmware instalado en la partición, el ESP32 colapsa al intentar arrancar una región de memoria vacía.
La nueva implementación usará la API `esp_ota_get_partition_description()` nativa de ESP-IDF antes de realizar el cambio de boot.

### 4.2. Flujo de Ejecución (Pseudocódigo)
```cpp
void bootToSlot(esp_partition_subtype_t slot_subtype, const char* expected_name) {
    const esp_partition_t* partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, slot_subtype, NULL);
    
    if (partition == NULL) {
        UIManager::showToast("Error crítico: Partición no existe en el sistema.");
        return;
    }

    esp_app_desc_t app_desc;
    esp_err_t err = esp_ota_get_partition_description(partition, &app_desc);

    if (err != ESP_OK || app_desc.magic_word != ESP_APP_DESC_MAGIC_WORD) {
        // La partición está vacía o el binario está corrupto.
        UIManager::showToast("Ranura vacía. Flashea un juego primero.");
        return;
    }

    // Opcional: Validar que el juego sea el esperado (o simplemente arrancar lo que haya)
    Serial.printf("Encontrado firmware válido: %s v%s\n", app_desc.project_name, app_desc.version);
    
    // Configurar y reiniciar
    UIManager::showToast(String("Iniciando ") + app_desc.project_name + "...");
    delay(500); // Dar tiempo al Toast para renderizarse
    esp_ota_set_boot_partition(partition);
    esp_restart();
}
```

## 5. Diseño de Interfaz de Usuario (UI)

La interfaz se mantendrá limpia. Las tarjetas visuales (botones grandes) estarán presentes.
Se pueden adoptar dos enfoques (a discreción del usuario/desarrollador):
1. **Cartuchos Temáticos (Actual):** Mostrar el arte de DOOM y GameBoy Color asumiendo que esos son los juegos que el usuario instalará.
2. **Cartuchos Genéricos (Alternativa):** Mostrar arte genérico ("Ranura 1: Retro", "Ranura 2: Arcade") que invite a flashear cualquier cosa.

En cualquier caso, la lógica subyacente es agnóstica al juego; simplemente arranca el bloque de memoria correspondiente.

## 6. Conclusión
Este esquema arquitectónico asegura:
- **Zero-Crashes:** El menú principal nunca provocará un *kernel panic* por particiones vacías.
- **Extensibilidad:** Desarrolladores externos tienen un mapa de memoria claro (`platformio.ini` envs) al que apuntar.
- **Rendimiento:** Carga instantánea de los juegos, preservando la experiencia "Console-like" sobre la que se asienta CBDos.
