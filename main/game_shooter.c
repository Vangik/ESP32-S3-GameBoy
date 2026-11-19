// SHOOTER — space-invaders style. LEFT/RIGHT move, FIRE shoots (one bullet on screen at a time).
// The formation speeds up as it shrinks — exactly like the 1978 original.
#include <stdio.h>
#include <string.h>
#include "game.h"
#include "display.h"
#include "sound.h"

#define AL_COLS 7
#define AL_ROWS 3
#define AL_DX   12
#define AL_DY   8
#define PL_Y    59
#define PL_W    9
#define MAX_EB  3

static const char *const ALIEN[2][5] = {
    { "..#.#..", ".#####.", "##.#.##", "#######", "#.#.#.#" },
    { "..#.#..", ".#####.", "##.#.##", "#######", ".#...#." },
};
static const char *const SHIP[4] = { "....#....", "...###...", ".#######.", "#########" };

static bool alive[AL_ROWS][AL_COLS];
static int fx, fy, fdir, move_cd, anim, n_alive;     // formation position / direction
static int pl_x, lives, invuln, score, wave;
static bool shot_on;
static int shot_x, shot_y;
static struct { bool on; int x, y; } eb[MAX_EB];      // enemy bullets

static void new_wave(void)
{
    for (int r = 0; r < AL_ROWS; r++)
        for (int c = 0; c < AL_COLS; c++) alive[r][c] = true;
    n_alive = AL_ROWS * AL_COLS;
    fx = 8;
    fy = 8 + (wave > 5 ? 4 : wave - 1) * 3;           // later waves start lower
    fdir = 1; move_cd = 0;
    shot_on = false;
    memset(eb, 0, sizeof eb);
}

static void reset(void)
{
    score = 0; lives = 3; wave = 1; invuln = 0;
    pl_x = (SCR_W - PL_W) / 2;
    new_wave();
}

static bool step(const input_t *in)
{
    if (in->l && pl_x > 0)            pl_x -= 2;
    if (in->r && pl_x < SCR_W - PL_W) pl_x += 2;

    if (in->fp && !shot_on) {
        shot_on = true; shot_x = pl_x + PL_W / 2; shot_y = PL_Y - 3;
        sound_play(&SFX_SHOT);
    }
    if (shot_on) {
        shot_y -= 4;
        if (shot_y < 0) shot_on = false;
        for (int r = 0; r < AL_ROWS && shot_on; r++)
            for (int c = 0; c < AL_COLS && shot_on; c++)
                if (alive[r][c] && overlap(shot_x, shot_y, 1, 3, fx + c * AL_DX, fy + r * AL_DY, 7, 5)) {
                    alive[r][c] = false;
                    n_alive--;
                    shot_on = false;
                    score += (AL_ROWS - r) * 10;
                    sound_play(&SFX_BOOM);
                }
    }
    if (n_alive == 0) { wave++; new_wave(); sound_play(&SFX_LEVEL); }

    // fewer aliens → shorter move period → faster march
    int period = 2 + n_alive / 3 - (wave > 3 ? 2 : wave - 1);
    if (period < 1) period = 1;
    if (++move_cd >= period) {
        move_cd = 0;
        anim ^= 1;
        int minc = AL_COLS, maxc = -1, maxr = -1;
        for (int r = 0; r < AL_ROWS; r++)
            for (int c = 0; c < AL_COLS; c++)
                if (alive[r][c]) {
                    if (c < minc) minc = c;
                    if (c > maxc) maxc = c;
                    if (r > maxr) maxr = r;
                }
        int left = fx + minc * AL_DX, right = fx + maxc * AL_DX + 7;
        if ((fdir > 0 && right + 2 > SCR_W) || (fdir < 0 && left - 2 < 0)) { fy += 3; fdir = -fdir; }
        else fx += 2 * fdir;
        if (fy + maxr * AL_DY + 5 >= PL_Y) return false;    // they landed
    }

    // the lowest alien in a random column shoots
    if (rnd(100) < 2 + wave) {
        int c = rnd(AL_COLS);
        for (int r = AL_ROWS - 1; r >= 0; r--)
            if (alive[r][c]) {
                for (int i = 0; i < MAX_EB; i++)
                    if (!eb[i].on) {
                        eb[i].on = true; eb[i].x = fx + c * AL_DX + 3; eb[i].y = fy + r * AL_DY + 5;
                        break;
                    }
                break;
            }
    }
    for (int i = 0; i < MAX_EB; i++) {
        if (!eb[i].on) continue;
        eb[i].y += 2;
        if (eb[i].y > SCR_H) eb[i].on = false;
        else if (!invuln && overlap(eb[i].x, eb[i].y, 1, 3, pl_x, PL_Y, PL_W, 4)) {
            eb[i].on = false;
            invuln = 40;                                     // ~1.6 s of blinking immunity
            sound_play(&SFX_HURT);
            if (--lives <= 0) return false;
        }
    }
    if (invuln) invuln--;

    for (int r = 0; r < AL_ROWS; r++)
        for (int c = 0; c < AL_COLS; c++)
            if (alive[r][c]) gfx_sprite(fx + c * AL_DX, fy + r * AL_DY, ALIEN[anim], 5, C_ON);
    if (!invuln || (invuln / 3) % 2) gfx_sprite(pl_x, PL_Y, SHIP, 4, C_ON);
    if (shot_on) gfx_rect(shot_x, shot_y, 1, 3, C_ON);
    for (int i = 0; i < MAX_EB; i++)
        if (eb[i].on) { gfx_px(eb[i].x, eb[i].y, C_ON); gfx_px(eb[i].x + 1, eb[i].y + 1, C_ON); gfx_px(eb[i].x, eb[i].y + 2, C_ON); }
    gfx_rect(0, SCR_H - 1, SCR_W, 1, C_ON);

    char b[16];
    snprintf(b, sizeof b, "%d", score);
    gfx_text(1, 1, b, 1, C_ON);
    for (int i = 0; i < lives; i++) gfx_rect(SCR_W - 5 - i * 6, 2, 4, 3, C_ON);
    return true;
}

static int get_score(void) { return score; }

const game_t GAME_SHOOTER = { "SHOOTER", reset, step, get_score };
