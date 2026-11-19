// Buttons are active-low: internal pull-up keeps the pin at 3.3 V (reads 1),
// pressing shorts it to GND (reads 0). Without the pull-up the input would "float"
// and read random noise.
//
// We poll once per frame (40 ms). Mechanical contacts bounce for only a few ms,
// so slow polling doubles as a free debounce.

#include "driver/gpio.h"
#include "input.h"
#include "pins.h"

static bool prev_l, prev_r, prev_f;
static int  f_count;

void input_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << BTN_LEFT) | (1ULL << BTN_FIRE) | (1ULL << BTN_RIGHT),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io);
}


void input_read(input_t *in)
{
    bool l = gpio_get_level(BTN_LEFT)  == 0;
    bool r = gpio_get_level(BTN_RIGHT) == 0;
    bool f = gpio_get_level(BTN_FIRE)  == 0;

    in->l = l; in->r = r; in->f = f;
    in->lp = l && !prev_l;
    in->rp = r && !prev_r;
    in->fp = f && !prev_f;
    in->fr = !f && prev_f;

    if (f) f_count++;
    in->f_frames = f_count;      // on the release frame this is the length of the press
    if (!f) f_count = 0;

    prev_l = l; prev_r = r; prev_f = f;
}
