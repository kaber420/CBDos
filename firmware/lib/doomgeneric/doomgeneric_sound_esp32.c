// ==========================================================================
// doomgeneric_sound_esp32.c — Driver de Sonido I2S con FreeRTOS en Core 0
//
// Mezclador de audio continuo e independiente en el Núcleo 0 (Core 0) a 22.050 Hz.
// Pacing por hardware DMA: velocidad 100% real, 0ms retardo, sin eco ni lentitud.
// ==========================================================================

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "doomtype.h"
#include "i_sound.h"
#include "sounds.h"
#include "w_wad.h"
#include "z_zone.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/i2s.h>
#include <esp_err.h>

// ─── Configuración I2S ───────────────────────────────────────────────────
#ifndef I2S_BCLK
#define I2S_BCLK 42
#endif

#ifndef I2S_LRC
#define I2S_LRC  2
#endif

#ifndef I2S_DOUT
#define I2S_DOUT 41
#endif

#define I2S_SAMPLE_RATE     22050
#define AUDIO_BLOCK_SAMPLES 256
#define MAX_CHANNELS        8

// ─── Cabecera de sonido original de DOOM en WAD ──────────────────────────
#pragma pack(push, 1)
typedef struct {
    uint16_t format;       // Debe ser 3
    uint16_t sample_rate;  // 11025 o 22050
    uint32_t num_samples;  // Cantidad de muestras de 8-bit
} doom_sfx_header_t;
#pragma pack(pop)

// ─── Estructura de Canal del Mezclador ────────────────────────────────────
typedef struct {
    const uint8_t* data;
    uint32_t length;
    uint32_t pos_fixed;  // Posición en punto fijo 16.16
    uint32_t step;       // Incremento por muestra (16.16)
    int volume;
    volatile boolean active;
} mixer_channel_t;

static mixer_channel_t s_channels[MAX_CHANNELS];
static volatile boolean s_soundRunning = false;
static TaskHandle_t s_audioTaskHandle = NULL;

// ─── Lista de Dispositivos Soportados ────────────────────────────────────
static snddevice_t s_soundDevices[] = {
    SNDDEVICE_SB,
    SNDDEVICE_PAS,
    SNDDEVICE_GUS,
    SNDDEVICE_WAVEBLASTER,
    SNDDEVICE_SOUNDCANVAS,
    SNDDEVICE_GENMIDI,
    SNDDEVICE_AWE32,
    SNDDEVICE_ADLIB,
    SNDDEVICE_PCSPEAKER
};

// ─── Tarea de Audio en Núcleo 0 (Core 0) ──────────────────────────────────
static void doom_audio_task(void *param) {
    (void)param;
    int16_t mixBuf[AUDIO_BLOCK_SAMPLES * 2]; // Estéreo 16-bit

    printf("[Audio DOOM] Tarea de audio iniciada en Core %d con prioridad %d\n",
           xPortGetCoreID(), (int)uxTaskPriorityGet(NULL));

    while (s_soundRunning) {
        memset(mixBuf, 0, sizeof(mixBuf));

        for (int ch = 0; ch < MAX_CHANNELS; ch++) {
            if (!s_channels[ch].active || !s_channels[ch].data) continue;

            int vol = s_channels[ch].volume; // 0 a 127
            uint32_t pos_fixed = s_channels[ch].pos_fixed;
            uint32_t step = s_channels[ch].step;
            uint32_t len = s_channels[ch].length;
            const uint8_t* src = s_channels[ch].data;

            for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
                uint32_t pos = pos_fixed >> 16;
                if (pos >= len) {
                    s_channels[ch].active = false;
                    break;
                }

                // Convertir 8-bit unsigned (0..255, centro 128) a 16-bit signed
                int32_t sample = ((int32_t)src[pos] - 128) << 8;
                sample = (sample * vol) / 127;

                // Mezclar a canal izquierdo y derecho
                int32_t left = (int32_t)mixBuf[i * 2] + sample;
                int32_t right = (int32_t)mixBuf[i * 2 + 1] + sample;

                // Clipping
                if (left > 32767) left = 32767;
                else if (left < -32768) left = -32768;

                if (right > 32767) right = 32767;
                else if (right < -32768) right = -32768;

                mixBuf[i * 2] = (int16_t)left;
                mixBuf[i * 2 + 1] = (int16_t)right;

                pos_fixed += step;
            }
            s_channels[ch].pos_fixed = pos_fixed;
        }

        size_t bytes_written = 0;
        // i2s_write con portMAX_DELAY hace que el hardware DMA marque el reloj
        // exacto de 22.050 Hz de forma continua sin que la CPU consuma recursos.
        i2s_write(I2S_NUM_0, mixBuf, sizeof(mixBuf), &bytes_written, portMAX_DELAY);
    }

    vTaskDelete(NULL);
}

// ─── Funciones del Módulo de Sonido ──────────────────────────────────────

static boolean DG_Sound_Init(boolean use_sfx_prefix) {
    (void)use_sfx_prefix;
    printf("[Audio DOOM] Inicializando driver I2S (BCLK=%d, LRC=%d, DOUT=%d) a %d Hz...\n",
           I2S_BCLK, I2S_LRC, I2S_DOUT, I2S_SAMPLE_RATE);

    i2s_driver_uninstall(I2S_NUM_0);

    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = I2S_SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = 0,
        .dma_buf_count = 6,
        .dma_buf_len = AUDIO_BLOCK_SAMPLES,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };

    esp_err_t err = i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        printf("[Audio DOOM] Error al instalar i2s_driver: 0x%x\n", err);
        return false;
    }

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCLK,
        .ws_io_num = I2S_LRC,
        .data_out_num = I2S_DOUT,
        .data_in_num = I2S_PIN_NO_CHANGE
    };

    err = i2s_set_pin(I2S_NUM_0, &pin_config);
    if (err != ESP_OK) {
        printf("[Audio DOOM] Error al configurar pines I2S: 0x%x\n", err);
        return false;
    }

    i2s_zero_dma_buffer(I2S_NUM_0);
    memset(s_channels, 0, sizeof(s_channels));

    // Iniciar tarea de audio en Core 0 con prioridad 5
    s_soundRunning = true;
    BaseType_t res = xTaskCreatePinnedToCore(
        doom_audio_task,
        "doom_audio",
        4096,
        NULL,
        5,
        &s_audioTaskHandle,
        0 // Core 0 dedicado
    );

    if (res != pdPASS) {
        printf("[Audio DOOM] Error al crear tarea de audio FreeRTOS en Core 0!\n");
        return false;
    }

    printf("[Audio DOOM] Driver y tarea de audio Core 0 listos.\n");
    return true;
}

static void DG_Sound_Shutdown(void) {
    if (s_soundRunning) {
        s_soundRunning = false;
        vTaskDelay(pdMS_TO_TICKS(50));
        i2s_zero_dma_buffer(I2S_NUM_0);
        i2s_driver_uninstall(I2S_NUM_0);
        printf("[Audio DOOM] Driver de audio desinstalado.\n");
    }
}

static int DG_Sound_GetSfxLumpNum(sfxinfo_t *sfxinfo) {
    char namebuf[12];
    snprintf(namebuf, sizeof(namebuf), "ds%s", sfxinfo->name);
    return W_CheckNumForName(namebuf);
}

static void DG_Sound_UpdateSoundParams(int channel, int vol, int sep) {
    (void)sep;
    if (channel >= 0 && channel < MAX_CHANNELS) {
        s_channels[channel].volume = vol;
    }
}

static int DG_Sound_StartSound(sfxinfo_t *sfxinfo, int channel, int vol, int sep) {
    (void)sep;
    if (channel < 0 || channel >= MAX_CHANNELS || !sfxinfo) {
        return -1;
    }

    if (sfxinfo->lumpnum < 0) {
        sfxinfo->lumpnum = DG_Sound_GetSfxLumpNum(sfxinfo);
        if (sfxinfo->lumpnum < 0) {
            return -1;
        }
    }

    if (!sfxinfo->driver_data) {
        sfxinfo->driver_data = W_CacheLumpNum(sfxinfo->lumpnum, PU_STATIC);
    }

    const uint8_t* raw = (const uint8_t*)sfxinfo->driver_data;
    if (!raw) {
        return -1;
    }

    int lumplen = W_LumpLength(sfxinfo->lumpnum);
    if (lumplen <= 16) {
        return -1;
    }

    const doom_sfx_header_t* header = (const doom_sfx_header_t*)raw;
    uint32_t srate = header->sample_rate;
    if (srate < 4000 || srate > 48000) {
        srate = 11025;
    }

    s_channels[channel].active = false; // pausa atómica momentánea
    s_channels[channel].data = raw + 8;
    s_channels[channel].length = (uint32_t)(lumplen - 8);
    s_channels[channel].pos_fixed = 0;
    s_channels[channel].step = (srate << 16) / I2S_SAMPLE_RATE;
    s_channels[channel].volume = vol;
    s_channels[channel].active = true;

    return channel;
}

static void DG_Sound_StopSound(int channel) {
    if (channel >= 0 && channel < MAX_CHANNELS) {
        s_channels[channel].active = false;
    }
}

static boolean DG_Sound_SoundIsPlaying(int channel) {
    if (channel >= 0 && channel < MAX_CHANNELS) {
        return s_channels[channel].active;
    }
    return false;
}

static void DG_Sound_CacheSounds(sfxinfo_t *sounds, int num_sounds) {
    (void)sounds;
    (void)num_sounds;
}

// La mezcla ahora corre continuamente en Core 0, UpdateSound es un tick de sincronización ligero
static void DG_Sound_Update(void) {
    // No-op: la tarea doom_audio_task en Core 0 alimenta continuamente el DMA
}

// ─── Variables para compatibilidad con i_sound.c ─────────────────────────
int use_libsamplerate = 0;
float libsamplerate_scale = 1.0f;

// ─── Módulo de Música (Stub) ──────────────────────────────────────────────
static boolean DG_Music_Init(void) { return true; }
static void DG_Music_Shutdown(void) {}
static void DG_Music_SetMusicVolume(int volume) { (void)volume; }
static void DG_Music_PauseMusic(void) {}
static void DG_Music_ResumeMusic(void) {}
static void *DG_Music_RegisterSong(void *data, int len) { (void)data; (void)len; return NULL; }
static void DG_Music_UnRegisterSong(void *handle) { (void)handle; }
static void DG_Music_PlaySong(void *handle, boolean looping) { (void)handle; (void)looping; }
static void DG_Music_StopSong(void) {}
static boolean DG_Music_MusicIsPlaying(void) { return false; }
static void DG_Music_Poll(void) {}

music_module_t DG_music_module = {
    s_soundDevices,
    sizeof(s_soundDevices) / sizeof(snddevice_t),
    DG_Music_Init,
    DG_Music_Shutdown,
    DG_Music_SetMusicVolume,
    DG_Music_PauseMusic,
    DG_Music_ResumeMusic,
    DG_Music_RegisterSong,
    DG_Music_UnRegisterSong,
    DG_Music_PlaySong,
    DG_Music_StopSong,
    DG_Music_MusicIsPlaying,
    DG_Music_Poll
};

// ─── Estructura Global del Módulo de Sonido ──────────────────────────────
sound_module_t DG_sound_module = {
    s_soundDevices,
    sizeof(s_soundDevices) / sizeof(snddevice_t),
    DG_Sound_Init,
    DG_Sound_Shutdown,
    DG_Sound_GetSfxLumpNum,
    DG_Sound_Update,
    DG_Sound_UpdateSoundParams,
    DG_Sound_StartSound,
    DG_Sound_StopSound,
    DG_Sound_SoundIsPlaying,
    DG_Sound_CacheSounds
};
