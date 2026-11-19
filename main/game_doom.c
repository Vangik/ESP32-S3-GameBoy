// DOOM — a Wolfenstein-style raycaster on a 1-bit 128x64 screen.
// LEFT/RIGHT turn, HOLD FIRE = walk forward, TAP FIRE = shoot.
//
// How it works: for each of the 128 screen columns we cast a ray through a 2D grid map
// (DDA algorithm — step cell by cell until we hit a wall). Wall height = 1 / distance.
// With only on/off pixels, "brightness" is faked with dithering patterns: near = solid,
// far = sparse dots. Enemies are flat sprites, scaled by distance and clipped by a z-buffer.

#include <math.h>
#include <stdio.h>
#include "game.h"
#include "display.h"
#include "sound.h"

#define MAP_W   16
#define MAP_H   16
#define PLANE   0.66f                         // camera plane half-width → ~66° field of view
#define PROJ    (SCR_W / (2.0f * PLANE))      // ≈ 97 px per world unit at distance 1
#define MAX_EN  6
#define TAP     4                             // press ≤ 4 frames (160 ms) = tap = shoot

static const char *const MAP[MAP_H] = {
    "################",
    "#......#.......#",
    "#.##...#..##...#",
    "#.#....#...#...#",
    "#...........#..#",
    "#..###...#.....#",
    "#....#...#..####",
    "###..#.........#",
    "#.........###..#",
    "#..#...........#",
    "#..#..##...#...#",
    "#..#.......#...#",
    "#.....#....###.#",
    "#.##..#........#",
    "#......#.......#",
    "################",
};

#define IMP_W 12
#define IMP_H 14
static const char *const IMP[IMP_H] = {
    "...oooooo...",
    "..o######o..",
    ".o#oo##oo#o.",
    ".o########o.",
    ".o##oooo##o.",
    "..o######o..",
    "oo##o##o##oo",
    "o####oo####o",
    "o#o######o#o",
    "o#o######o#o",
    "oo.o####o.oo",
    "...o#oo#o...",
    "..o##oo##o..",
    "..ooo..ooo..",
};
static const char *const GUN[8] = {
    ".......oo.......",
    "......o##o......",
    "......o##o......",
    "......o##o......",
    ".....oo##oo.....",
    "....o######o....",
    "...o##o##o##o...",
    "..o##########o..",
};
static const char *const FLASH[5] = { "#..#..#", ".#.#.#.", "..###..", ".#####.", "..###.." };

static const int SPAWN[][2] = { {13,1}, {14,4}, {8,7}, {13,9}, {2,12}, {12,14}, {8,11}, {5,3} };
#define N_SPAWN ((int)(sizeof SPAWN / sizeof SPAWN[0]))

typedef struct { bool on, awake; float x, y; int hp, hurt, cool; } enemy_t;
static enemy_t en[MAX_EN];
static float plx, ply, ang;
static int hp, kills, wave, gun_cd, flash, hurt_flash, bob;
static float zbuf[SCR_W];                     // wall distance per column, for sprite clipping

static bool is_wall(float x, float y)
{
    int ix = (int)x, iy = (int)y;
    if (ix < 0 || iy < 0 || ix >= MAP_W || iy >= MAP_H) return true;
    return MAP[iy][ix] == '#';
}

// Move with a small radius and test X and Y separately → you slide along walls instead of sticking.
static void try_move(float *x, float *y, float dx, float dy)
{
    const float R = 0.2f;
    float nx = *x + dx;
    if (!is_wall(nx + (dx > 0 ? R : -R), *y)) *x = nx;
    float ny = *y + dy;
    if (!is_wall(*x, ny + (dy > 0 ? R : -R))) *y = ny;
}

static void spawn_wave(void)
{
    int n = 2 + wave, k = 0;
    if (n > MAX_EN) n = MAX_EN;
    for (int t = 0; t < 50 && k < n; t++) {
        const int *s = SPAWN[rnd(N_SPAWN)];
        float x = s[0] + 0.5f, y = s[1] + 0.5f;
        if (hypotf(x - plx, y - ply) < 4.0f) continue;           // not right next to the player
        bool dup = false;
        for (int j = 0; j < k; j++) if (en[j].x == x && en[j].y == y) dup = true;
        if (dup) continue;
        en[k++] = (enemy_t){ .on = true, .x = x, .y = y, .hp = 2 };
    }
    for (; k < MAX_EN; k++) en[k].on = false;
}

static void reset(void)
{
    plx = 1.5f; ply = 1.5f; ang = 0;
    hp = 100; kills = 0; wave = 1;
    gun_cd = flash = hurt_flash = bob = 0;
    spawn_wave();
}

// Ordered dithering: 5 "grey" levels out of black & white pixels.
static bool dither(int x, int y, int lvl)
{
    switch (lvl) {
    case 4:  return true;                              // 100%
    case 3:  return !((x & 1) && (y & 1));             //  75%
    case 2:  return ((x ^ y) & 1) == 0;                //  50% checkerboard
    case 1:  return !(x & 1) && !(y & 1);              //  25%
    default: return !(x & 3) && !(y & 3);              //  ~6%
    }
}

static void render_walls(float dx, float dy, float cx, float cy)
{
    for (int x = 0; x < SCR_W; x++) {
        float cam = 2.0f * x / SCR_W - 1.0f;           // -1 (left edge) .. +1 (right edge)
        float rx = dx + cx * cam, ry = dy + cy * cam;  // ray direction for this column
        int mx = (int)plx, my = (int)ply;
        float ddx = rx == 0 ? 1e30f : fabsf(1.0f / rx);   // ray length to cross one cell in X
        float ddy = ry == 0 ? 1e30f : fabsf(1.0f / ry);
        int stx, sty;
        float sdx, sdy;
        if (rx < 0) { stx = -1; sdx = (plx - mx) * ddx; } else { stx = 1; sdx = (mx + 1.0f - plx) * ddx; }
        if (ry < 0) { sty = -1; sdy = (ply - my) * ddy; } else { sty = 1; sdy = (my + 1.0f - ply) * ddy; }

        int side = 0;
        for (int i = 0; i < 64; i++) {                 // DDA: always step to the nearer grid line
            if (sdx < sdy) { sdx += ddx; mx += stx; side = 0; }
            else           { sdy += ddy; my += sty; side = 1; }
            if (MAP[my][mx] == '#') break;             // map border is solid, so this always ends
        }
        // perpendicular distance (not Euclidean) → no fish-eye distortion
        float dist = side == 0 ? sdx - ddx : sdy - ddy;
        if (dist < 0.05f) dist = 0.05f;
        zbuf[x] = dist;

        float wx = side == 0 ? ply + dist * ry : plx + dist * rx;   // where along the wall we hit
        wx -= floorf(wx);
        bool edge = wx < 0.05f || wx > 0.95f;          // vertical lines between wall blocks

        int h = (int)(PROJ / dist);
        int top = SCR_H / 2 - h / 2, bot = top + h;
        int lvl = 4 - (int)(dist / 1.5f) - side;       // farther = darker; Y-facing walls darker
        if (lvl < 0) lvl = 0;

        int y0 = top < 0 ? 0 : top, y1 = bot > SCR_H ? SCR_H : bot;
        for (int y = y0; y < y1; y++)
            if (edge || y == top || y == bot - 1 || dither(x, y, lvl)) gfx_px(x, y, C_ON);
    }
}

// World → camera space. Returns false if behind the player.
static bool project(const enemy_t *e, float dx, float dy, float cx, float cy, float *sx, float *depth)
{
    float rx = e->x - plx, ry = e->y - ply;
    float inv = 1.0f / (cx * dy - dx * cy);            // inverse of the camera matrix determinant
    float tx = inv * (dy * rx - dx * ry);              // sideways offset
    float tz = inv * (-cy * rx + cx * ry);             // depth
    if (tz < 0.2f) return false;
    *sx = SCR_W / 2.0f * (1.0f + tx / tz);
    *depth = tz;
    return true;
}

static int imp_width(float depth) { return (int)(PROJ / depth) * 3 / 4 * IMP_W / IMP_H; }

static void draw_imp(float sx, float depth, bool blink)
{
    int sh = (int)(PROJ / depth);          // height of a full wall at this depth
    int bot = SCR_H / 2 + sh / 2;          // floor line
    int ih = sh * 3 / 4;                   // imp is 3/4 of a wall tall
    int iw = ih * IMP_W / IMP_H;
    if (ih < 2 || iw < 1) return;
    int left = (int)sx - iw / 2, top = bot - ih;
    int x0 = left < 0 ? 0 : left, x1 = left + iw > SCR_W ? SCR_W : left + iw;
    int y0 = top < 0 ? 0 : top,   y1 = bot > SCR_H ? SCR_H : bot;
    for (int x = x0; x < x1; x++) {
        if (depth >= zbuf[x]) continue;    // a wall is closer in this column → hidden
        int u = (x - left) * IMP_W / iw;
        for (int y = y0; y < y1; y++) {
            char c = IMP[(y - top) * IMP_H / ih][u];
            if (c == '#') gfx_px(x, y, blink ? C_OFF : C_ON);
            else if (c == 'o') gfx_px(x, y, blink ? C_ON : C_OFF);
        }
    }
}

static bool step(const input_t *in)
{
    if (in->l) ang -= 0.09f;
    if (in->r) ang += 0.09f;
    float dx = cosf(ang), dy = sinf(ang);
    float cx = -dy * PLANE, cy = dx * PLANE;          // camera plane ⟂ view direction

    if (in->f && in->f_frames > TAP) { try_move(&plx, &ply, dx * 0.07f, dy * 0.07f); bob++; }
    if (gun_cd) gun_cd--;
    if (flash) flash--;

    render_walls(dx, dy, cx, cy);

    // enemy AI: wake up when close, walk towards the player, bite when adjacent
    bool any = false;
    for (int i = 0; i < MAX_EN; i++) {
        enemy_t *e = &en[i];
        if (!e->on) continue;
        any = true;
        float ex = plx - e->x, ey = ply - e->y;
        float d = sqrtf(ex * ex + ey * ey);
        if (d < 6.0f) e->awake = true;
        if (e->hurt) e->hurt--;
        if (e->cool) e->cool--;
        if (e->awake && d > 0.7f) try_move(&e->x, &e->y, ex / d * 0.035f, ey / d * 0.035f);
        if (d < 0.9f && !e->cool) {
            hp -= 7 + rnd(6);
            e->cool = 20;
            hurt_flash = 2;
            sound_play(&SFX_HURT);
        }
    }
    if (!any) { wave++; spawn_wave(); sound_play(&SFX_LEVEL); }

    // shooting = hitscan: nearest enemy covering the screen centre and closer than the wall there
    if (in->fr && in->f_frames <= TAP && gun_cd == 0) {
        gun_cd = 6; flash = 2;
        sound_play(&SFX_GUN);
        enemy_t *best = NULL;
        float best_d = zbuf[SCR_W / 2];
        for (int i = 0; i < MAX_EN; i++) {
            float sx, depth;
            if (!en[i].on || !project(&en[i], dx, dy, cx, cy, &sx, &depth)) continue;
            if (fabsf(sx - SCR_W / 2) < imp_width(depth) / 2 + 1 && depth < best_d) {
                best = &en[i];
                best_d = depth;
            }
        }
        if (best) {
            best->awake = true;
            best->hurt = 6;
            if (--best->hp <= 0) { best->on = false; kills++; sound_play(&SFX_BOOM); }
        }
    }

    // sprites: painter's algorithm — sort far → near, draw in that order
    struct { int i; float sx, depth; } vis[MAX_EN], tmp;
    int n = 0;
    for (int i = 0; i < MAX_EN; i++)
        if (en[i].on && project(&en[i], dx, dy, cx, cy, &vis[n].sx, &vis[n].depth)) vis[n++].i = i;
    for (int a = 1; a < n; a++)
        for (int b = a; b > 0 && vis[b].depth > vis[b - 1].depth; b--) {
            tmp = vis[b]; vis[b] = vis[b - 1]; vis[b - 1] = tmp;
        }
    for (int k = 0; k < n; k++)
        draw_imp(vis[k].sx, vis[k].depth, en[vis[k].i].hurt & 1);

    // gun + crosshair + HUD
    int gy = SCR_H - 8 + ((bob >> 2) & 1);            // weapon bob while walking
    if (flash) gfx_sprite(SCR_W / 2 - 3, gy - 5, FLASH, 5, C_ON);
    gfx_sprite(SCR_W / 2 - 8, gy, GUN, 8, C_ON);
    gfx_px(SCR_W / 2 - 2, SCR_H / 2, C_INV); gfx_px(SCR_W / 2 + 2, SCR_H / 2, C_INV);
    gfx_px(SCR_W / 2, SCR_H / 2 - 2, C_INV); gfx_px(SCR_W / 2, SCR_H / 2 + 2, C_INV);

    char b[16];
    snprintf(b, sizeof b, "HP %d", hp < 0 ? 0 : hp);
    gfx_rect(0, 0, gfx_text_width(b, 1) + 2, 7, C_OFF);
    gfx_text(1, 1, b, 1, C_ON);
    snprintf(b, sizeof b, "K %d", kills);
    int w = gfx_text_width(b, 1);
    gfx_rect(SCR_W - w - 2, 0, w + 2, 7, C_OFF);
    gfx_text(SCR_W - w - 1, 1, b, 1, C_ON);

    if (hurt_flash) { hurt_flash--; gfx_invert(); }   // screen flashes when you get hit
    return hp > 0;
}

static int get_score(void) { return kills; }

const game_t GAME_DOOM = { "DOOM", reset, step, get_score };
