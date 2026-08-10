# DISEÑO: Sistema de Configuración Multi-Gateway

**Fecha:** 2026-08-09
**Proyecto:** espOS32 Firmware
**Objetivo:** Definir la arquitectura del sistema de configuración con dominios separados: WiFi, LoRa, FLRC, Gateways.

---

## 1. PRINCIPIO: SEPARACIÓN DE DOMINIOS

Cada subsistema tiene su propia configuración. **Nunca se mezclan.**

```
┌─────────────────────────────────────────────────────┐
│                  CONFIGURACIÓN                       │
├─────────────┬─────────────┬──────────┬──────────────┤
│    WiFi     │    LoRa     │   FLRC   │   Gateways   │
│  (red)      │  (900MHz)   │ (2.4GHz) │  (ruteo)     │
├─────────────┼─────────────┼──────────┼──────────────┤
│ SSID        │ Frecuencia  │ Canal    │ Dirección    │
│ Password    │ Potencia    │ Potencia │ Auth token   │
│ IP fija?    │ BW          │ BW       │ Dominio      │
│ DNS         │ SF          │ Rate     │ Puerto MQTT  │
│             │ Sync word   │ Sync word│ TLS?         │
└─────────────┴─────────────┴──────────┴──────────────┘
```

- **WiFi** → Cómo se conecta a la red local
- **LoRa** → Parámetros de radio 900MHz (mesh, largo alcance)
- **FLRC** → Parámetros de radio 2.4GHz (alta velocidad)
- **Gateway** → A qué servidor/routeo se conecta (el `.enc` es SOLO para esto)

---

## 2. EL .enc SOLO CONFIGURA EL GATEWAY

### 2.1. Formato binario (igual que el actual)

```
┌─────────────────┬──────────────┬──────────────────────────┬──────────────┐
│ Salt (8 bytes)  │ IV (12 bytes)│ Ciphertext (JSON)        │ Tag (16 bytes)│
└─────────────────┴──────────────┴──────────────────────────┴──────────────┘
```

**Encriptación:** AES-256-GCM con PBKDF2-HMAC-SHA256 (10000 iteraciones).

### 2.2. JSON desencriptado (SOLO datos del gateway, NADA de WiFi/LoRa/FLRC)

```json
{
    "name": "MiGateway",
    "address": "192.168.1.50",
    "domain": "gateway.local",
    "mqtt_port": 1883,
    "mqtt_use_tls": false,
    "auth_token": "abc123def456...",
    "auth_type": "token",
    "discovery_method": "static",
    "notes": "Gateway principal"
}
```

### 2.3. Campos

| Campo | Tipo | Req | Descripción |
|-------|------|-----|-------------|
| `name` | string | SÍ | Nombre descriptivo (aparece en UI) |
| `address` | string | SÍ | IP o dominio del servidor MQTT |
| `domain` | string | NO | Dominio mDNS para discovery |
| `mqtt_port` | int | NO | Puerto MQTT (default: 1883) |
| `mqtt_use_tls` | bool | NO | MQTT over TLS (default: false) |
| `auth_token` | string | SÍ | Token de autenticación del dispositivo |
| `auth_type` | string | NO | `token` (default), `certificate`, `key` |
| `discovery_method` | string | NO | `static`, `mdns`, `broadcast` |
| `notes` | string | NO | Notas |

### 2.4. Estructura en disco

```
SD/
├── gateways/
│   ├── gw_principal.enc
│   ├── gw_secundario.enc
│   └── ...
```

---

## 3. ESTRUCTURA DE CONFIGURACIÓN GENERAL

### 3.1. Configuración WiFi (separable, se ingresa desde UI o provisioning)

```cpp
struct WiFiConfig {
    String ssid;
    String password;
    bool useStaticIp = false;
    String staticIp;
    String gateway;
    String subnet;
    String dns1;
    String dns2;
};
```

**Almacenamiento NVS:**
```
Namespace "wifi":
  "ssid"        → String
  "pass"        → String
  "static"      → bool
  "ip"          → String
  "gw"          → String
  "sub"         → String
  "dns1"        → String
  "dns2"        → String
```

### 3.2. Configuración LoRa 900MHz

```cpp
struct LoRaConfig {
    float frequency = 915.0;      // MHz (915 America, 868 Europa, 433 Asia)
    int8_t txPower = 14;          // dBm (max 20)
    float bandwidth = 250.0;      // kHz (125, 250, 500)
    uint8_t spreadingFactor = 7;  // SF7-SF12
    uint8_t codingRate = 5;       // 4/5, 4/6, 4/7, 4/8
    uint16_t syncWord = 0x32;     // Sincronización de red
    bool enableCRC = true;
    uint16_t preambleLength = 8;
};
```

**Almacenamiento NVS:**
```
Namespace "lora":
  "freq"    → float
  "txpwr"   → int8_t
  "bw"      → float
  "sf"      → uint8_t
  "cr"      → uint8_t
  "sync"    → uint16_t
  "crc"     → bool
  "preamb"  → uint16_t
```

### 3.3. Configuración FLRC 2.4GHz

```cpp
struct FLRCConfig {
    float frequency = 2.400;       // GHz (2.400 - 2.4835)
    int8_t txPower = 10;           // dBm
    float bandwidth = 1.2;         // MHz (0.6, 1.2)
    uint8_t dataRate = 1;          // 0=1Mbps, 1=1.3Mbps
    uint8_t codingRate = 2;        // 0=1/2, 1=3/4, 2=2/3 (FLRC specific)
    uint16_t syncWord = 0x7B5A;    // Sincronización FLRC
    bool enableCRC = true;
    uint16_t preambleLength = 8;
};
```

**Almacenamiento NVS:**
```
Namespace "flrc":
  "freq"    → float
  "txpwr"   → int8_t
  "bw"      → float
  "rate"    → uint8_t
  "cr"      → uint8_t
  "sync"    → uint16_t
  "crc"     → bool
  "preamb"  → uint16_t
```

### 3.4. Configuración Gateway (la que viene en el .enc)

```cpp
struct GatewayConfig {
    String name;
    String address;
    String domain;
    int mqttPort = 1883;
    bool mqttUseTls = false;
    String authToken;
    String authType = "token";
    String discoveryMethod = "static";
    String notes;
};
```

**Almacenamiento NVS:**
```
Namespace "gateways":
  "active_id"           → String (ID del gateway activo)
  "count"               → int
  
  "gw_001_name"         → String
  "gw_001_address"      → String
  "gw_001_domain"       → String
  "gw_001_mqtt_port"    → int
  "gw_001_mqtt_tls"     → bool
  "gw_001_auth_token"   → String
  "gw_001_auth_type"    → String
  "gw_001_discovery"    → String
  "gw_001_notes"        → String
  
  (repetir por cada gateway)
```

---

## 4. ConfigManager: OPERACIONES POR DOMINIO

```cpp
class ConfigManager {
public:
    // ─── WiFi ───
    bool loadWiFi(WiFiConfig& cfg);
    bool saveWiFi(const WiFiConfig& cfg);
    
    // ─── LoRa ───
    bool loadLoRa(LoRaConfig& cfg);
    bool saveLoRa(const LoRaConfig& cfg);
    
    // ─── FLRC ───
    bool loadFLRC(FLRCConfig& cfg);
    bool saveFLRC(const FLRCConfig& cfg);
    
    // ─── Gateways ───
    bool importGateway(const String& encPath, const String& pin, String& errorOut);
    bool removeGateway(const String& gwId);
    std::vector<GatewayConfig> listGateways();
    bool setActiveGateway(const String& gwId);
    bool loadActiveGateway(GatewayConfig& gw);
};
```

---

## 5. FLUJO DE PROVISIONING DEL .enc

```
1. Usuario coloca /gateways/gw_nuevo.enc en la SD
2. Desde UI, presiona "Agregar Gateway"
3. Se le pide el PIN de ese .enc
4. Se desencripta (PBKDF2 → AES-256-GCM)
5. Se extrae JSON → GatewayConfig
6. Se genera ID único ("gw_001", "gw_002"...)
7. Se guarda en NVS bajo namespace "gateways"
8. Se añade a la lista
9. Se elimina el .enc de la SD
```

---

## 6. CAMBIOS EN SERVICIOS

### MQTTService

```cpp
// ANTES:
void init(const String& hubIp, int mqttPort, const String& mac);

// DESPUÉS:
void init(const GatewayConfig& gw, const String& mac);
void reconnectTo(const GatewayConfig& gw);
```

### DiscoveryService

```cpp
// ANTES:
void startDiscovery(const String& savedHubIp);

// DESPUÉS:
void startDiscovery(const GatewayConfig& gw);
```

Dependiendo de `gw.discovery_method`:
- `"static"` → Ir directo a `gw.address`
- `"mdns"` → Buscar `gw.domain` vía mDNS
- `"broadcast"` → UDP broadcast

---

## 7. UI: PANTALLA DE CONFIGURACIÓN

### 7.1. Acceso

Botón "Configuración" (id=4) en DashboardView → `ConfigView`

### 7.2. ConfigView: Menú Principal

```
┌─────────────────────────────────────┐
│ ←  Configuración                   │
├─────────────────────────────────────┤
│                                     │
│  [ WiFi ]                           │  → Sub-pantalla WiFiConfigView
│                                     │
│  [ LoRa (900MHz) ]                  │  → Sub-pantalla LoRaConfigView
│                                     │
│  [ FLRC (2.4GHz) ]                  │  → Sub-pantalla FLRCConfigView
│                                     │
│  [ Gateways ]                       │  → Sub-pantalla GatewayConfigView
│                                     │
│  [ Sistema ]                        │  → DiagnosticsModal (ya existe)
│                                     │
└─────────────────────────────────────┘
```

### 7.3. Sub-pantalla: WiFiConfigView

```
┌─────────────────────────────────────┐
│ ←  WiFi                            │
├─────────────────────────────────────┤
│                                     │
│  Red: MiRedWiFi          [ Editar ] │
│  IP:   192.168.1.105                │
│  Estado: Conectado ✓                │
│                                     │
│  IP Estática:     [ No ]            │
│  Gateway:    192.168.1.1            │
│  Subnet:     255.255.255.0          │
│  DNS primario:   8.8.8.8            │
│  DNS secundario: 8.8.4.4            │
│                                     │
└─────────────────────────────────────┘
```

### 7.4. Sub-pantalla: LoRaConfigView

```
┌─────────────────────────────────────┐
│ ←  LoRa (900MHz)                   │
├─────────────────────────────────────┤
│                                     │
│  Frecuencia:    915.0 MHz  [ Editar]│
│  Potencia TX:   14 dBm             │
│  Ancho banda:   250 kHz            │
│  Spreading:     SF7                │
│  Coding Rate:   4/5                │
│  Sync Word:     0x32               │
│  CRC:           Sí                 │
│                                     │
│  [ Guardar ]                        │
│                                     │
└─────────────────────────────────────┘
```

### 7.5. Sub-pantalla: FLRCConfigView

```
┌─────────────────────────────────────┐
│ ←  FLRC (2.4GHz)                   │
├─────────────────────────────────────┤
│                                     │
│  Frecuencia:    2.400 GHz  [ Editar]│
│  Potencia TX:   10 dBm             │
│  Ancho banda:   1.2 MHz            │
│  Data Rate:     1.3 Mbps           │
│  Coding Rate:   2/3                │
│  Sync Word:     0x7B5A             │
│  CRC:           Sí                 │
│                                     │
│  [ Guardar ]                        │
│                                     │
└─────────────────────────────────────┘
```

### 7.6. Sub-pantalla: GatewayConfigView

```
┌─────────────────────────────────────┐
│ ←  Gateways                        │
├─────────────────────────────────────┤
│                                     │
│  Gateway Activo:                    │
│  ┌─────────────────────────────┐    │
│  │ 🟢 MiGateway                │    │
│  │ 192.168.1.50:1883           │    │
│  │ MQTT: Conectado ✓           │    │
│  └─────────────────────────────┘    │
│                                     │
│  Todos los gateways:                │
│  ┌─────────────────────────────┐    │
│  │ ● MiGateway                 │    │
│  │ ○ GatewaySecundario         │    │
│  └─────────────────────────────┘    │
│                                     │
│  [ Cambiar Gateway ]                │
│  [ Agregar Gateway .enc ]           │
│  [ Eliminar Gateway ]               │
│                                     │
└─────────────────────────────────────┘
```

---

## 8. PANEL DE AJUSTES RÁPIDOS (Quick Settings)

### 8.1. Concepto

Panel que se despliega desde arriba (swipe down desde HeaderBar o toque en reloj/ícono). Acceso rápido a toggles y sliders **sin entrar a Configuración completa**. Solo opera sobre configuración ya existente.

### 8.2. Activación

- **Swipe down** desde cualquier parte del HeaderBar
- **Toque en el reloj** del HeaderBar
- Se cierra con **swipe up** o **toque fuera del panel**

### 8.3. Diseño

```
┌──────────────────────────────────────┐
│  ✅ WiFi      🔲 LoRa    🔲 FLRC     │  ← Toggles on/off
│                                      │
│  🔆 ████████░░░░░░  65%              │  ← Slider brillo
│  🔊 ██████░░░░░░░░  45%              │  ← Slider volumen
│                                      │
│  🟢 Gateway: MiGateway               │  ← Gateway activo (tap para cambiar)
│  ─────────────────────────────────── │
│  [ Configuración completa ]          │  → Abre ConfigView
└──────────────────────────────────────┘
```

### 8.4. Funciones

| Elemento | Tipo | Función |
|----------|------|---------|
| WiFi toggle | Switch | Encender/apagar WiFi (guarda en NVS) |
| LoRa toggle | Switch | Encender/apagar radio LoRa (guarda en NVS) |
| FLRC toggle | Switch | Encender/apagar radio FLRC (guarda en NVS) |
| Brillo slider | Slider 0-100% | Ajustar backlight (guarda en NVS) |
| Volumen slider | Slider 0-100% | Ajustar volumen I2S (guarda en NVS) |
| Gateway | Label + tap | Muestra gateway activo, toque abre selector rápido |
| Config completa | Button | Navega a ConfigView |

### 8.5. Almacenamiento de ajustes rápidos

```
Namespace "quick_settings":
  "wifi_on"      → bool (default: true)
  "lora_on"      → bool (default: false)
  "flrc_on"      → bool (default: false)
  "brightness"   → uint8_t (0-100, default: 80)
  "volume"       → uint8_t (0-100, default: 50)
```

### 8.6. Archivos UI nuevos

```
firmware/src/UI/Components/
├── QuickSettingsPanel.h / .cpp    ← Panel desplegable
```

### 8.7. Integración con HeaderBar

El HeaderBar detecta el gesto swipe down y despliega el QuickSettingsPanel como overlay sobre la pantalla actual. No reemplaza la pantalla — se superpone.

---

## 8. ARCHIVOS NUEVOS

```
firmware/src/UI/Views/
├── ConfigView.h / .cpp              ← Menú principal de config
├── WiFiConfigView.h / .cpp          ← Config WiFi
├── LoRaConfigView.h / .cpp          ← Config LoRa
├── FLRCConfigView.h / .cpp          ← Config FLRC
├── GatewayConfigView.h / .cpp       ← Config Gateways
├── GatewaySelectorModal.h / .cpp    ← Modal elegir gateway
├── AddGatewayModal.h / .cpp         ← Modal importar .enc
└── QuickSettingsPanel.h / .cpp      ← Panel ajustes rápidos (swipe down)
```

---

## 9. ARCHIVOS A MODIFICAR

| Archivo | Cambio |
|---------|--------|
| `ConfigManager.h/.cpp` | Nuevos structs, CRUD por dominio, importación .enc |
| `MQTTService.h/.cpp` | `init()` recibe `GatewayConfig` |
| `DiscoveryService.h/.cpp` | `startDiscovery()` recibe `GatewayConfig` |
| `main.cpp` | Carga WiFi + Gateway activo al boot |
| `UIManager.h/.cpp` | `loadConfig()` → ConfigView, `loadQuickSettings()` |
| `DashboardView.cpp` | Botón Config → ConfigView |
| `HeaderBar.h/.cpp` | Detección swipe down para QuickSettingsPanel |

---

## 10. FLUJO DE BOOT

```
BOOT
  │
  ├─→ lv_init() + Display + Touch
  ├─→ ConfigManager::init()
  ├─→ SD.begin()
  │
  ├─→ Cargar WiFi desde NVS → WiFi.begin()
  ├─→ Cargar Gateway activo desde NVS
  │
  ├─→ ¿Gateway configurado?
  │     ├─ NO → Mostrar ConfigView (setup inicial)
  │     └─ SÍ → MQTTService::init(gw, mac)
  │             → DiscoveryService::startDiscovery(gw)
  │
  └─→ Launcher (DashboardView)
        │
        ├─ Swipe down HeaderBar → QuickSettingsPanel
        ├─ Configuración → ConfigView
        ├─ Media Viewer → MenuView
        ├─ Mesh Chat → (pendiente)
        └─ WAV Browser → (pendiente)
```

---

## 11. SEGURIDAD

- El `.enc` SOLO contiene datos del gateway (no WiFi, no LoRa, no FLRC)
- Cada `.enc` tiene su propio PIN
- El `auth_token` viaja encriptado y se guarda en NVS
- El `.enc` se **elimina** de la SD después de importarlo
- 3 intentos fallidos de PIN → bloqueo hasta reinicio

---

## 12. PREGUNTAS ABIERTAS

- [ ] ¿El PIN es el mismo para todos los .enc o uno por .enc?
- [ ] ¿Cuántos gateways máximo? (NVS tiene ~4000 bytes)
- [ ] ¿Se puede exportar .enc desde el dispositivo (backup)?
- [ ] ¿El auth_token se renueva o es estático?
- [ ] ¿Soporte MQTT over TLS a futuro? (mbedtls ya está en el proyecto)
