#pragma once

#include <cstddef>
#include <cstdlib>

namespace cbdos {
namespace mem {

void* alloc_psram(size_t size);
void* realloc_psram(void* ptr, size_t size);
void* alloc_dma(size_t size);
void* alloc_internal(size_t size);
void* realloc_internal(void* ptr, size_t size);
void free_mem(void* ptr);

} // namespace mem
} // namespace cbdos
