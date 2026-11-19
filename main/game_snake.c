// SNAKE — with 3 buttons the turns are RELATIVE: LEFT = turn left, RIGHT = turn right
// (from the snake's point of view). Hold FIRE to go faster.
#include <stdio.h>
#include <string.h>
#include "game.h"
#include "display.h"
#include "sound.h"

#define CELL   4
#define COLS   (SCR_W / CELL)          // 32
#define TOP    8                       // field starts under the HUD line
#define ROWS   ((SCR_H - TOP) / CELL)  // 14
#define MAXLEN (COLS * ROWS)

// Ring buffer of body cells: sx/sy[head] is the head, older segments are behind it.
static uint8_t sx[MAXLEN], sy[MAXLEN];
static uint8_t occ[ROWS][COLS];        // occupancy grid → O(1) self-collision test
static int head, len, dir, food_x, food_y, timer, eaten, blink;
static int turnq[2], nq;               // queued turns, so two quick taps between moves aren't lost

static void place_food(void)
{
    for (int t = 0; t < 200; t++) {
        int x = rnd(COLS), y = rnd(ROWS);
        if (!occ[y][x]) { food_x = x; food_y = y; return; }
    }
    for (int y = 0; y < ROWS; y++)
        for (int x = 0; x < COLS; x++)
            if (!occ[y][x]) { food_x = x; food_y = y; return; }
}

static void reset(void)
{
    memset(occ, 0, sizeof occ);
    len = 3; head = 2; dir = 0; nq = 0; timer = 0; eaten = 0;
    for (int i = 0; i < 3; i++) {
        sx[i] = 4 + i; sy[i] = ROWS / 2;
        occ[sy[i]][sx[i]] = 1;
    }
    place_food();
}

static bool step(const input_t *in)
{
    static const int DX[4] = { 1, 0, -1, 0 }, DY[4] = { 0, 1, 0, -1 };   // right, down, left, up

    if (in->lp && nq < 2) turnq[nq++] = 3;   // +3 mod 4 = counter-clockwise = turn left
    if (in->rp && nq < 2) turnq[nq++] = 1;   // +1 mod 4 = clockwise        = turn right

    int period = 6 - eaten / 4;              // frames per move: gets faster as you eat
    if (period < 2) period = 2;
    if (in->f) period = (period + 1) / 2;

    if (++timer >= period) {
        timer = 0;
        if (nq) { dir = (dir + turnq[0]) % 4; turnq[0] = turnq[1]; nq--; }

        int nx = sx[head] + DX[dir], ny = sy[head] + DY[dir];
        if (nx < 0 || nx >= COLS || ny < 0 || ny >= ROWS) { sound_play(&SFX_CRASH); return false; }

        bool grow = nx == food_x && ny == food_y;
        if (!grow) {                          // tail leaves first, so you may move into its cell
            int tail = (head - len + 1 + MAXLEN) % MAXLEN;
            occ[sy[tail]][sx[tail]] = 0;
        }
        if (occ[ny][nx]) { sound_play(&SFX_CRASH); return false; }
        if (grow) { len++; eaten++; sound_play(&SFX_EAT); }

        head = (head + 1) % MAXLEN;
        sx[head] = nx; sy[head] = ny;
        occ[ny][nx] = 1;
        if (grow) {
            if (len >= MAXLEN) return false;  // board full — you win, technically
            place_food();
        }
    }

    gfx_rect(0, TOP - 1, SCR_W, 1, C_ON);
    for (int i = 0; i < len; i++) {
        int k = (head - i + MAXLEN) % MAXLEN;
        int size = i == 0 ? 4 : 3;            // head is a bit bigger
        gfx_rect(sx[k] * CELL, TOP + sy[k] * CELL, size, size, C_ON);
    }
    if ((++blink & 4) == 0) {                 // food blinks
        int cx = food_x * CELL + 1, cy = TOP + food_y * CELL + 1;
        gfx_px(cx, cy - 1, C_ON);
        gfx_rect(cx - 1, cy, 3, 1, C_ON);
        gfx_px(cx, cy + 1, C_ON);
    }

    char b[16];
    snprintf(b, sizeof b, "%d", eaten);
    gfx_text(1, 1, b, 1, C_ON);
    return true;
}

static int get_score(void) { return eaten; }

const game_t GAME_SNAKE = { "SNAKE", reset, step, get_score };
