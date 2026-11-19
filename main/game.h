#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_random.h"
#include "input.h"

// Every game implements the same 3 functions — the menu doesn't need to know anything else.
// This is "polymorphism in C": a struct of function pointers (like a C++ interface / vtable).
typedef struct {
    const char *name;                      // also used as the NVS key for the high score
    void (*reset)(void);                   // start a new game
    bool (*step)(const input_t *in);       // advance one frame + draw; return false = game over
    int  (*score)(void);
} game_t;

extern const game_t GAME_ROCKS, GAME_BOUNCE, GAME_SNAKE, GAME_RACING, GAME_SHOOTER, GAME_DOOM;

// helpers shared by games
static inline int rnd(int n) { return (int)(esp_random() % (uint32_t)n); }   // hardware RNG

static inline bool overlap(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh)
{
    return ax < bx + bw && bx < ax + aw && ay < by + bh && by < ay + ah;      // AABB test
}
