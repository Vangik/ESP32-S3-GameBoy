// RACING — 3-lane highway. LEFT/RIGHT change lanes, hold FIRE for nitro (faster = more points).
#include <stdio.h>
#include <string.h>
#include "game.h"
#include "display.h"
#include "sound.h"

#define ROAD_L     31
#define LANE_W     22
#define ROAD_R     (ROAD_L + 3 * LANE_W)   // 97
#define CAR_W      9
#define CAR_H      12
#define PLAYER_Y   50
#define MAX_CARS   6
#define TRAFFIC_V  12                      // other cars' own speed (1/16 px per frame)

static const char *const CAR[CAR_H] = {
    "..#####..",
    ".#.....#.",
    "##.###.##",
    "##.....##",
    ".#.....#.",
    ".#.###.#.",
    ".#.###.#.",
    ".#.....#.",
    "##.....##",
    "##.###.##",
    ".#.....#.",
    "..#####..",
};
static const char *const TREE[5] = { "..#..", ".###.", "#####", "..#..", "..#.." };

typedef struct { bool on; int lane, y; } car_t;     // y in 1/16 px
static car_t cars[MAX_CARS];
static int lane, px, scroll, dist, spawn_cd;
static int cur_v;                                    // current speed, for the HUD

static int lane_x(int l) { return ROAD_L + l * LANE_W + LANE_W / 2; }   // lane centre

static void reset(void)
{
    memset(cars, 0, sizeof cars);
    lane = 1; px = lane_x(1);
    scroll = dist = 0;
    spawn_cd = 20;
}

static void spawn(int l)
{
    for (int i = 0; i < MAX_CARS; i++)
        if (!cars[i].on) { cars[i] = (car_t){ true, l, -CAR_H * 16 }; return; }
}

static void draw_car(int x, int y)
{
    gfx_rect(x, y, CAR_W, CAR_H, C_OFF);   // black backing so lane dashes don't show through
    gfx_sprite(x, y, CAR, CAR_H, C_ON);
}

static bool step(const input_t *in)
{
    if (in->lp && lane > 0) lane--;
    if (in->rp && lane < 2) lane++;
    int tx = lane_x(lane);                  // slide smoothly towards the target lane
    if (px < tx) { px += 4; if (px > tx) px = tx; }
    if (px > tx) { px -= 4; if (px < tx) px = tx; }

    int base = 32 + dist / 3000;            // speed slowly rises with distance
    if (base > 72) base = 72;
    cur_v = in->f ? base * 3 / 2 : base;    // nitro
    scroll += cur_v;
    dist += cur_v;

    for (int i = 0; i < MAX_CARS; i++) {
        car_t *c = &cars[i];
        if (!c->on) continue;
        c->y += cur_v - TRAFFIC_V;          // relative speed: we're faster, so they come towards us
        if ((c->y >> 4) > SCR_H) { c->on = false; continue; }
        if (overlap(px - CAR_W / 2, PLAYER_Y, CAR_W, CAR_H,
                    lane_x(c->lane) - CAR_W / 2, c->y >> 4, CAR_W, CAR_H)) {
            sound_play(&SFX_CRASH);
            return false;
        }
    }

    if (--spawn_cd <= 0) {
        int l = rnd(3);
        spawn(l);
        if (dist > 16 * 2000 && rnd(3) == 0) {   // later: two cars side by side, one lane stays free
            spawn((l + 1 + rnd(2)) % 3);
            spawn_cd = 35;                        // long gap after a double row, so it's always passable
        } else {
            spawn_cd = 14 + rnd(16);
        }
    }

    int s = scroll >> 4;
    gfx_vline(ROAD_L - 1, 0, SCR_H - 1, C_ON);
    gfx_vline(ROAD_R, 0, SCR_H - 1, C_ON);
    for (int k = 1; k < 3; k++)
        for (int y = s % 12 - 12; y < SCR_H; y += 12)
            gfx_vline(ROAD_L + k * LANE_W, y, y + 5, C_ON);
    for (int y = s % 16 - 16; y < SCR_H; y += 16) {
        gfx_rect(ROAD_L - 5, y, 2, 3, C_ON);
        gfx_rect(ROAD_R + 3, y, 2, 3, C_ON);
    }
    for (int y = s % 40 - 40; y < SCR_H; y += 40) {
        gfx_sprite(10, y, TREE, 5, C_ON);
        gfx_sprite(SCR_W - 15, y + 20, TREE, 5, C_ON);
    }
    for (int i = 0; i < MAX_CARS; i++)
        if (cars[i].on) draw_car(lane_x(cars[i].lane) - CAR_W / 2, cars[i].y >> 4);
    draw_car(px - CAR_W / 2, PLAYER_Y);

    char b[16];
    snprintf(b, sizeof b, "%d", dist / 160);
    gfx_rect(0, 0, gfx_text_width(b, 1) + 2, 7, C_OFF);
    gfx_text(1, 1, b, 1, C_ON);
    snprintf(b, sizeof b, "%d", cur_v * 3);       // "km/h"
    gfx_rect(0, SCR_H - 7, gfx_text_width(b, 1) + 2, 7, C_OFF);
    gfx_text(1, SCR_H - 6, b, 1, C_ON);
    return true;
}

static int get_score(void) { return dist / 160; }

const game_t GAME_RACING = { "RACING", reset, step, get_score };
