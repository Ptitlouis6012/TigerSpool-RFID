#pragma once
// LVGL's heap lives in PSRAM. The board has 8 MB of it and only 320 KB of
// internal DRAM, which the network stacks and the MQTT buffers already want.
//
// heap_caps_malloc(MALLOC_CAP_SPIRAM) rather than ps_malloc(): ps_malloc falls
// back to internal RAM silently when PSRAM is full, which turns an out-of-memory
// bug into an intermittent one that only shows up under load.
#include <esp_heap_caps.h>
#include <stddef.h>

static inline void* lv_psram_malloc(size_t n)            { return heap_caps_malloc(n, MALLOC_CAP_SPIRAM); }
static inline void  lv_psram_free(void* p)               { heap_caps_free(p); }
static inline void* lv_psram_realloc(void* p, size_t n)  { return heap_caps_realloc(p, n, MALLOC_CAP_SPIRAM); }
