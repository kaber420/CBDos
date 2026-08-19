#ifndef LV_CONF_H
#define LV_CONF_H

#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
#  define LV_COLOR_DEPTH 16
#  define LV_COLOR_16_SWAP 0
#else
#  define LV_COLOR_DEPTH 16
#  define LV_COLOR_16_SWAP 0
#endif

#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR 0

#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN

#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
#define LV_USE_SDL 1
#else
#define LV_USE_SDL 0
#endif

#define LV_HOR_RES_MAX 480
#define LV_VER_RES_MAX 320

#define LV_USE_LABEL 1
#define LV_USE_BTN 1
#define LV_USE_SLIDER 1
#define LV_USE_SWITCH 1
#define LV_USE_TEXTAREA 1
#define LV_USE_DROPDOWN 1
#define LV_USE_QRCODE 1

#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_DEFAULT &lv_font_montserrat_16

#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1

#define LV_USE_DEMO_WIDGETS 0
#define LV_USE_DEMO_BENCHMARK 0
#define LV_USE_DEMO_STRESS 0

/* --- LVGL 9.5 Memory Pool Allocator (PSRAM 8MB en ESP32-S3) --- */
#define LV_USE_STDLIB_MALLOC LV_STDLIB_BUILTIN
#define LV_MEM_SIZE          (1024 * 1024U) /* 1 MB de heap exclusivo para LVGL 9.5 en PSRAM */
#define LV_MEM_POOL_INCLUDE  <esp_heap_caps.h>
#define LV_MEM_POOL_ALLOC(size) heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)



/* --- Image and GIF Decoders --- */
#define LV_USE_BMP 1
#define LV_USE_TJPGD 1
#define LV_USE_PNG 1
#define LV_USE_GIF 1

#endif /* LV_CONF_H */