#include "BTGamepadDriver.h"

static const NimBLEUUID HID_SERVICE_UUID((uint16_t)0x1812);

// Variables estáticas
volatile uint16_t BTGamepadDriver::s_btButtons = 0;
volatile uint16_t BTGamepadDriver::s_prevButtons = 0;
volatile bool     BTGamepadDriver::s_enabled = true;
volatile bool     BTGamepadDriver::s_connected = false;
volatile bool     BTGamepadDriver::s_doConnect = false;
const NimBLEAdvertisedDevice* BTGamepadDriver::s_targetDevice = nullptr;
NimBLEClient*     BTGamepadDriver::s_pClient = nullptr;
char              BTGamepadDriver::s_deviceName[32] = "Desconectado";
uint32_t          BTGamepadDriver::s_lastPacketMs = 0;
GamepadProfileType BTGamepadDriver::s_activeProfile = PROFILE_AUTO;

// ─── Callbacks de Cliente BLE ─────────────────────────────────────────────
class BTDriverClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* pClient) override {
        Serial.printf("\n[BT GAMEPAD] ¡Conectado a %s!\n", pClient->getPeerAddress().toString().c_str());
    }

    void onDisconnect(NimBLEClient* pClient, int reason) override {
        Serial.printf("\n[BT GAMEPAD] Control desconectado (motivo: %d).\n", reason);
        BTGamepadDriver::s_connected = false;
        BTGamepadDriver::s_btButtons = 0;
        BTGamepadDriver::s_prevButtons = 0;
        if (BTGamepadDriver::s_enabled) {
            NimBLEDevice::getScan()->start(0, false);
        }
    }
};

// ─── Callbacks de Escaneo ─────────────────────────────────────────────────
class BTDriverScanCallbacks : public NimBLEScanCallbacks {
    void onDiscovered(const NimBLEAdvertisedDevice* advertisedDevice) override {
        if (!BTGamepadDriver::s_enabled) return;

        String name = advertisedDevice->getName().c_str();
        String addr = advertisedDevice->getAddress().toString().c_str();
        bool hasHID = advertisedDevice->isAdvertisingService(HID_SERVICE_UUID);

        String nameLower = name;
        nameLower.toLowerCase();
        bool looksLikeGamepad = hasHID ||
            nameLower.indexOf("gamepad") >= 0 ||
            nameLower.indexOf("controller") >= 0 ||
            nameLower.indexOf("wireless") >= 0 ||
            nameLower.indexOf("xbox") >= 0 ||
            nameLower.indexOf("pro") >= 0 ||
            nameLower.indexOf("joy-con") >= 0 ||
            nameLower.indexOf("8bitdo") >= 0 ||
            nameLower.indexOf("mocute") >= 0 ||
            nameLower.indexOf("ipega") >= 0 ||
            nameLower.indexOf("bm769") >= 0 ||
            nameLower.indexOf("bsp") >= 0;

        if (looksLikeGamepad && !BTGamepadDriver::s_connected && !BTGamepadDriver::s_doConnect) {
            Serial.printf("[BT GAMEPAD] Dispositivo detectado: '%s' [%s]. Conectando...\n", name.c_str(), addr.c_str());
            NimBLEDevice::getScan()->stop();
            BTGamepadDriver::s_targetDevice = advertisedDevice;
            BTGamepadDriver::s_doConnect = true;
        }
    }
};

BTGamepadDriver::BTGamepadDriver() {}

bool BTGamepadDriver::begin(const char* deviceName) {
    Serial.println("[BT GAMEPAD] Inicializando módulo Bluetooth...");
    s_enabled = true;
    s_btButtons = 0;
    s_prevButtons = 0;

    NimBLEDevice::init(deviceName);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    NimBLEDevice::setSecurityAuth(true, true, true);

    NimBLEScan* pScan = NimBLEDevice::getScan();
    pScan->setScanCallbacks(new BTDriverScanCallbacks());
    pScan->setInterval(45);
    pScan->setWindow(15);
    pScan->setActiveScan(true);

    pScan->start(0, false);
    Serial.println("[BT GAMEPAD] Escaneo en segundo plano activo.");
    return true;
}

void BTGamepadDriver::setEnabled(bool enable) {
    s_enabled = enable;
    if (!enable) {
        if (s_connected && s_pClient) {
            s_pClient->disconnect();
        }
        s_connected = false;
        s_btButtons = 0;
        s_prevButtons = 0;
        NimBLEDevice::getScan()->stop();
        Serial.println("[BT GAMEPAD] Bluetooth APAGADO por el usuario.");
    } else {
        Serial.println("[BT GAMEPAD] Bluetooth ACTIVADO por el usuario. Escaneando...");
        NimBLEDevice::getScan()->start(0, false);
    }
}

bool BTGamepadDriver::isEnabled() const {
    return s_enabled;
}

void BTGamepadDriver::setProfile(GamepadProfileType profile) {
    s_activeProfile = profile;
    Serial.printf("[BT GAMEPAD] Perfil fijado a: %d\n", (int)profile);
}

void BTGamepadDriver::update() {
    if (!s_enabled) return;

    // Conexión pendiente
    if (s_doConnect && !s_connected && s_targetDevice) {
        s_doConnect = false;
        if (!s_pClient) {
            s_pClient = NimBLEDevice::createClient();
            s_pClient->setClientCallbacks(new BTDriverClientCallbacks(), false);
            s_pClient->setConnectionParams(12, 12, 0, 51);
        }

        if (s_pClient->connect(s_targetDevice)) {
            s_connected = true;
            String name = s_targetDevice->getName().c_str();
            strncpy(s_deviceName, name.length() > 0 ? name.c_str() : "Gamepad", sizeof(s_deviceName) - 1);

            auto services = s_pClient->getServices(true);
            for (auto* pService : services) {
                auto chars = pService->getCharacteristics(true);
                for (auto* pChar : chars) {
                    if (pChar->canNotify()) {
                        pChar->subscribe(true, notifyCallback);
                    }
                }
            }
            Serial.printf("[BT GAMEPAD] ¡VINCULADO A '%s'!\n", s_deviceName);
        } else {
            s_connected = false;
            NimBLEDevice::getScan()->start(0, false);
        }
    }

    // Watchdog de seguridad: Si pasan más de 120ms sin paquetes de movimiento/botones, limpiar estado
    if (s_connected && (millis() - s_lastPacketMs > 120) && (s_btButtons != 0)) {
        s_btButtons = 0;
        if (s_prevButtons != 0) {
            Serial.println("[BT DEBUG] Watchdog: Todos los controles en REPOSO (0x0000)");
            s_prevButtons = 0;
        }
    }
}

void BTGamepadDriver::notifyCallback(NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
    if (length == 0 || !pData || !s_enabled) return;
    s_lastPacketMs = millis();
    parseHIDReport(pData, length);
}

void BTGamepadDriver::parseHIDReport(const uint8_t* data, size_t len) {
    uint16_t mask = 0;

    // ─── PERFIL 1: Mando BM769 / 17 Bytes ──────────────────────────────────
    if (len >= 17) {
        uint8_t b0 = data[0];
        uint8_t b1 = data[1];

        // 1. Botón A / Disparo (SOLO si bit 0 está activo y NO es estado liberado 0x02)
        if ((b0 & 0x01) && (b0 != 0x02) && (b0 != 0x00)) {
            mask |= PAD_A;
        }

        // 2. Botón B / Abrir (Cuando b0 es 0x02 y b1 es un indicador de acción o b0==0x02 con toque)
        // Se activa con B explícito
        if (b0 == 0x02 && b1 == 0x95) {
            mask |= PAD_B;
        }

        // 3. Botón X / Cambio de Arma
        if (b1 == 0x19 || b1 == 0xA5) {
            mask |= PAD_WEAPON_NEXT;
        }

        // 4. Botón Y / Sprint Run
        if (b1 == 0x2D || b1 == 0xA8) {
            mask |= PAD_RUN;
        }

        // 5. Palancas analógicas / D-Pad
        int8_t dx = (int8_t)data[1];
        int8_t dy = (int8_t)data[2];

        // Solo procesar deltas si están en rango válido de stick (-100 a +100 con deadzone de 35)
        if (dy < -35 && dy > -110) mask |= PAD_UP;
        if (dy > 35 && dy < 110)   mask |= PAD_DOWN;
        if (dx < -35 && dx > -110) mask |= PAD_STRAFE_L;
        if (dx > 35 && dx < 110)   mask |= PAD_STRAFE_R;

        // Palanca Derecha (Giro de cámara)
        if (len >= 8) {
            int8_t rdx = (int8_t)data[4];
            if (rdx < -30 && rdx > -110) mask |= PAD_LEFT;
            if (rdx > 30 && rdx < 110)   mask |= PAD_RIGHT;
        }
    }
    // ─── PERFIL 2: Gamepad Estándar HID (8..16 Bytes) ──────────────────────
    else if (len >= 6) {
        uint8_t lx = data[0];
        uint8_t ly = data[1];
        uint8_t rx = data[2];
        uint16_t rawBtns = data[4] | (data[5] << 8);

        if (ly < 70)  mask |= PAD_UP;
        if (ly > 185) mask |= PAD_DOWN;
        if (lx < 70)  mask |= PAD_STRAFE_L;
        if (lx > 185) mask |= PAD_STRAFE_R;

        if (rx < 70)  mask |= PAD_LEFT;
        if (rx > 185) mask |= PAD_RIGHT;

        if (rawBtns & (1 << 0)) mask |= PAD_A;           // A -> Disparar
        if (rawBtns & (1 << 1)) mask |= PAD_B;           // B -> Usar / Abrir
        if (rawBtns & (1 << 2)) mask |= PAD_WEAPON_NEXT; // X -> Cambiar Arma
        if (rawBtns & (1 << 3)) mask |= PAD_RUN;         // Y -> Correr
        if (rawBtns & (1 << 4)) mask |= PAD_RUN;         // L1 -> Correr
        if (rawBtns & (1 << 5)) mask |= PAD_A;           // R1 -> Disparar
        if (rawBtns & (1 << 6)) mask |= PAD_A;           // L2 -> Disparar
        if (rawBtns & (1 << 7)) mask |= PAD_A;           // R2 -> Disparar
        if (rawBtns & (1 << 8)) mask |= PAD_SELECT;      // Select
        if (rawBtns & (1 << 9)) mask |= PAD_START;       // Start
    }

    s_btButtons = mask;

    // ─── Debug Serial Inteligente (Solo al cambiar de estado) ──────────────
    if (mask != s_prevButtons) {
        Serial.printf("[BT MAPPER] Botones: 0x%04X -> ", mask);
        if (mask == 0) {
            Serial.println("[REPOSO]");
        } else {
            if (mask & PAD_A)           Serial.print("[A:DISPARO] ");
            if (mask & PAD_B)           Serial.print("[B:ABRIR] ");
            if (mask & PAD_WEAPON_NEXT) Serial.print("[X:ARMA] ");
            if (mask & PAD_RUN)         Serial.print("[Y/L1:RUN] ");
            if (mask & PAD_UP)          Serial.print("[ARRIBA] ");
            if (mask & PAD_DOWN)        Serial.print("[ABAJO] ");
            if (mask & PAD_STRAFE_L)    Serial.print("[STRAFE_L] ");
            if (mask & PAD_STRAFE_R)    Serial.print("[STRAFE_R] ");
            if (mask & PAD_LEFT)        Serial.print("[GIRO_L] ");
            if (mask & PAD_RIGHT)       Serial.print("[GIRO_R] ");
            if (mask & PAD_START)       Serial.print("[START] ");
            if (mask & PAD_SELECT)      Serial.print("[SELECT] ");
            Serial.println();
        }
        s_prevButtons = mask;
    }
}

uint16_t BTGamepadDriver::readButtons() {
    return s_enabled ? s_btButtons : 0;
}

bool BTGamepadDriver::isConnected() const {
    return s_enabled && s_connected;
}

const char* BTGamepadDriver::getConnectedName() const {
    return s_deviceName;
}
