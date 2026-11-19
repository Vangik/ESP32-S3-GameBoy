// ROCKS — shoot falling rocks. LEFT/RIGHT move, FIRE shoots.
#include <stdio.h>
#include <string.h>
#include "game.h"
#include "display.h"
#include "sound.h"

#define MAX_ROCKS 8
#define MAX_SHOTS 4
#define SHIP_W    9
#define SHIP_Y    58

typedef struct { bool on; int x, y, size, vy; } rock_t;   // y, vy in 1/4 px (fixed point)
typedef struct { bool on; int x, y; } shot_t;

static rock_t rocks[MAX_ROCKS];
static shot_t shots[MAX_SHOTS];
static int ship_x, score;

static void reset(void)
{
    memset(rocks, 0, sizeof rocks);
    memset(shots, 0, sizeof shots);
    ship_x = (SCR_W - SHIP_W) / 2;
    score = 0;
}

static bool step(const input_t *in)
{
    if (in->l && ship_x > 0)              ship_x -= 2;
    if (in->r && ship_x < SCR_W - SHIP_W) ship_x += 2;

    if (in->fp)
        for (int i = 0; i < MAX_SHOTS; i++)
            if (!shots[i].on) {
                shots[i] = (shot_t){ true, ship_x + SHIP_W / 2, SHIP_Y - 3 };
                sound_play(&SFX_SHOT);
                break;
            }
    for (int i = 0; i < MAX_SHOTS; i++)
        if (shots[i].on && (shots[i].y -= 4) < -3) shots[i].on = false;

    int chance = 4 + score / 5;                  // spawn chance per frame grows with score
    if (chance > 30) chance = 30;
    if (rnd(100) < chance)
        for (int i = 0; i < MAX_ROCKS; i++)
            if (!rocks[i].on) {
                int s = 4 + rnd(4);
                rocks[i] = (rock_t){ true, rnd(SCR_W - s), -s * 4, s, 2 + rnd(3) + score / 15 };
                break;
            }

    for (int i = 0; i < MAX_ROCKS; i++) {
        rock_t *r = &rocks[i];
        if (!r->on) continue;
        r->y += r->vy;
        int ry = r->y / 4;
        if (ry > SCR_H) { r->on = false; continue; }
        for (int j = 0; j < MAX_SHOTS; j++)
            if (shots[j].on && overlap(shots[j].x, shots[j].y, 1, 3, r->x, ry, r->size, r->size)) {
                shots[j].on = r->on = false;
                score++;
                sound_play(&SFX_BOOM);
                break;
            }
        if (r->on && overlap(ship_x, SHIP_Y, SHIP_W, 6, r->x, ry, r->size, r->size)) {
            sound_play(&SFX_CRASH);
            return false;
        }
    }

    for (int i = 0; i < MAX_ROCKS; i++)
        if (rocks[i].on) gfx_rect(rocks[i].x, rocks[i].y / 4, rocks[i].size, rocks[i].size, C_ON);
    for (int i = 0; i < MAX_SHOTS; i++)
        if (shots[i].on) gfx_rect(shots[i].x, shots[i].y, 1, 3, C_ON);
    gfx_rect(ship_x + 4, SHIP_Y,     1, 2, C_ON);
    gfx_rect(ship_x + 2, SHIP_Y + 2, 5, 2, C_ON);
    gfx_rect(ship_x,     SHIP_Y + 4, 9, 2, C_ON);

    char b[16];
    snprintf(b, sizeof b, "%d", score);
    gfx_text(1, 1, b, 1, C_ON);
    return true;
}

static int get_score(void) { return score; }

const game_t GAME_ROCKS = { "ROCKS", reset, step, get_score };
