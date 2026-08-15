#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>
#include "CartridgeGamepad.h"

// ─── Botones extendidos de Doom ───────────────────────────────────────────
#ifndef PAD_WEAPON_NEXT
#define PAD_WEAPON_NEXT (1 << 12)
#endif
#ifndef PAD_AUTOMAP
#define PAD_AUTOMAP     (1 << 13)
#endif

// ─── Estructura de Perfil de Mapeo de Control ─────────────────────────────
enum GamepadProfileType {
    PROFILE_AUTO = 0,
    PROFILE_BM769_CHINESE,   // Mando Chino BM769 (17 bytes)
    PROFILE_STANDARD_HID,    // Gamepad estándar Android / Xbox (8-12 bytes)
    PROFILE_SWITCH_PRO       // Nintendo Switch Pro Controller
};

struct GamepadMapping {
    GamepadProfileType type;
    const char* name;
    uint8_t deadzone;
};

class BTGamepadDriver {
public:
    BTGamepadDriver();

    // Inicializa la radio NimBLE
    bool begin(const char* deviceName = "CBDos-Doom");

    // Activar o desactivar Bluetooth en tiempo de ejecución
    void setEnabled(bool enable);
    bool isEnabled() const;

    // Procesa eventos pendientes y watchdog de seguridad
    void update();

    // Devuelve la máscara de bits de botones activos (PAD_UP, PAD_A, PAD_B, etc.)
    uint16_t readButtons();

    // Estado y nombre
    bool isConnected() const;
    const char* getConnectedName() const;

    // Forzar un perfil de control específico
    void setProfile(GamepadProfileType profile);

private:
    friend class BTDriverScanCallbacks;
    friend class BTDriverClientCallbacks;

    static void notifyCallback(NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify);
    static void parseHIDReport(const uint8_t* data, size_t len);

    static volatile uint16_t s_btButtons;
    static volatile uint16_t s_prevButtons;
    static volatile bool     s_enabled;
    static volatile bool     s_connected;
    static volatile bool     s_doConnect;
    static const NimBLEAdvertisedDevice* s_targetDevice;
    static NimBLEClient*     s_pClient;
    static char              s_deviceName[32];
    static uint32_t          s_lastPacketMs;
    static GamepadProfileType s_activeProfile;
};
