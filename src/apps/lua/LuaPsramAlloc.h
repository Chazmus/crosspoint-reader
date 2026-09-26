#pragma once

#include <cstddef>
#include <cstdlib>
#include <esp_heap_caps.h>

inline void* luaPsramAlloc(void* ud, void* ptr, size_t osize, size_t nsize) {
  (void)ud;
  (void)osize;
  if (nsize == 0) {
    free(ptr);
    return nullptr;
  }
  // Allocate in 8MB Octal PSRAM to isolate Lua VM state from internal DRAM
  void* p = heap_caps_realloc(ptr, nsize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!p) {
    p = realloc(ptr, nsize);
  }
  return p;
}
