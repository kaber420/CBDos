# RFC: Cargador Inteligente OTA (Smart OTA Launcher)

## 1. Resumen
Propuesta para dotar de "inteligencia" al menú de lanzamiento de juegos (`DoomView.cpp`) en CBDos. Actualmente, el sistema lanza la partición OTA correspondiente de manera "ciega". Si el firmware de la partición no está instalado o faltan los archivos necesarios en la SD (`.wad` o `.gbc`), el ESP32 colapsa o entra en un bucle de reinicio. 
El **Smart OTA Launcher** verificará todo en tiempo real al pulsar el botón, sin consumir ciclos en segundo plano.

## 2. Objetivos
- **Prevenir bloqueos (Bootloops):** Validar que la partición OTA de destino tiene una aplicación válida antes de reiniciar.
- **Validación de Archivos:** Asegurar que los recursos de memoria (ej. `doom1.wad`) existen en la MicroSD.
- **Feedback Visual Inmediato:** Usar el sistema de notificaciones (`UIManager::showToast()`) para informar al usuario si algo falta ("Falta instalar firmware", "No se encontró doom.wad").

## 3. Implementación Propuesta

### 3.1. Detección de Firmware (esp_ota_ops)
Al pulsar el botón de lanzamiento, se llamará a la API nativa del ESP32:
```cpp
esp_app_desc_t app_desc;
if (esp_ota_get_partition_description(game_partition, &app_desc) != ESP_OK) {
    // La partición está vacía o el binario está corrupto.
    UIManager::showToast("Falta instalar firmware");
    return; // Abortar lanzamiento
}
```

### 3.2. Detección de Archivos (SD.h)
Se integrará la librería `<SD.h>` en `DoomView.cpp` (que actualmente no la incluye) para hacer comprobaciones asíncronas solo *on-click*.

**Para DOOM:**
```cpp
bool checkDoomFiles() {
    const char* wads[] = {"/doom1.wad", "/DOOM1.WAD", "/doom.wad", "/DOOM.WAD", "/doom2.wad", "/DOOM2.WAD"};
    for(int i=0; i<6; i++) {
        if(SD.exists(wads[i])) return true;
    }
    return false;
}
```

**Para Game Boy:**
```cpp
bool checkGBCFiles() {
    const char* dirs[] = {"/roms/gbc", "/roms/gb", "/roms/gameboy", "/roms"};
    for(int i=0; i<4; i++) {
        File dir = SD.open(dirs[i]);
        if(dir && dir.isDirectory()) {
            File f = dir.openNextFile();
            while(f) {
                String name = f.name();
                name.toLowerCase();
                if(name.endsWith(".gb") || name.endsWith(".gbc") || name.endsWith(".cgb")) {
                    f.close();
                    dir.close();
                    return true;
                }
                f.close();
                f = dir.openNextFile();
            }
            dir.close();
        }
    }
    return false;
}
```

### 3.3. Refactorización de bootToPartition()
Se actualizará la firma de la función genérica para inyectarle las reglas de validación en el momento del click:
```cpp
typedef bool (*PreflightCheckFn)(String& outErrorMsg);
static void bootToPartition(esp_partition_subtype_t subtype, const char* gameName, PreflightCheckFn checkFn);
```

## 4. Conclusión
El coste de rendimiento será **cero** durante la navegación por la UI. El único coste se pagará (aprox. 5-10 milisegundos) cuando el usuario intente arrancar un juego, brindando una experiencia robusta y pulida.
