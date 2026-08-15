// ==========================================================================
// BLEGamepadTester.cpp — Herramienta de Diagnóstico y Sniffer de Gamepads BLE
//
// Diseñado para escanear, emparejar y analizar cualquier control Bluetooth
// (Android HID, Xbox BLE, Switch, DualShock 4, Mandos chinos genéricos).
// Muestra datos en tiempo real en Serial (115200 baud) y pantalla AMOLED JC3248W535.
// ==========================================================================

#include <Arduino.h>
#include <JC3248W535.h>
#include <NimBLEDevice.h>

// ─── Display AMOLED ───────────────────────────────────────────────────────
static JC3248W535_Display s_display;

// ─── Estado Global ────────────────────────────────────────────────────────
static bool s_doConnect = false;
static bool s_connected = false;
static NimBLEAdvertisedDevice* s_targetDevice = nullptr;
static NimBLEClient* s_pClient = nullptr;

static String s_statusMessage = "Iniciando BLE...";
static String s_deviceName = "Ninguno";
static String s_deviceAddr = "00:00:00:00:00:00";
static uint8_t s_lastReport[32];
static size_t s_lastReportLen = 0;
static uint32_t s_packetCount = 0;
static bool s_needScreenRedraw = true;

// UUIDs estándar de Bluetooth SIG
static const NimBLEUUID HID_SERVICE_UUID((uint16_t)0x1812);
static const NimBLEUUID REPORT_CHAR_UUID((uint16_t)0x2A4D);

// ─── Renderizado en Pantalla AMOLED ───────────────────────────────────────
void updateScreen() {
    if (!s_display.getCanvas()) return;
    auto* canvas = s_display.getCanvas();
    canvas->fillScreen(0x0000); // Negro

    // Encabezado
    canvas->fillRect(0, 0, 320, 36, 0x18E3); // Barra azul oscura
    canvas->setTextColor(0xFFFF);
    canvas->setTextSize(2);
    canvas->setCursor(20, 8);
    canvas->print("BLE Gamepad Sniffer");

    // Estado de conexión
    canvas->setTextSize(1);
    canvas->setCursor(10, 46);
    canvas->setTextColor(0x7BEF); // Gris
    canvas->print("ESTADO: ");
    if (s_connected) {
        canvas->setTextColor(0x07E0); // Verde
        canvas->print("CONECTADO");
    } else if (s_doConnect) {
        canvas->setTextColor(0xFD20); // Naranja
        canvas->print("CONECTANDO...");
    } else {
        canvas->setTextColor(0x05BF); // Cian
        canvas->print("ESCANEANDO MANDOS...");
    }

    // Información del dispositivo
    canvas->setCursor(10, 62);
    canvas->setTextColor(0x7BEF);
    canvas->print("DISPOSITIVO: ");
    canvas->setTextColor(0xFFFF);
    canvas->print(s_deviceName.substring(0, 20));

    canvas->setCursor(10, 78);
    canvas->setTextColor(0x7BEF);
    canvas->print("MAC: ");
    canvas->setTextColor(0xFFFF);
    canvas->print(s_deviceAddr);

    canvas->drawFastHLine(10, 95, 300, 0x39E7);

    // Reporte de Datos RAW
    canvas->setCursor(10, 105);
    canvas->setTextColor(0xFFE0); // Amarillo
    canvas->setTextSize(2);
    canvas->print("PAQUETE #");
    canvas->print(s_packetCount);

    canvas->setTextSize(1);
    canvas->setCursor(10, 130);
    canvas->setTextColor(0x07FF); // Cian
    canvas->printf("Longitud: %d bytes\n", s_lastReportLen);

    // Imprimir bytes en hexadecimal
    canvas->setCursor(10, 150);
    canvas->setTextColor(0xFFFF);
    canvas->setTextSize(2);
    if (s_lastReportLen > 0) {
        for (size_t i = 0; i < s_lastReportLen && i < 16; i++) {
            canvas->printf("%02X ", s_lastReport[i]);
            if (i == 7) {
                canvas->println();
                canvas->setCursor(10, 175);
            }
        }
    } else {
        canvas->setTextColor(0x7BEF);
        canvas->print("Esperando botones...");
    }

    // Área de ayuda inferior
    canvas->fillRect(0, 420, 320, 60, 0x1082);
    canvas->setTextSize(1);
    canvas->setTextColor(0x07E0);
    canvas->setCursor(10, 428);
    canvas->println("Monitor Serie: 115200 baudios");
    canvas->setTextColor(0xFFFF);
    canvas->setCursor(10, 442);
    canvas->println("Prueba HOME+X (Android), HOME+B (Xbox)");
    canvas->setCursor(10, 456);
    canvas->println("o HOME+Y para cambiar el modo de tu mando");

    s_display.flush();
}

// ─── Callback de Notificación HID Report (Datos en tiempo real) ───────────
static void notifyCallback(NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
    s_packetCount++;
    s_lastReportLen = (length > 32) ? 32 : length;
    memcpy(s_lastReport, pData, s_lastReportLen);
    s_needScreenRedraw = true;

    // Volcado completo y formateado por Monitor Serie
    Serial.printf("\n[BT SNIFFER #%05lu] Len: %d | RAW HEX: ", (unsigned long)s_packetCount, length);
    for (size_t i = 0; i < length; i++) {
        Serial.printf("%02X ", pData[i]);
    }
    Serial.print(" | ");

    // Intento de decodificación rápida para ejes y botones comunes
    if (length >= 4) {
        Serial.printf("B0:%02X B1:%02X B2:%02X B3:%02X ", pData[0], pData[1], pData[2], pData[3]);
    }
    if (length >= 8) {
        Serial.printf("B4:%02X B5:%02X B6:%02X B7:%02X ", pData[4], pData[5], pData[6], pData[7]);
    }
    Serial.println();
}

// ─── Callbacks de Cliente BLE ─────────────────────────────────────────────
class ClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* pClient) override {
        Serial.println("\n[BLE] *** ¡CONECTADO CON ÉXITO AL CONTROL! ***");
        s_connected = true;
        s_statusMessage = "Conectado";
        s_needScreenRedraw = true;
    }

    void onDisconnect(NimBLEClient* pClient) override {
        Serial.println("\n[BLE] *** CONTROL DESCONECTADO ***");
        s_connected = false;
        s_doConnect = false;
        s_lastReportLen = 0;
        s_statusMessage = "Desconectado. Reescaneando...";
        s_needScreenRedraw = true;
        NimBLEDevice::getScan()->start(0, false);
    }
};

// ─── Callbacks de Escaneo ─────────────────────────────────────────────────
class AdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) override {
        String name = advertisedDevice->getName().c_str();
        String addr = advertisedDevice->getAddress().toString().c_str();
        bool hasHID = advertisedDevice->isAdvertisingService(HID_SERVICE_UUID);

        Serial.printf("[BLE ESCANEO] Dispositivo: '%s' | MAC: %s | RSSI: %d | HID: %s\n",
            name.c_str(), addr.c_str(), advertisedDevice->getRSSI(), hasHID ? "SI" : "NO");

        // Criterio de selección: si tiene servicio HID o si el nombre parece un control/gamepad
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
            nameLower.indexOf("bsp") >= 0;

        if (looksLikeGamepad && !s_connected && !s_doConnect) {
            Serial.printf("[BLE] >> ¡CONTROL CANDIDATO DETECTADO! Conectando a '%s' [%s]...\n", name.c_str(), addr.c_str());
            NimBLEDevice::getScan()->stop();
            s_targetDevice = advertisedDevice;
            s_deviceName = name.length() > 0 ? name : "Gamepad Sin Nombre";
            s_deviceAddr = addr;
            s_doConnect = true;
            s_needScreenRedraw = true;
        }
    }
};

// ─── Conectar y Descubrir Características HID ─────────────────────────────
bool connectToServer() {
    Serial.printf("[BLE] Iniciando conexión con %s...\n", s_targetDevice->getAddress().toString().c_str());

    if (!s_pClient) {
        s_pClient = NimBLEDevice::createClient();
        s_pClient->setClientCallbacks(new ClientCallbacks(), false);
        s_pClient->setConnectionParams(12, 12, 0, 51); // Conexión de baja latencia para gaming
    }

    if (!s_pClient->connect(s_targetDevice)) {
        Serial.println("[BLE ERROR] Falló la conexión al dispositivo!");
        return false;
    }

    Serial.println("[BLE] Buscando servicios...");
    std::vector<NimBLERemoteService*>* services = s_pClient->getServices(true);
    if (!services) {
        Serial.println("[BLE ERROR] No se pudieron obtener los servicios.");
        s_pClient->disconnect();
        return false;
    }

    bool subscribed = false;
    for (auto* pService : *services) {
        Serial.printf("[BLE] Servicio encontrado: UUID: %s\n", pService->getUUID().toString().c_str());
        std::vector<NimBLERemoteCharacteristic*>* chars = pService->getCharacteristics(true);
        if (chars) {
            for (auto* pChar : *chars) {
                Serial.printf("   -> Característica: UUID: %s | Notif:%s | Read:%s\n",
                    pChar->getUUID().toString().c_str(),
                    pChar->canNotify() ? "SI" : "NO",
                    pChar->canRead() ? "SI" : "NO");

                // Suscribirse a notificaciones de características HID o de reporte
                if (pChar->canNotify()) {
                    if (pChar->subscribe(true, notifyCallback)) {
                        Serial.printf("   [+] ¡Suscrito exitosamente a notificaciones de %s!\n", pChar->getUUID().toString().c_str());
                        subscribed = true;
                    }
                }
            }
        }
    }

    if (!subscribed) {
        Serial.println("[BLE ADVERTENCIA] No se encontró ninguna característica con Notify en los servicios escaneados.");
    } else {
        Serial.println("\n=======================================================");
        Serial.println("  ¡LISTO! Presiona botones o mueve palancas en tu mando ");
        Serial.println("=======================================================\n");
    }

    return true;
}

// ─── Setup ────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=============================================");
    Serial.println("   espOS32 — BLE Gamepad Sniffer & Tester    ");
    Serial.println("=============================================");

    // 1. Inicializar Pantalla AMOLED
    if (s_display.begin()) {
        s_display.setRotation(ROTATION_0); // Portrait 320x480
        s_display.backlightOn();
        updateScreen();
    } else {
        Serial.println("[ERROR] No se pudo inicializar la pantalla AMOLED.");
    }

    // 2. Inicializar NimBLE
    NimBLEDevice::init("espOS32-Sniffer");
    NimBLEDevice::setPower(ESP_PWR_LVL_P9); // Máxima potencia de transmisión BLE
    NimBLEDevice::setSecurityAuth(true, true, true);

    // 3. Configurar Escaneo
    NimBLEScan* pScan = NimBLEDevice::getScan();
    pScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
    pScan->setInterval(45);
    pScan->setWindow(15);
    pScan->setActiveScan(true);

    Serial.println("[BLE] Iniciando escaneo de mandos Bluetooth...");
    pScan->start(0, false);
}

// ─── Loop Principal ───────────────────────────────────────────────────────
void loop() {
    if (s_doConnect && !s_connected) {
        s_doConnect = false;
        if (!connectToServer()) {
            Serial.println("[BLE] Reanudando escaneo...");
            NimBLEDevice::getScan()->start(0, false);
        }
    }

    if (s_needScreenRedraw) {
        s_needScreenRedraw = false;
        updateScreen();
    }

    delay(20);
}
