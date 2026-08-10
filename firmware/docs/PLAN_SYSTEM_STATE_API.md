# Plan de Arquitectura: API de Estado del Sistema (`SystemStateAPI`)

## 1. Problema y Contexto
Actualmente, la barra superior (`HeaderBar`) intenta actualizar su estado de señal WiFi, batería e identificador mediante llamadas directas en el bucle principal de LVGL o a través de variables globales dispersas (`g_currentRssi`, `g_isConfigured`, etc.). 

Esto introducía tres problemas principales:
1. **Acoplamiento Directo (UI <-> Hardware):** La interfaz UI realizaba evaluaciones de estado en cada *tick* de pantalla.
2. **Desincronización en Cambios de Vista:** Al cambiar entre vistas (ej. Dashboard -> Configuración -> Dashboard), se destruían e instanciaban objetos `HeaderBar` perdiendo o bloqueando la referencia del último estado renderizado.
3. **Latencia / Bloqueos de Bus:** La UI evaluaba la red en la misma hebra que el renderizado gráfico.

---

## 2. Propuesta Arquitectónica (`SystemStateAPI`)

Diseñar una **API de Estado en Memoria de Única Fuente de Verdad (*Single Source of Truth*)** llamada `SystemStateAPI`.

```
                  ┌───────────────────────────────┐
                  │    Core 0 (Task de Red)       │
                  │  - Consulta WiFi.RSSI() 2 sec  │
                  │  - Verifica status conexión   │
                  └──────────────┬────────────────┘
                                 │ Escribe estado en RAM (Atómico)
                                 ▼
                  ┌───────────────────────────────┐
                  │     SystemStateAPI (RAM)      │
                  │  - isWifiConnected()          │
                  │  - getWifiRSSI()              │
                  │  - getWifiSignalLevel()       │
                  └──────────────┬────────────────┘
                                 │ Lee estado al instanciar / update
                                 ▼
                  ┌───────────────────────────────┐
                  │     HeaderBar (Core 1 UI)     │
                  │  - Renderiza badge en canvas  │
                  │  - Lee directamente de la API │
                  └───────────────────────────────┘
```

### Principios Fundamentales:
1. **Escritura desde Core 0 (Task de Red):** La tarea de red actualiza variables atómicas simples en RAM (`wifiConnected`, `rssi`) únicamente cuando se detecta un cambio de estado o tras un intervalo de polling de 2 segundos.
2. **Lectura Pasiva desde Core 1 (HeaderBar UI):** Al crearse cualquier `HeaderBar` (`HeaderBar::create()`), el componente simplemente lee `SystemStateAPI::getWifiState()` y dibuja su badge de inmediato.
3. **Zero IO/NVS en Hilos de Renderizado:** La UI nunca invoca llamadas bloqueantes ni consulta NVS/SPI durante la creación o dibujado.

---

## 3. Especificación de la API propuesta (`SystemStateAPI.h`)

```cpp
enum class WifiSignalLevel {
    DISCONNECTED = 0,
    WEAK = 1,      // RSSI < -80
    MEDIUM = 2,    // -80 <= RSSI < -70
    EXCELLENT = 3  // RSSI >= -70
};

struct SystemState {
    bool wifiConnected;
    int rssi;
    WifiSignalLevel wifiLevel;
};

class SystemStateAPI {
public:
    static void setWifiState(bool connected, int rssi);
    static SystemState getSystemState();
    static bool isWifiConnected();
    static WifiSignalLevel getWifiSignalLevel();
};
```

---

## 4. Plan de Verificación y Pruebas
1. **Prueba de Cero Latencia en UI:** Medir el tiempo de renderizado de `HeaderBar::create()` en microsegundos demostrando O(1) sin accesos al bus SPI.
2. **Prueba de Transición Multipantalla:** Navegar continuamente entre Dashboard, Configuración y Media Viewer garantizando que el icono de WiFi mantenga su color/estado de manera consistente.
3. **Revisión por Subagente / Modelo Especializado:** Enviar este plan a `deepseek-v4-flash-free` vía OpenCode para validar la coherencia multihilo en ESP32-S3 FreeRTOS.
