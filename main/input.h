#pragma once
#include <stdbool.h>

typedef struct {
    bool l, r, f;        // held right now
    bool lp, rp, fp;     // pressed this frame (edge 0 → 1)
    bool fr;             // fire released this frame
    int  f_frames;       // how many frames fire has been held (still valid on the release frame)
} input_t;

void input_init(void);
void input_read(input_t *in);
