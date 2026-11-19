// A passive buzzer has no oscillator: it clicks on every edge. A 50% square wave
// at F Hz = a tone of F Hz. The LEDC (PWM) peripheral makes that wave in hardware,
// so the game loop never waits for sound; sound_tick() just steps through notes.

#include <stddef.h>
#include "driver/ledc.h"
#include "sound.h"
#include "pins.h"

#define DEF_SFX(name, prio, ...)                                         \
    static const note_t name##_notes[] = { __VA_ARGS__ };                 \
    const sfx_t name = { name##_notes, sizeof(name##_notes) / sizeof(note_t), prio }

DEF_SFX(SFX_BLIP,   1, {1200, 1});
DEF_SFX(SFX_SHOT,   1, {1800, 1}, {1400, 1});
DEF_SFX(SFX_BOUNCE, 1, {660, 1});
DEF_SFX(SFX_GUN,    2, {220, 1}, {110, 2});
DEF_SFX(SFX_BRICK,  2, {990, 1}, {1320, 1});
DEF_SFX(SFX_EAT,    2, {1000, 1}, {1500, 1});
DEF_SFX(SFX_BOOM,   2, {300, 1}, {180, 2});
DEF_SFX(SFX_HURT,   3, {200, 2}, {140, 3});
DEF_SFX(SFX_CRASH,  3, {160, 3}, {110, 3}, {80, 4});
DEF_SFX(SFX_LEVEL,  4, {523, 2}, {659, 2}, {784, 2}, {1047, 2}, {1319, 4});
DEF_SFX(SFX_START,  4, {523, 2}, {659, 2}, {784, 2}, {1047, 4});
DEF_SFX(SFX_OVER,   4, {784, 4}, {659, 4}, {523, 4}, {392, 10});

static const sfx_t *cur;
static int idx, left;

static void tone(uint32_t freq)
{
    if (freq) {
        ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, freq);    // pitch
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 512);   // 512/1024 = 50% square wave
    } else {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);     // pin stays LOW = silence
    }
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);         // duty applies only after this
}

void sound_init(void)
{
    ledc_timer_config_t t = {
        .speed_mode = LEDC_LOW_SPEED_MODE,     // ESP32-S3 has only low-speed LEDC
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .freq_hz = 1000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&t);
    ledc_channel_config_t ch = {
        .gpio_num = PIN_BUZZER,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
    };
    ledc_channel_config(&ch);
}

void sound_play(const sfx_t *s)
{
    if (cur && s->prio < cur->prio) return;    // a "pew" must not cut the game-over melody
    cur = s;
    idx = 0;
    left = s->notes[0].frames;
    tone(s->notes[0].freq);
}

void sound_tick(void)
{
    if (!cur || --left > 0) return;
    if (++idx >= cur->len) { cur = NULL; tone(0); return; }
    left = cur->notes[idx].frames;
    tone(cur->notes[idx].freq);
}
