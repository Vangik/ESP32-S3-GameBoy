#pragma once
#include <stdint.h>
#include <stdbool.h>

#define SCR_W 128
#define SCR_H 64

enum { C_OFF = 0, C_ON = 1, C_INV = 2 };   // pixel "colors": clear, set, flip

void disp_init(void);
void disp_flush(void);                      // send framebuffer to the OLED (~23 ms at 400 kHz)

void gfx_clear(void);
void gfx_invert(void);                      // flip every pixel (used for "damage flash")
void gfx_px(int x, int y, int c);
void gfx_rect(int x, int y, int w, int h, int c);    // filled
void gfx_frame(int x, int y, int w, int h, int c);   // outline
void gfx_vline(int x, int y0, int y1, int c);        // inclusive

// Sprites are arrays of strings: '#' = pixel in color c, 'o' = opposite color (outline), '.' = transparent
void gfx_sprite(int x, int y, const char *const *rows, int h, int c);

int  gfx_text(int x, int y, const char *s, int scale, int c);   // 3x5 font, returns end x
int  gfx_text_width(const char *s, int scale);
void gfx_text_center(int y, const char *s, int scale, int c);
