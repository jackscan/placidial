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

static inline uint8_t blend(uint32_t x, uint32_t y, int a, int d)
{
    uint32_t b = ((x & 0x33) * (d - a) + (y & 0x33) * a);
    uint32_t c = ((x & 0xCC) * (d - a) + ((y | 0xC0) & 0xCC) * a);
    return (uint8_t)(((b & 0xCC) + (c & 0x330)) >> 2);
}

void draw_box(struct GBitmap *bmp, uint8_t color, int x, int y, int w, int h)
{
    for (int i = 0; i < h; ++i)
    {
        uint8_t *line = gbitmap_get_data_row_info(bmp, (unsigned)(y + i)).data;
        for (int j = 0; j < w; ++j)
            line[x + j] = color;
    }
}

void draw_circle(struct GBitmap *bmp, uint8_t color, int32_t cx, int32_t cy,
                 int32_t r, bool outline)
{
    int32_t half = (1 << (FIXED_SHIFT - 1));
    int smooth = 2;
    int32_t fs2 = fixed(smooth)/2;
    int od = outline ? 3 : 4;

    int y0 = fixedfloor(cy - r);
    int y1 = fixedceil(cy + r);
    int x0 = fixedfloor(cx - r);
    int x1 = fixedfloor(cx + r);

    int32_t r0 = r - fs2;
    int32_t r1 = r + fs2;
    int32_t r2 = r1 * r1;
    int32_t rs = r2 - r0 * r0;

    for (int y = y0; y < y1; ++y)
    {
        int32_t dy = fixed(y) + half - cy;
        uint8_t *line = gbitmap_get_data_row_info(bmp, (unsigned)y).data;
        for (int x = x0; x < x1; ++x)
        {
            int32_t dx = fixed(x) + half - cx;
            int32_t ds = dx * dx + dy * dy;
            int32_t a = (r2 - ds) * 4 / rs;
            if (a <= 0) continue;
            if (a >= 4) line[x] = color;
            else line[x] = blend(line[x], color, a, od);
        }
    }
}

void draw_white_circle(struct GBitmap *bmp, int32_t cx, int32_t cy, int32_t r)
{
    int32_t half = (1 << (FIXED_SHIFT - 1));
    int smooth = 2;
    int32_t fs2 = fixed(smooth)/2;

    int y0 = fixedfloor(cy - r);
    int y1 = fixedceil(cy + r);
    int x0 = fixedfloor(cx - r);
    int x1 = fixedfloor(cx + r);

    int32_t r0 = r - fs2;
    int32_t r1 = r + fs2;
    int32_t r2 = r1 * r1;
    int32_t rs = r2 - r0 * r0;

    int32_t s = fixed(1024) / rs;
    r2 *= s;

    for (int y = y0; y < y1; ++y)
    {
        int32_t dy = fixed(y) + half - cy;
        uint8_t *line = gbitmap_get_data_row_info(bmp, (unsigned)y).data;
        for (int x = x0; x < x1; ++x)
        {
            int32_t dx = fixed(x) + half - cx;
            int32_t ds = dx * dx + dy * dy;
            int32_t a = (r2 - ds * s) >> (8 + FIXED_SHIFT);
            if (a <= 0) continue;
            if (a >= 3) line[x] = 0xFF;
            else line[x] = 0xC0 | a | (a << 2) | (a << 4);
        }
    }
}

static inline void update_scanline(struct scanline *line, int x0, int x1)
{
    int start = x0 >> 2;
    int end = (x1 + 3) >> 2;
    if (line->start > start) line->start = start;
    if (line->end < end) line->end = end;
}

void draw_white_rect(struct GBitmap *bmp, struct scanline *scanlines,
                     int32_t px, int32_t py, int32_t dx, int32_t dy,
                     int32_t len, int32_t w)
{
    // length of (dx, dy) is assumed to be fixed(256)
    const int dshift = FIXED_SHIFT + 8;

    // (dx, dy) shall point downwards or right
    if (dy < 0 || (dy == 0 && dx < 0))
    {
        px += (dx * len) >> dshift;
        py += (dy * len) >> dshift;
        dx = -dx;
        dy = -dy;
    }

    int32_t half = (1 << (FIXED_SHIFT - 1));

    int smooth = 2;
    int32_t fs2 = fixed(smooth)/2;
    int32_t wi = w - fs2;
    w += fs2;
    int32_t s0 = -fs2;
    int32_t s1 = len + fs2;
    int32_t t0 = dx < 0 ? s0 + 2 * fs2 : s0;
    int32_t t1 = dx < 0 ? s1 : s1 - 2 * fs2;

    int32_t wdx = ((dx < 0 ? -dx : dx) * w) >> dshift;
    int32_t sdy = (fs2 * dy) >> dshift;
    int y0 = fixedfloor(py - wdx - sdy);
    int y1 = fixedceil(py + ((dy * len) >> dshift) + wdx + sdy);

    int32_t ws0 = w << dshift;
    int32_t ws1 = wi << dshift;
    int32_t pxdy = px * dy;
    int32_t pxdx = px * dx;

    for (int y = y0; y < y1; ++y)
    {
        int32_t fy = fixed(y) + half;
        int32_t fydx = (fy - py) * dx;
        int32_t fydy = (fy - py) * dy;

        int32_t x0, x1, x2;

        if (dy > 0)
        {
            x0 = (fydx - ws0) / dy;
            x1 = (fydx + ws1) / dy;
            x2 = (fydx + ws0) / dy;

            if (dx != 0)
            {
                int32_t x3 = ((t0 << dshift) - fydy) / dx;
                int32_t x4 = ((t1 << dshift) - fydy) / dx;
                int32_t x5 =
                    ((dx < 0 ? s0 << dshift : s1 << dshift) - fydy) / dx;
                if (x3 > x4) swapi(&x3, &x4);
                if (x3 > x0) x0 = x3;
                if (x4 < x1) x1 = x4;
                if (x5 < x2) x2 = x5;
            }
        }
        else
        {
            x0 = t0;
            x1 = t1;
            x2 = s1;
        }

        uint8_t *line = gbitmap_get_data_row_info(bmp, (unsigned)y).data;
        int ix0 = fixedfloor(x0 + px);
        int ix1 = fixedfloor(x1 + px);
        int ix2 = fixedfloor(x2 + px);
        int32_t dys = dy << FIXED_SHIFT;
        int32_t dxs = dx << FIXED_SHIFT;

        update_scanline(scanlines + y, ix0, ix2);

        int32_t d0s = fydx + pxdy - (fixed(ix0) + half) * dy;
        int32_t d1s = (fixed(ix0) + half) * dx - pxdx + fydy;
        int x;
        for (x = ix0; x < ix1; ++x)
        {
            int32_t d0 = d0s >> dshift;
            int32_t d1 = d1s >> dshift;
            d0s -= dys;
            d1s += dxs;
            int32_t d = mini(d0 < 0 ? w + d0 : w - d0,
                             d1 < fs2 ? d1 - s0 : s1 - d1);

            // int a = (d * 4 / smooth) >> FIXED_SHIFT;
            int a = d >> (FIXED_SHIFT - 1);
            if (a <= 0) continue;
            if (a >= 3) break;
            else
            {
                uint8_t c = 0xC0 | a | (a << 2) | (a << 4);
                if (c > line[x]) line[x] = c;
            }
        }

        for (; x < ix1; ++x)
            line[x] = 0xFF;

        d0s = fydx + pxdy - (fixed(x) + half) * dy;
        d1s = (fixed(x) + half) * dx - pxdx + fydy;
        for (; x < ix2; ++x)
        {
            int32_t d0 = d0s >> dshift;
            int32_t d1 = d1s >> dshift;
            d0s -= dys;
            d1s += dxs;
            int32_t d = mini(d0 < 0 ? w + d0 : w - d0,
                             d1 < fs2 ? d1 - s0 : s1 - d1);

            // int a = (d * 4 / smooth) >> FIXED_SHIFT;
            int a = d >> (FIXED_SHIFT - 1);
            if (a <= 0) continue;
            if (a >= 3) line[x] = 0xFF;
            else
            {
                uint8_t c = 0xC0 | a | (a << 2) | (a << 4);
                if (c > line[x]) line[x] = c;
            }
        }
    }
}

void draw_rect(struct GBitmap *bmp, struct scanline *scanlines,
               uint8_t color, int32_t px, int32_t py,
               int32_t dx, int32_t dy, int32_t len, int32_t w, bool outline)
{
    // length of (dx, dy) is assumed to be fixed(256)
    const int dshift = FIXED_SHIFT + 8;

    // (dx, dy) shall point downwards or right
    if (dy < 0 || (dy == 0 && dx < 0))
    {
        px += (dx * len) >> dshift;
        py += (dy * len) >> dshift;
        dx = -dx;
        dy = -dy;
    }

    int32_t half = (1 << (FIXED_SHIFT - 1));

    int od = outline ? 3 : 4;
    int smooth = 2;
    int32_t fs2 = fixed(smooth)/2;
    int32_t wi = w - fs2;
    w += fs2;
    int32_t s0 = -fs2;
    int32_t s1 = len + fs2;
    int32_t t0 = dx < 0 ? s0 + 2 * fs2 : s0;
    int32_t t1 = dx < 0 ? s1 : s1 - 2 * fs2;

    int32_t wdx = ((dx < 0 ? -dx : dx) * w) >> dshift;
    int32_t sdy = (fs2 * dy) >> dshift;
    int y0 = fixedfloor(py - wdx - sdy);
    int y1 = fixedceil(py + ((dy * len) >> dshift) + wdx + sdy);

    int32_t ws0 = w << dshift;
    int32_t ws1 = wi << dshift;
    int32_t pxdy = px * dy;
    int32_t pxdx = px * dx;

    for (int y = y0; y < y1; ++y)
    {
        int32_t fy = fixed(y) + half;
        int32_t fydx = (fy - py) * dx;
        int32_t fydy = (fy - py) * dy;

        int32_t x0, x1, x2;

        if (dy > 0)
        {
            x0 = (fydx - ws0) / dy;
            x1 = (fydx + ws1) / dy;
            x2 = (fydx + ws0) / dy;

            if (dx != 0)
            {
                int32_t x3 = ((t0 << dshift) - fydy) / dx;
                int32_t x4 = ((t1 << dshift) - fydy) / dx;
                int32_t x5 =
                    ((dx < 0 ? s0 << dshift : s1 << dshift) - fydy) / dx;
                if (x3 > x4) swapi(&x3, &x4);
                if (x3 > x0) x0 = x3;
                if (x4 < x1) x1 = x4;
                if (x5 < x2) x2 = x5;
            }
        }
        else
        {
            x0 = t0;
            x1 = t1;
            x2 = s1;
        }

        uint8_t *line = gbitmap_get_data_row_info(bmp, (unsigned)y).data;
        int ix0 = fixedfloor(x0 + px);
        int ix1 = fixedfloor(x1 + px);
        int ix2 = fixedfloor(x2 + px);
        int32_t dys = dy << FIXED_SHIFT;
        int32_t dxs = dx << FIXED_SHIFT;

        update_scanline(scanlines + y, ix0, ix2);

        int32_t d0s = fydx + pxdy - (fixed(ix0) + half) * dy;
        int32_t d1s = (fixed(ix0) + half) * dx - pxdx + fydy;
        int x;
        for (x = ix0; x < ix1; ++x)
        {
            int32_t d0 = d0s >> dshift;
            int32_t d1 = d1s >> dshift;
            d0s -= dys;
            d1s += dxs;
            int32_t d = mini(d0 < 0 ? w + d0 : w - d0,
                             d1 < fs2 ? d1 - s0 : s1 - d1);

            int a = (d * 4 / smooth) >> FIXED_SHIFT;
            if (a <= 0) continue;
            if (a >= 4) break;
            else line[x] = blend(line[x], color, a, od);
        }

        // only valid for opaque color
        for (; x < ix1; ++x)
            line[x] = color;

        d0s = fydx + pxdy - (fixed(x) + half) * dy;
        d1s = (fixed(x) + half) * dx - pxdx + fydy;
        for (; x < ix2; ++x)
        {
            int32_t d0 = d0s >> dshift;
            int32_t d1 = d1s >> dshift;
            d0s -= dys;
            d1s += dxs;
            int32_t d = mini(d0 < 0 ? w + d0 : w - d0,
                             d1 < fs2 ? d1 - s0 : s1 - d1);

            int a = (d * 4 / smooth) >> FIXED_SHIFT;
            if (a <= 0) continue;
            if (a >= 4) line[x] = color;
            else line[x] = blend(line[x], color, a, od);
        }
    }
}

void draw_digit(struct GBitmap *bmp, uint8_t color, int x, int y, int n)
{
    static const uint32_t digitmask[5] = {
        07777717737,
        05541114425,
        07747756725,
        04545474125,
        07747747777,
    };

    int h = 3;
    int s = x >> 2;
    int n3 = n * 3;
    uint32_t col4 = (color << 24) | (color << 16) | (color << 8) | color;

    for (int r = 0; r < 5; ++r)
    {
        int k = r < 1 || 2 < r ? h : h - 1;
        for (int i = 0; i < k; ++i, ++y)
        {
            uint32_t mask = (digitmask[r] >> n3) & 0x7;
            uint32_t *line = (uint32_t *)gbitmap_get_data_row_info(bmp, y).data;
            for (int j = 0; mask; ++j, mask >>= 1)
                if (mask & 1) line[s + j] = col4;
        }
    }

}

void draw_disconnected(struct GBitmap *bmp, struct scanline *scanlines,
                       uint8_t color, int cx, int cy)
{
    static const uint32_t bitmask[] = {
        0b0000001111000000,
        0b0000011111000000,
        0b0000111111000000,
        0b0001111111000000,
        0b0011110111000000,
        0b0111100111000111,
        0b1111000111001111,
        0b0111100111011110,
        0b0011110111111100,
        0b0001111111111000,
        0b0000111111110000,
        0b0000011111100000,
        0b0000111111110000,
        0b0001111111111000,
        0b0011110111111100,
        0b0111100111011110,
        0b1111000111001111,
        0b0111100111000111,
        0b0011110111000000,
        0b0001111111000000,
        0b0000111111000000,
        0b0000011111000000,
        0b0000001111000000,
    };

    int w = 32;


    int h = ARRAY_LENGTH(bitmask);

    int x = cx - w / 2;
    int y = cy - h / 2;

    for (int r = 0; r < h; ++r, ++y)
    {
        uint32_t mask = bitmask[r];
        uint8_t *line = gbitmap_get_data_row_info(bmp, y).data;
        for (int j = 0; mask; ++j, mask >>= 1)
            if (mask & 0x1)
                line[x + j] = color;

        update_scanline(scanlines + y, x, x + w);
    }
}

void draw_battery(struct GBitmap *bmp, struct scanline *scanlines,
                  uint8_t color, int cx, int cy, uint8_t level)
{
    int w = 22;
    int h = 10;
    int b = 2;

    int x = cx - w / 2;
    int y = cy - h / 2;

    int l = (level * (w - b * 3 - 2) + 50)/ 100;
    uint8_t *line;

    for (int j = 0; j < b; ++j, ++y)
    {
        line = gbitmap_get_data_row_info(bmp, y).data;
        for (int i = 0; i < w - b; ++i)
            line[i + x] = color;
        update_scanline(scanlines + y, x, x + w);
    }

    for (int j = b; j < h - b; ++j, ++y)
    {
        line = gbitmap_get_data_row_info(bmp, y).data;
        for (int i = 0; i < b; ++i) line[x + i] = color;
        int k = b;
        if (j > b && j < h - b - 1)
        {
            for (int i = 0; i < l; ++i)
                line[x + b + 1 + i] = color;
            k += b;
        }

        for (int i = 0; i < k; ++i) line[x + w - 2 * b + i] = color;

        update_scanline(scanlines + y, x, x + w);
    }

    for (int j = 0; j < b; ++j, ++y)
    {
        line = gbitmap_get_data_row_info(bmp, y).data;
        for (int i = 0; i < w - b; ++i)
            line[i + x] = color;
        update_scanline(scanlines + y, x, x + w);
    }

    update_scanline(scanlines + y, x, x + w);
}
