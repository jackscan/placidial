/*
 * Copyright(c) 2016 Mathias Fiedler
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files(the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions :
 *
 *     The above copyright notice and this permission notice shall be included
 *     in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef RASTERIZER_H
#define RASTERIZER_H

#include <stdbool.h>
#include <stdint.h>

#define FIXED_SHIFT 4

#define DIGIT_HEIGHT 13
#define DIGIT_WIDTH 12

#define SMALL_DIGIT_WIDTH 6
#define SMALL_DIGIT_HEIGHT 9

#define DISCONNECT_ICON_WIDTH 16
#define DISCONNECT_ICON_HEIGHT 23

#define BATTERY_ICON_WIDTH 26
#define BATTERY_ICON_HEIGHT 12

struct GBitmap;

struct scanline
{
    int start;
    int end;
};

struct bmpset
{
    struct GBitmap *bmp;
    int w, h;
};

void draw_2bit_bmp(struct GBitmap *bmp, struct bmpset *set, int n,
                   int x, int y, uint32_t colors);
void draw_2bit_bmp_aligned(struct GBitmap *bmp, struct bmpset *set, int n,
                           int x, int y, uint32_t colors);
void draw_digit(struct GBitmap *bmp, uint8_t color, int x, int y, int n);
void draw_small_digit(struct GBitmap *bmp, uint8_t color, int x, int y, int n);
void draw_box(struct GBitmap *bmp, uint8_t color, int x, int y, int w, int h);
void draw_rect(struct GBitmap *bmp, struct scanline *scanlines,
               uint8_t color, int32_t px, int32_t py,
               int32_t dx, int32_t dy, int32_t len, int32_t w,
               bool outline, bool dark_bg);
void draw_bg_rect(struct GBitmap *bmp, struct scanline *scanlines,
                  uint32_t colors, int32_t px, int32_t py,
                  int32_t dx, int32_t dy, int32_t len, int32_t w);
void draw_circle(struct GBitmap *bmp, uint8_t color, int32_t cx, int32_t cy,
                 int32_t r, bool outline, bool dark_bg);

void draw_disconnected(struct GBitmap *bmp, struct scanline *scanlines,
                       uint8_t color, int cx, int cy);
void draw_battery(struct GBitmap *bmp, struct scanline *scanlines,
                  uint8_t color, int cx, int cy, uint8_t level);

int32_t sqrti(int32_t i);

static inline int32_t fixed(int i)
{
   return (int32_t)i << FIXED_SHIFT;
}

static inline bool dark_color(uint8_t color)
{
    return (color & 0x2A) == 0;
}

static inline uint8_t blend_inv(uint32_t x, uint32_t y, int a, int d)
{
    uint32_t b = ((0x33 - (x & 0x33)) * (d - a) + (0x33 - (y & 0x33)) * a);
    uint32_t c = ((0xCC - (x & 0xCC)) * (d - a) + (0xCC - (y & 0xCC)) * a);
    return (uint8_t)((0xFF - (((b & 0xCC) + (c & 0x330)) >> 2)) | 0xC0);
}

static inline uint8_t blend(uint32_t x, uint32_t y, int a, int d)
{
    uint32_t b = ((x & 0x33) * (d - a) + (y & 0x33) * a);
    uint32_t c = ((x & 0xCC) * (d - a) + ((y | 0xC0) & 0xCC) * a);
    return (uint8_t)(((b & 0xCC) + (c & 0x330)) >> 2);
}

#endif
