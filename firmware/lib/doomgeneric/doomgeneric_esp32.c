// ==========================================================================
// doomgeneric_esp32.c — HAL de integración DOOM ↔ espOS32 (ESP32-S3)
//
// Implementa las 5+1 funciones callback que doomgeneric necesita:
//   DG_Init, DG_DrawFrame, DG_SleepMs, DG_GetTicksMs, DG_GetKey,
//   DG_SetWindowTitle
//
// Copyright 2024 espOS32 project — Motor DOOM bajo GPL2
// ==========================================================================

#include "doomgeneric.h"
#include "doomkeys.h"

#include <stdio.h>
#include <string.h>

#ifdef ARDUINO
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#else
#include <unistd.h>
#include <time.h>
#endif

// ─── Estado global compartido con DoomView.cpp ─────────────────────────

// Canvas RGB565 en PSRAM (escrito por DG_DrawFrame, leído por LVGL)
extern uint16_t* g_doomCanvasBuf;

// Bitmask de zonas táctiles (escrito por el gamepad handler)
extern volatile uint16_t g_doomZoneBits;

// Flag de ejecución (false → la tarea de render sale del loop)
extern volatile bool g_doomRunning;

// ─── Cola de teclas DOOM (ring buffer simple) ──────────────────────────

#define KEY_QUEUE_SIZE 16

typedef struct {
    int pressed;
    unsigned char key;
} key_entry_t;

static key_entry_t s_keyQueue[KEY_QUEUE_SIZE];
static volatile int s_keyQueueRead = 0;
static volatile int s_keyQueueWrite = 0;

static void pushKey(int pressed, unsigned char key) {
    int next = (s_keyQueueWrite + 1) % KEY_QUEUE_SIZE;
    if (next == s_keyQueueRead) return; // queue full, drop
    s_keyQueue[s_keyQueueWrite].pressed = pressed;
    s_keyQueue[s_keyQueueWrite].key = key;
    s_keyQueueWrite = next;
}

void Doom_QueueKey(int pressed, unsigned char key) {
    pushKey(pressed, key);
}

// Estado anterior de zonas (para detectar press/release)
static uint16_t s_prevZoneBits = 0;

// Mapeo de bits de zona a keycodes DOOM
static const unsigned char s_zoneToKey[11] = {
    KEY_STRAFE_L,    // bit 0 = Strafe Izquierda
    KEY_UPARROW,     // bit 1 = Adelante (Arriba en menú)
    KEY_STRAFE_R,    // bit 2 = Strafe Derecha
    KEY_LEFTARROW,   // bit 3 = Girar Izquierda
    KEY_FIRE,        // bit 4 = Disparo (Ctrl)
    KEY_RIGHTARROW,  // bit 5 = Girar Derecha
    KEY_ENTER,       // bit 6 = ENTER (Entrar a New Game / Menú)
    KEY_USE,         // bit 7 = USAR / ABRIR PUERTAS (Spacebar)
    KEY_DOWNARROW,   // bit 8 = Atrás (Abajo en menú)
    KEY_ESCAPE,      // bit 9 = ESCAPE / Menú Principal de DOOM
    KEY_RSHIFT       // bit 10 = Run / Speed (Shift)
};

// Procesa los cambios en g_doomZoneBits y genera eventos press/release
static void processZoneBits(void) {
    uint16_t current = g_doomZoneBits;
    uint16_t changed = current ^ s_prevZoneBits;

    for (int i = 0; i < 11; i++) {
        uint16_t mask = (1 << i);
        if (changed & mask) {
            pushKey((current & mask) ? 1 : 0, s_zoneToKey[i]);
        }
    }
    s_prevZoneBits = current;
}

// ─── DG_Init ───────────────────────────────────────────────────────────

void DG_Init(void) {
    s_keyQueueRead = 0;
    s_keyQueueWrite = 0;
    s_prevZoneBits = 0;

#ifdef ARDUINO
    printf("[DOOM] DG_Init: motor inicializado\n");
#endif
}

// ─── DG_DrawFrame ──────────────────────────────────────────────────────
// DG_ScreenBuffer contiene ARGB8888 (32-bit). Convertimos a RGB565 en el
// canvas PSRAM para que LVGL lo muestre directamente.

void DG_DrawFrame(void) {
    if (!g_doomCanvasBuf || !DG_ScreenBuffer) return;

    const uint32_t* src = (const uint32_t*)DG_ScreenBuffer;
    uint16_t* dst = g_doomCanvasBuf;
    int totalPixels = DOOMGENERIC_RESX * DOOMGENERIC_RESY;

    for (int i = 0; i < totalPixels; i++) {
        uint32_t argb = src[i];
        uint8_t r = (argb >> 16) & 0xFF;
        uint8_t g = (argb >> 8)  & 0xFF;
        uint8_t b =  argb        & 0xFF;
        // RGB565: RRRRRGGG GGGBBBBB (big endian para lv_canvas)
        dst[i] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    }
}

// ─── DG_SleepMs ────────────────────────────────────────────────────────

void DG_SleepMs(uint32_t ms) {
#ifdef ARDUINO
    vTaskDelay(pdMS_TO_TICKS(ms));
#else
    usleep(ms * 1000);
#endif
}

// ─── DG_GetTicksMs ─────────────────────────────────────────────────────

uint32_t DG_GetTicksMs(void) {
#ifdef ARDUINO
    return (uint32_t)millis();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
#endif
}

// ─── DG_GetKey ─────────────────────────────────────────────────────────
// Primero procesamos los cambios en las zonas táctiles, luego sacamos
// de la cola.

int DG_GetKey(int* pressed, unsigned char* doomKey) {
    // Convertir estado de zonas a eventos
    processZoneBits();

    // Sacar de la cola
    if (s_keyQueueRead == s_keyQueueWrite) {
        return 0; // no hay teclas
    }
    *pressed = s_keyQueue[s_keyQueueRead].pressed;
    *doomKey = s_keyQueue[s_keyQueueRead].key;
    s_keyQueueRead = (s_keyQueueRead + 1) % KEY_QUEUE_SIZE;
    return 1;
}

// ─── DG_SetWindowTitle ─────────────────────────────────────────────────

void DG_SetWindowTitle(const char* title) {
#ifdef ARDUINO
    printf("[DOOM] Title: %s\n", title);
#else
    (void)title;
#endif
}
