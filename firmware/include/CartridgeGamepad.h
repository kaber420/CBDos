#pragma once

#include <Arduino.h>
#include <JC3248W535.h>
#include <esp_ota_ops.h>

// ─── Máscaras de bits para botones estándar de Gamepad ──────────────────────
enum GamepadButtons : uint16_t {
    PAD_NONE     = 0,
    PAD_UP       = 1 << 0,
    PAD_DOWN     = 1 << 1,
    PAD_LEFT     = 1 << 2,
    PAD_RIGHT    = 1 << 3,
    PAD_A        = 1 << 4,
    PAD_B        = 1 << 5,
    PAD_START    = 1 << 6,
    PAD_SELECT   = 1 << 7,
    PAD_EXIT     = 1 << 8,
    PAD_STRAFE_L = 1 << 9,
    PAD_STRAFE_R = 1 << 10,
    PAD_RUN      = 1 << 11
};

enum GamepadLayout {
    LAYOUT_PORTRAIT_GBC,  // Portrait 320x480 (Juego arriba 320x288, Controles abajo Y=288..480)
    LAYOUT_PORTRAIT_DOOM, // Portrait 320x480 (Juego arriba 320x200, Controles abajo Y=200..480)
    LAYOUT_LANDSCAPE_DOOM // Landscape 480x320 (Controles en márgenes laterales)
};

class CartridgeGamepad {
public:
    CartridgeGamepad();
    
    // Inicializar con punteros a Display y Touch
    void begin(JC3248W535_Display* display, JC3248W535_Touch* touch, GamepadLayout layout = LAYOUT_PORTRAIT_GBC);

    // Dibuja el overlay / panel de botones en pantalla
    void draw(bool force = false);

    // Lee los toques del panel táctil y devuelve la máscara de bits de botones presionados
    uint16_t read();

    // Comprueba si se presionó el botón SALIR y maneja el reinicio a espOS32 (app0)
    bool handleExit();

    // Dibuja el estado de Bluetooth (ON / OFF / CONECTADO)
    void drawBTStatus(bool connected, bool enabled);

    // Salir inmediatamente a espOS32 (app0)
    static void exitToOS();

private:
    JC3248W535_Display* m_display;
    JC3248W535_Touch*   m_touch;
    GamepadLayout       m_layout;
    uint16_t            m_lastButtons;
    bool                m_drawn;

    void drawPortraitGBC();
    void drawPortraitDoom();
    void drawLandscapeDoom();
    uint16_t readPortraitGBC(int16_t x, int16_t y);
    uint16_t readPortraitDoom(int16_t x, int16_t y);
    uint16_t readLandscapeDoom(int16_t x, int16_t y);
};
