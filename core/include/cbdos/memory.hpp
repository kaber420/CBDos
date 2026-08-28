#pragma once

#include <cstddef>
#include <cstdlib>

#if defined(ESP_PLATFORM)
#include <esp_heap_caps.h>
#endif

namespace cbdos {
namespace mem {

inline void* alloc_psram(size_t size) {
#if defined(ESP_PLATFORM)
    void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!ptr) {
        ptr = malloc(size);
    }
    return ptr;
#else
    return malloc(size);
#endif
}

inline void* realloc_psram(void* ptr, size_t size) {
#if defined(ESP_PLATFORM)
    void* p = heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p) {
        p = realloc(ptr, size);
    }
    return p;
#else
    return realloc(ptr, size);
#endif
}

inline void* alloc_dma(size_t size) {
#if defined(ESP_PLATFORM)
    return heap_caps_malloc(size, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
#else
    return malloc(size);
#endif
}

inline void* alloc_internal(size_t size) {
#if defined(ESP_PLATFORM)
    return heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#else
    return malloc(size);
#endif
}

inline void* realloc_internal(void* ptr, size_t size) {
#if defined(ESP_PLATFORM)
    return heap_caps_realloc(ptr, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#else
    return realloc(ptr, size);
#endif
}

inline void free_mem(void* ptr) {
    if (ptr) {
        ::free(ptr);
    }
}

} // namespace mem
} // namespace cbdos
