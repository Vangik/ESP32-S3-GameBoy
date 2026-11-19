// GAME BOY — ESP32-S3 + SSD1306 + 3 buttons + buzzer.
//
// MENU:      LEFT/RIGHT = up/down, FIRE = play
// IN GAME:   hold LEFT+RIGHT ~1 s = back to menu
// GAME OVER: FIRE = play again, LEFT or RIGHT = menu
//
// Fixed-timestep loop: every frame is exactly 40 ms (25 fps) thanks to vTaskDelayUntil,
// so game speed doesn't depend on how long drawing took.

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "display.h"
#include "input.h"
#include "sound.h"
#include "hiscore.h"
#include "game.h"

static const game_t *const GAMES[] = {
    &GAME_ROCKS, &GAME_BOUNCE, &GAME_SNAKE, &GAME_RACING, &GAME_SHOOTER, &GAME_DOOM,
};
#define NUM_GAMES  ((int)(sizeof GAMES / sizeof GAMES[0]))
#define FRAME_MS   40
#define EXIT_HOLD  25          // frames of LEFT+RIGHT to quit (~1 s)
#define VISIBLE    6           // menu rows on screen

static int32_t hi[NUM_GAMES];

static void draw_menu(int sel)
{
    gfx_text_center(1, "GAME BOY", 2, C_ON);
    int first = sel >= VISIBLE ? sel - VISIBLE + 1 : 0;   // scroll if more games than rows
    for (int k = 0; k < VISIBLE && first + k < NUM_GAMES; k++) {
        int i = first + k, y = 13 + k * 8;
        int c = C_ON;
        if (i == sel) { gfx_rect(0, y, SCR_W, 8, C_ON); c = C_OFF; }   // inverted bar = cursor
        gfx_text(4, y + 1, GAMES[i]->name, 1, c);
        char b[16];
        snprintf(b, sizeof b, "%ld", (long)hi[i]);
        gfx_text(SCR_W - 3 - gfx_text_width(b, 1), y + 1, b, 1, c);
    }
}

static void draw_over(int g, int score, bool record, bool ready, int frame)
{
    char b[24];
    gfx_text_center(2, "GAME OVER", 2, C_ON);
    gfx_text_center(16, GAMES[g]->name, 1, C_ON);
    snprintf(b, sizeof b, "SCORE %d", score);
    gfx_text_center(26, b, 1, C_ON);
    if (record) {
        if (frame & 8) gfx_text_center(35, "NEW RECORD!", 1, C_ON);   // blinks
    } else {
        snprintf(b, sizeof b, "HI %ld", (long)hi[g]);
        gfx_text_center(35, b, 1, C_ON);
    }
    if (ready) {
        gfx_text_center(48, "FIRE - AGAIN", 1, C_ON);
        gfx_text_center(56, "L/R - MENU", 1, C_ON);
    }
}

static void draw_exit_bar(int hold)
{
    gfx_rect(24, 24, 80, 16, C_OFF);
    gfx_frame(24, 24, 80, 16, C_ON);
    gfx_text_center(27, "MENU", 1, C_ON);
    gfx_rect(27, 35, 74 * hold / EXIT_HOLD, 2, C_ON);
}

void app_main(void)
{
    input_init();
    sound_init();
    disp_init();
    hiscore_init();
    for (int i = 0; i < NUM_GAMES; i++) hi[i] = hiscore_load(GAMES[i]->name);
    printf("GAME BOY ready: %d games\n", NUM_GAMES);

    enum { ST_MENU, ST_PLAY, ST_OVER } st = ST_MENU;
    int sel = 0, exit_hold = 0, over_timer = 0, last_score = 0, frame = 0;
    bool record = false;
    input_t in;
    TickType_t last_wake = xTaskGetTickCount();

    while (true) {
        input_read(&in);
        gfx_clear();
        frame++;

        switch (st) {
        case ST_MENU:
            if (in.lp) { sel = (sel + NUM_GAMES - 1) % NUM_GAMES; sound_play(&SFX_BLIP); }
            if (in.rp) { sel = (sel + 1) % NUM_GAMES;             sound_play(&SFX_BLIP); }
            if (in.fp) {
                GAMES[sel]->reset();
                st = ST_PLAY;
                exit_hold = 0;
                sound_play(&SFX_START);
            }
            draw_menu(sel);
            break;

        case ST_PLAY: {
            bool alive = GAMES[sel]->step(&in);
            if (in.l && in.r) {
                if (++exit_hold >= EXIT_HOLD) { st = ST_MENU; break; }
                if (exit_hold > 5) draw_exit_bar(exit_hold);
            } else {
                exit_hold = 0;
            }
            if (!alive) {
                last_score = GAMES[sel]->score();
                record = last_score > hi[sel];
                if (record) {
                    hi[sel] = last_score;
                    hiscore_save(GAMES[sel]->name, last_score);
                }
                st = ST_OVER;
                over_timer = 25;      // 1 s pause so a held button doesn't skip the screen
                sound_play(&SFX_OVER);
            }
            break;
        }

        case ST_OVER:
            if (over_timer > 0) over_timer--;
            draw_over(sel, last_score, record, over_timer == 0, frame);
            if (over_timer == 0) {
                if (in.fp)              { GAMES[sel]->reset(); st = ST_PLAY; exit_hold = 0; sound_play(&SFX_START); }
                else if (in.lp || in.rp) { st = ST_MENU; sound_play(&SFX_BLIP); }
            }
            break;
        }

        disp_flush();
        sound_tick();
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(FRAME_MS));   // sleep until the next 40 ms tick
    }
}
