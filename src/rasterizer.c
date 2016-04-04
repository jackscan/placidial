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

#include "rasterizer.h"

#include <pebble.h>

static inline int fixedfloor(int32_t f)
{
    // only for positive values
    return f >> FIXED_SHIFT;
}

static inline int fixedceil(int32_t f)
{
    // only for positive values
    return (f + 0xF) >> FIXED_SHIFT;
}

static inline void swapi(int32_t *a, int32_t *b)
{
    int32_t c = *a;
    *a = *b;
    *b = c;
}

static inline int32_t mini(int32_t a, int32_t b)
{
    return a < b ? a : b;
}

static inline uint8_t blend(uint32_t x, uint32_t y, int a)
{
    uint32_t b = ((x & 0x33) * (5 - a) + (y & 0x33) * a);
    uint32_t c = ((x & 0xCC) * (5 - a) + ((y | 0xC0) & 0xCC) * a);
    return (uint8_t)(((b & 0xCC) + (c & 0x330)) >> 2);
}

void draw_rect(struct GBitmap *bmp, uint8_t color, int32_t px, int32_t py,
                      int32_t dx, int32_t dy, int32_t len, int32_t w)
{
    // (dx, dy) shall point downwards or right
    if (dy < 0 || (dy == 0 && dx < 0))
    {
        px += dx;
        py += dy;
        dx = -dx;
        dy = -dy;
    }

    int32_t half = (1 << (FIXED_SHIFT - 1));

    int smooth = 2;
    int32_t fs2 = fixed(smooth)/2;
    w += fs2;
    int32_t s0 = -fs2;
    int32_t s1 = len + fs2;

    int32_t wdx = ((dx < 0 ? -dx : dx) * w) / len;
    int32_t sdy = (fs2 * dy) / len;
    int y0 = fixedfloor(py - wdx - sdy);
    int y1 = fixedceil(py + dy + wdx + sdy);

    int32_t wlen = w * len;
    int32_t pxdy = px * dy;
    int32_t pxdx = px * dx;

    for (int y = y0; y < y1; ++y)
    {
        int32_t fy = fixed(y) + half;
        int32_t fydx = (fy - py) * dx;
        int32_t fydy = (fy - py) * dy;

        int32_t x0, x1;

        if (dy > 0)
        {
            x0 = (fydx - wlen) / dy;
            x1 = (fydx + wlen) / dy;

            if (dx != 0)
            {
                int32_t x2 = (s0 * len - fydy) / dx;
                int32_t x3 = (s1 * len - fydy) / dx;
                if (x2 > x3) swapi(&x2, &x3);
                if (x2 > x0) x0 = x2;
                if (x3 < x1) x1 = x3;
            }
        }
        else
        {
            x0 = s0;
            x1 = s1;
        }

        uint8_t *line = gbitmap_get_data_row_info(bmp, (unsigned)y).data;
        int ix0 = fixedfloor(x0 + px);
        int ix1 = fixedfloor(x1 + px);
        for (int x = ix0; x < ix1; ++x)
        {
            int32_t fx = fixed(x) + half;
            int32_t d0 = (fydx + pxdy -  fx * dy) / len;
            int32_t d1 = (fx * dx - pxdx + fydy) / len;
            int32_t d = mini(d0 < 0 ? w + d0 : w - d0,
                             d1 < fs2 ? d1 - s0 : s1 - d1);

            int a = (d * 5 / smooth) >> FIXED_SHIFT;
            if (a <= 0) continue;
            if (a >= 5) line[x] = color;
            else line[x] = blend(line[x], color, a);
        }
    }
}
