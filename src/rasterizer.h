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

#include <stdint.h>

#define FIXED_SHIFT 4

#define DIGIT_HEIGHT 13
#define DIGIT_WIDTH 12

struct GBitmap;

struct scanline
{
    int start;
    int end;
};

void draw_digit(struct GBitmap *bmp, int x, int y, int n);
void draw_box(struct GBitmap *bmp, uint8_t color, int x, int y, int w, int h);
void draw_rect(struct GBitmap *bmp, struct scanline *scanlines,
               uint8_t color, int32_t px, int32_t py,
               int32_t dx, int32_t dy, int32_t len, int32_t w);
void draw_white_rect(struct GBitmap *bmp, struct scanline *scanlines,
                     int32_t px, int32_t py, int32_t dx, int32_t dy,
                     int32_t len, int32_t w);
void draw_circle(struct GBitmap *bmp, uint8_t color, int32_t cx, int32_t cy,
                 int32_t r);
void draw_white_circle(struct GBitmap *bmp, int32_t cx, int32_t cy, int32_t r);

static inline int32_t fixed(int i)
{
   return (int32_t)i << FIXED_SHIFT;
}

#endif
