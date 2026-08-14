#include "CartridgeGamepad.h"

CartridgeGamepad::CartridgeGamepad()
    : m_display(nullptr)
    , m_touch(nullptr)
    , m_layout(LAYOUT_PORTRAIT_GBC)
    , m_lastButtons(PAD_NONE)
    , m_drawn(false)
{
}

void CartridgeGamepad::begin(JC3248W535_Display* display, JC3248W535_Touch* touch, GamepadLayout layout) {
    m_display = display;
    m_touch = touch;
    m_layout = layout;
    m_drawn = false;
    m_lastButtons = PAD_NONE;
}

void CartridgeGamepad::exitToOS() {
    Serial.println("[CartridgeGamepad] Saliendo... Configurando arranque en espOS32 (app0)");
    const esp_partition_t* os_partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);

    if (os_partition != NULL) {
        esp_ota_set_boot_partition(os_partition);
        delay(200);
        esp_restart();
    } else {
        Serial.println("[CartridgeGamepad] ERROR: No se encontró la partición del OS (app0)");
        esp_restart();
    }
}

void CartridgeGamepad::drawPortraitGBC() {
    if (!m_display || !m_display->getCanvas()) return;
    Arduino_Canvas* c = m_display->getCanvas();

    // 1. Fondo del Gamepad (Inferior 320x192)
    c->fillRect(0, 288, 320, 192, 0x18C3); // Gris azulado oscuro Game Boy
    c->drawFastHLine(0, 288, 320, 0x4A69); // Línea divisoria superior

    // 2. Barra de Estado / Decoración (Y: 289..312)
    c->setTextSize(1);
    c->setTextColor(0xFD20); // Naranja retro
    c->setCursor(12, 296);
    c->print("GAME BOY ");
    c->setTextColor(0x07FF); // Cyan
    c->print("COLOR");

    // Botón SALIR (Top Right del Gamepad: 245x292, W:65, H:20)
    c->fillRect(245, 292, 65, 20, 0xC800); // Rojo oscuro
    c->drawRect(245, 292, 65, 20, 0xF800); // Borde rojo brillante
    c->setTextColor(0xFFFF);
    c->setCursor(258, 298);
    c->print("SALIR");

    // 3. D-Pad (Cruceta izquierda)
    // Fondo de la cruz
    c->fillRect(55, 325, 30, 90, 0x2104); // Vertical
    c->fillRect(25, 355, 90, 30, 0x2104); // Horizontal
    c->drawRect(55, 325, 30, 90, 0x632C);
    c->drawRect(25, 355, 90, 30, 0x632C);

    // Flechas / Indicadores
    c->setTextSize(2);
    c->setTextColor(0xAD55);
    c->setCursor(63, 332); c->print("^"); // Arriba
    c->setCursor(63, 392); c->print("v"); // Abajo
    c->setCursor(32, 362); c->print("<"); // Izq
    c->setCursor(95, 362); c->print(">"); // Der

    // 4. Botones de Acción (Derecha en diagonal)
    // Botón B (Inferior Izq de la diagonal: cx=205, cy=395)
    c->fillCircle(205, 395, 20, 0x900B); // Magenta oscuro
    c->drawCircle(205, 395, 20, 0xF81F); // Magenta brillante
    c->setTextSize(2);
    c->setTextColor(0xFFFF);
    c->setCursor(199, 388);
    c->print("B");

    // Botón A (Superior Der de la diagonal: cx=265, cy=355)
    c->fillCircle(265, 355, 20, 0x900B);
    c->drawCircle(265, 355, 20, 0xF81F);
    c->setCursor(259, 348);
    c->print("A");

    // 5. Botones SELECT y START (Inferior Centro Y: 445..468)
    // SELECT
    c->fillRoundRect(80, 446, 65, 22, 6, 0x3186);
    c->drawRoundRect(80, 446, 65, 22, 6, 0x7BEF);
    c->setTextSize(1);
    c->setTextColor(0xFFFF);
    c->setCursor(92, 453);
    c->print("SELECT");

    // START
    c->fillRoundRect(175, 446, 65, 22, 6, 0x3186);
    c->drawRoundRect(175, 446, 65, 22, 6, 0x7BEF);
    c->setCursor(190, 453);
    c->print("START");
}

void CartridgeGamepad::drawLandscapeDoom() {
    if (!m_display || !m_display->getCanvas()) return;
    Arduino_Canvas* c = m_display->getCanvas();

    // 1. Botón SALIR (Top Right: 420x5, W:55, H:38)
    c->fillRect(420, 5, 55, 38, 0xF800);
    c->drawRect(420, 5, 55, 38, 0xFFFF);
    c->setTextSize(1);
    c->setTextColor(0xFFFF);
    c->setCursor(430, 20);
    c->print("SALIR");

    // 2. Botón ENTER / MENU (Top Center: 160x5, W:160, H:38)
    c->fillRect(160, 5, 160, 38, 0x03E0);
    c->drawRect(160, 5, 160, 38, 0x07E0);
    c->setTextSize(2);
    c->setTextColor(0xFFFF);
    c->setCursor(185, 16);
    c->print("ENTER / OK");

    // 3. Botón ABRIR / USE (Bottom Center: 160x275, W:160, H:38)
    c->fillRect(160, 275, 160, 38, 0x001F);
    c->drawRect(160, 275, 160, 38, 0x07FF);
    c->setTextSize(2);
    c->setTextColor(0xFFFF);
    c->setCursor(170, 286);
    c->print("ABRIR / USE");

    // 4. Cruceta Izquierda (D-Pad Movimiento)
    c->fillRect(15, 65, 50, 40, 0x2104);
    c->drawRect(15, 65, 50, 40, 0xCE79);
    c->setTextSize(2);
    c->setTextColor(0xFFFF);
    c->setCursor(33, 76); c->print("^");

    c->fillRect(15, 205, 50, 40, 0x2104);
    c->drawRect(15, 205, 50, 40, 0xCE79);
    c->setCursor(33, 216); c->print("v");

    c->fillRect(5, 125, 33, 60, 0x2104);
    c->drawRect(5, 125, 33, 60, 0xCE79);
    c->setCursor(12, 146); c->print("<");

    c->fillRect(42, 125, 33, 60, 0x2104);
    c->drawRect(42, 125, 33, 60, 0xCE79);
    c->setCursor(50, 146); c->print(">");

    // 5. Panel Derecho (Giro y Disparo)
    c->fillRect(405, 65, 33, 50, 0x2104);
    c->drawRect(405, 65, 33, 50, 0xCE79);
    c->setCursor(412, 80); c->print("L");

    c->fillRect(442, 65, 33, 50, 0x2104);
    c->drawRect(442, 65, 33, 50, 0xCE79);
    c->setCursor(450, 80); c->print("R");

    c->fillRect(405, 135, 70, 120, 0xC800);
    c->drawRect(405, 135, 70, 120, 0xF800);
    c->setTextSize(2);
    c->setTextColor(0xFFFF);
    c->setCursor(415, 185);
    c->print("FIRE");
}

void CartridgeGamepad::draw(bool force) {
    if (m_drawn && !force) return;
    if (m_layout == LAYOUT_PORTRAIT_GBC) {
        drawPortraitGBC();
    } else {
        drawLandscapeDoom();
    }
    m_drawn = true;
}

uint16_t CartridgeGamepad::readPortraitGBC(int16_t x, int16_t y) {
    uint16_t btns = PAD_NONE;

    // 1. Botón SALIR (230..320, 288..320)
    if (x >= 230 && x <= 320 && y >= 288 && y <= 320) {
        return PAD_EXIT;
    }

    // 2. D-Pad (Centro ~ 70, 375 con tolerancias amplias)
    if (x >= 0 && x <= 140 && y >= 315 && y <= 440) {
        int dx = x - 70;
        int dy = y - 375;
        if (abs(dy) > abs(dx)) {
            if (dy < -10) btns |= PAD_UP;
            else if (dy > 10) btns |= PAD_DOWN;
        } else {
            if (dx < -10) btns |= PAD_LEFT;
            else if (dx > 10) btns |= PAD_RIGHT;
        }
    }

    // 3. Botones de Acción (Zona Derecha)
    // Botón B (Centro ~ 205, 395)
    int dxB = x - 205;
    int dyB = y - 395;
    if ((dxB * dxB + dyB * dyB) <= (32 * 32)) {
        btns |= PAD_B;
    }

    // Botón A (Centro ~ 265, 355)
    int dxA = x - 265;
    int dyA = y - 355;
    if ((dxA * dxA + dyA * dyA) <= (32 * 32)) {
        btns |= PAD_A;
    }

    // 4. Botones SELECT / START
    if (y >= 435 && y <= 480) {
        if (x >= 60 && x <= 155) {
            btns |= PAD_SELECT;
        } else if (x >= 165 && x <= 260) {
            btns |= PAD_START;
        }
    }

    return btns;
}

uint16_t CartridgeGamepad::readLandscapeDoom(int16_t x, int16_t y) {
    uint16_t btns = PAD_NONE;

    if (x >= 400 && y <= 50) return PAD_EXIT;
    if (y <= 50 && x >= 150 && x <= 330) return PAD_START;  // Enter / Menu
    if (y >= 270 && x >= 150 && x <= 330) return PAD_SELECT; // Use / Open

    // Cruceta Izquierda
    if (x < 80) {
        if (y < 110) btns |= PAD_UP;
        else if (y > 190) btns |= PAD_DOWN;
        else if (x < 40) btns |= PAD_LEFT;
        else btns |= PAD_RIGHT;
    }

    // Panel Derecho
    if (x > 400) {
        if (y >= 120 && y <= 260) btns |= PAD_A; // Fire
        else if (y < 120) {
            if (x < 440) btns |= PAD_LEFT; // Giro L
            else btns |= PAD_RIGHT;        // Giro R
        }
    }

    return btns;
}

uint16_t CartridgeGamepad::read() {
    if (!m_touch) return PAD_NONE;

    TouchPoint tp;
    if (m_touch->read(tp) && tp.touched) {
        uint16_t btns = PAD_NONE;
        if (m_layout == LAYOUT_PORTRAIT_GBC) {
            btns = readPortraitGBC(tp.x, tp.y);
        } else {
            btns = readLandscapeDoom(tp.x, tp.y);
        }
        m_lastButtons = btns;
        return btns;
    }

    m_lastButtons = PAD_NONE;
    return PAD_NONE;
}

bool CartridgeGamepad::handleExit() {
    if (m_lastButtons & PAD_EXIT) {
        exitToOS();
        return true;
    }
    return false;
}
