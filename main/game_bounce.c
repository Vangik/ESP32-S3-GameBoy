// BOUNCE — breakout. LEFT/RIGHT move the paddle, FIRE launches the ball.
// Where the ball hits the paddle sets its angle: edges send it sideways.
#include <stdio.h>
#include "game.h"
#include "display.h"
#include "sound.h"

#define BR_COLS  8
#define BR_ROWS  4
#define BR_W     15
#define BR_H     4
#define BR_TOP   10
#define TOP_WALL 8          // line under the HUD
#define PAD_W    20
#define PAD_Y    60

static bool bricks[BR_ROWS][BR_COLS];
static int bricks_left, pad_x, lives, level, score;
static int bx, by, vx, vy;  // ball position and velocity in 1/16 px (Q4 fixed point)
static bool stuck;          // ball sits on the paddle until FIRE

static int speed(void) { return 20 + level * 3; }   // 16 = 1 px/frame

static void new_level(void)
{
    for (int r = 0; r < BR_ROWS; r++)
        for (int c = 0; c < BR_COLS; c++) bricks[r][c] = true;
    bricks_left = BR_ROWS * BR_COLS;
    stuck = true;
}

static void reset(void)
{
    score = 0; lives = 3; level = 1;
    pad_x = (SCR_W - PAD_W) / 2;
    new_level();
}

static bool hit_brick(void)
{
    int x = bx >> 4, y = by >> 4;
    for (int r = 0; r < BR_ROWS; r++)
        for (int c = 0; c < BR_COLS; c++)
            if (bricks[r][c] && overlap(x, y, 2, 2, 1 + c * 16, BR_TOP + r * 6, BR_W, BR_H)) {
                bricks[r][c] = false;
                bricks_left--;
                score += BR_ROWS - r;                  // higher rows are worth more
                sound_play(&SFX_BRICK);
                return true;
            }
    return false;
}

static bool step(const input_t *in)
{
    if (in->l) pad_x -= 3;
    if (in->r) pad_x += 3;
    if (pad_x < 0) pad_x = 0;
    if (pad_x > SCR_W - PAD_W) pad_x = SCR_W - PAD_W;

    if (stuck) {
        bx = (pad_x + PAD_W / 2 - 1) << 4;
        by = (PAD_Y - 2) << 4;
        if (in->fp) {
            stuck = false;
            vx = rnd(2) ? 10 : -10;
            vy = -speed();
            sound_play(&SFX_BOUNCE);
        }
    } else {
        // Move X and Y separately: whichever axis caused the brick hit is the one to reflect.
        bx += vx;
        if (bx < 0)                { bx = 0;                vx = -vx; sound_play(&SFX_BOUNCE); }
        if (bx > (SCR_W - 2) << 4) { bx = (SCR_W - 2) << 4; vx = -vx; sound_play(&SFX_BOUNCE); }
        if (hit_brick()) { bx -= vx; vx = -vx; }

        by += vy;
        if (by < (TOP_WALL + 1) << 4) { by = (TOP_WALL + 1) << 4; vy = -vy; sound_play(&SFX_BOUNCE); }
        if (hit_brick()) { by -= vy; vy = -vy; }

        int x = bx >> 4, y = by >> 4;
        if (vy > 0 && overlap(x, y, 2, 2, pad_x, PAD_Y, PAD_W, 2)) {
            int off = (x + 1) - (pad_x + PAD_W / 2);   // -10..+10: where on the paddle we hit
            vx = off * 3;
            vy = -speed();
            sound_play(&SFX_BOUNCE);
        }
        if (y >= SCR_H) {
            if (--lives <= 0) return false;
            stuck = true;
            sound_play(&SFX_HURT);
        }
        if (bricks_left == 0) { level++; new_level(); sound_play(&SFX_LEVEL); }
    }

    gfx_rect(0, TOP_WALL, SCR_W, 1, C_ON);
    for (int r = 0; r < BR_ROWS; r++)
        for (int c = 0; c < BR_COLS; c++)
            if (bricks[r][c]) {
                if (r % 2 == 0) gfx_rect (1 + c * 16, BR_TOP + r * 6, BR_W, BR_H, C_ON);
                else            gfx_frame(1 + c * 16, BR_TOP + r * 6, BR_W, BR_H, C_ON);
            }
    gfx_rect(pad_x, PAD_Y, PAD_W, 2, C_ON);
    gfx_rect(bx >> 4, by >> 4, 2, 2, C_ON);

    char b[16];
    snprintf(b, sizeof b, "%d", score);
    gfx_text(1, 1, b, 1, C_ON);
    snprintf(b, sizeof b, "L%d", level);
    gfx_text_center(1, b, 1, C_ON);
    for (int i = 0; i < lives; i++) gfx_rect(SCR_W - 4 - i * 5, 2, 3, 3, C_ON);
    return true;
}

static int get_score(void) { return score; }

const game_t GAME_BOUNCE = { "BOUNCE", reset, step, get_score };
