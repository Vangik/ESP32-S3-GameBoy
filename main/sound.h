#pragma once
#include <stdint.h>

typedef struct { uint16_t freq; uint8_t frames; } note_t;            // freq 0 = pause; 1 frame = 40 ms
typedef struct { const note_t *notes; uint8_t len, prio; } sfx_t;   // higher prio is not cut by lower

void sound_init(void);
void sound_play(const sfx_t *s);
void sound_tick(void);      // call once per frame

extern const sfx_t SFX_BLIP, SFX_SHOT, SFX_GUN, SFX_BOUNCE, SFX_BRICK, SFX_EAT,
                   SFX_BOOM, SFX_HURT, SFX_CRASH, SFX_LEVEL, SFX_START, SFX_OVER;
