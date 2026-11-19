// SSD1306 128x64 over I2C + a tiny graphics library.
//
// The SSD1306 RAM is organised in 8 "pages" of 128 bytes; each byte is a vertical
// strip of 8 pixels (bit0 = top). We keep a local copy (framebuffer), draw into it,
// and stream all 1024 bytes once per frame.

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "driver/i2c.h"     // legacy driver: OK on IDF 4.4 / 5.x (deprecation warning on 5.x)
#include "display.h"
#include "pins.h"

#define I2C_PORT   I2C_NUM_0
#define OLED_ADDR  0x3C

static uint8_t fb[1 + SCR_W * SCR_H / 8];   // fb[0] = 0x40 control byte: "data follows"
#define PIX (fb + 1)

static void oled_cmd(const uint8_t *cmds, size_t n)
{
    uint8_t buf[32];
    buf[0] = 0x00;                           // control byte: "commands follow"
    memcpy(buf + 1, cmds, n);
    i2c_master_write_to_device(I2C_PORT, OLED_ADDR, buf, n + 1, pdMS_TO_TICKS(100));
}

void disp_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = PIN_SDA,
        .scl_io_num = PIN_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,  // I2C is open-drain: lines need pull-ups
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    i2c_param_config(I2C_PORT, &conf);
    i2c_driver_install(I2C_PORT, conf.mode, 0, 0, 0);

    static const uint8_t init[] = {
        0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40,
        0x8D, 0x14,             // charge pump on: OLED needs ~7 V, made internally from 3.3 V
        0x20, 0x00,             // horizontal addressing: one 1024-byte burst fills the screen
        0xA1, 0xC8,             // flip so (0,0) is top-left
        0xDA, 0x12, 0x81, 0xCF, 0xD9, 0xF1, 0xDB, 0x40,
        0xA4, 0xA6, 0xAF        // show RAM, normal colors, display ON
    };
    oled_cmd(init, sizeof init);
}

void disp_flush(void)
{
    static const uint8_t window[] = { 0x21, 0, SCR_W - 1, 0x22, 0, 7 };
    oled_cmd(window, sizeof window);
    fb[0] = 0x40;
    i2c_master_write_to_device(I2C_PORT, OLED_ADDR, fb, sizeof fb, pdMS_TO_TICKS(100));
}

void gfx_clear(void)  { memset(PIX, 0, SCR_W * SCR_H / 8); }
void gfx_invert(void) { for (int i = 0; i < SCR_W * SCR_H / 8; i++) PIX[i] ^= 0xFF; }

void gfx_px(int x, int y, int c)
{
    if ((unsigned)x >= SCR_W || (unsigned)y >= SCR_H) return;   // also rejects negatives
    uint8_t *b = &PIX[x + (y >> 3) * SCR_W];                     // page = y / 8
    uint8_t m = 1 << (y & 7);                                    // bit  = y % 8
    if (c == C_ON) *b |= m;
    else if (c == C_OFF) *b &= ~m;
    else *b ^= m;
}

void gfx_rect(int x, int y, int w, int h, int c)
{
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > SCR_W ? SCR_W : x + w, y1 = y + h > SCR_H ? SCR_H : y + h;
    for (int j = y0; j < y1; j++)
        for (int i = x0; i < x1; i++)
            gfx_px(i, j, c);
}

void gfx_frame(int x, int y, int w, int h, int c)
{
    gfx_rect(x, y, w, 1, c);
    gfx_rect(x, y + h - 1, w, 1, c);
    gfx_rect(x, y + 1, 1, h - 2, c);
    gfx_rect(x + w - 1, y + 1, 1, h - 2, c);
}

void gfx_vline(int x, int y0, int y1, int c)
{
    for (int y = y0; y <= y1; y++) gfx_px(x, y, c);
}

void gfx_sprite(int x, int y, const char *const *rows, int h, int c)
{
    int bg = c == C_ON ? C_OFF : (c == C_OFF ? C_ON : -1);
    for (int j = 0; j < h; j++)
        for (int i = 0; rows[j][i]; i++) {
            if (rows[j][i] == '#') gfx_px(x + i, y + j, c);
            else if (rows[j][i] == 'o' && bg >= 0) gfx_px(x + i, y + j, bg);
        }
}

// ---------------- 3x5 font: 3 columns per glyph, bit0 = top row ----------------
static const uint8_t DIGITS[10][3] = {
    {0x1F,0x11,0x1F},{0x12,0x1F,0x10},{0x1D,0x15,0x17},{0x15,0x15,0x1F},{0x07,0x04,0x1F},
    {0x17,0x15,0x1D},{0x1F,0x15,0x1D},{0x01,0x01,0x1F},{0x1F,0x15,0x1F},{0x17,0x15,0x1F},
};
static const uint8_t ALPHA[26][3] = {
    {0x1E,0x05,0x1E},{0x1F,0x15,0x0A},{0x1F,0x11,0x11},{0x1F,0x11,0x0E},{0x1F,0x15,0x11}, // A-E
    {0x1F,0x05,0x01},{0x1F,0x11,0x1D},{0x1F,0x04,0x1F},{0x11,0x1F,0x11},{0x18,0x10,0x1F}, // F-J
    {0x1F,0x04,0x1B},{0x1F,0x10,0x10},{0x1F,0x06,0x1F},{0x1F,0x01,0x1E},{0x1F,0x11,0x1F}, // K-O
    {0x1F,0x05,0x07},{0x0F,0x09,0x1F},{0x1F,0x05,0x1A},{0x17,0x15,0x1D},{0x01,0x1F,0x01}, // P-T
    {0x1F,0x10,0x1F},{0x0F,0x10,0x0F},{0x1F,0x0C,0x1F},{0x1B,0x04,0x1B},{0x03,0x1C,0x03}, // U-Y
    {0x19,0x15,0x13},                                                                      // Z
};

static const uint8_t *glyph(char c)
{
    static const uint8_t COLON[3] = {0x00,0x0A,0x00}, DASH[3] = {0x04,0x04,0x04};
    static const uint8_t SLASH[3] = {0x18,0x04,0x03}, EXCL[3] = {0x00,0x17,0x00};
    static const uint8_t GT[3]    = {0x11,0x0A,0x04}, LT[3]   = {0x04,0x0A,0x11};
    if (c >= '0' && c <= '9') return DIGITS[c - '0'];
    if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
    if (c >= 'A' && c <= 'Z') return ALPHA[c - 'A'];
    switch (c) {
    case ':': return COLON;
    case '-': return DASH;
    case '/': return SLASH;
    case '!': return EXCL;
    case '>': return GT;
    case '<': return LT;
    default:  return NULL;      // space / unknown → blank
    }
}

int gfx_text(int x, int y, const char *s, int scale, int c)
{
    for (; *s; s++, x += 4 * scale) {
        const uint8_t *g = glyph(*s);
        if (!g) continue;
        for (int cx = 0; cx < 3; cx++)
            for (int cy = 0; cy < 5; cy++)
                if ((g[cx] >> cy) & 1)
                    gfx_rect(x + cx * scale, y + cy * scale, scale, scale, c);
    }
    return x;
}

int gfx_text_width(const char *s, int scale)
{
    int n = (int)strlen(s);
    return n ? n * 4 * scale - scale : 0;
}

void gfx_text_center(int y, const char *s, int scale, int c)
{
    gfx_text((SCR_W - gfx_text_width(s, scale)) / 2, y, s, scale, c);
}
