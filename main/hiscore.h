#pragma once
#include <stdint.h>

// High scores in NVS (a key-value store in flash) — they survive power-off.
void    hiscore_init(void);
int32_t hiscore_load(const char *key);
void    hiscore_save(const char *key, int32_t value);
