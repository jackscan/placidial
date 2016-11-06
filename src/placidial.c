/*
 * Copyright(c) 2016 Mathias Fiedler
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files(the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
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

enum
{
    SHOWSEC_KEY,
    SHOWDAY_KEY,
    OUTLINE_KEY,
    BGCOL_KEY,
    HOUR_KEY,
    MIN_KEY,
    SEC_KEY,
    CENTER_KEY,
    HOURTICK_KEY,
    DAYCOLORS_KEY,
    STATUSCONF_KEY,
    DIALNUMBERS_KEY,
    HOURBELOW_KEY,
    MINTICK_KEY,
    LASTTICK_KEY,
    FONTS_KEY
};

#define NUM_MESSAGE_KEYS    42

#define DEMO 0
#define BENCH 0

struct hand_conf
{
    int32_t w, r0, r1;
    uint8_t col;
};

struct tick_conf
{
    int32_t w, h;
    uint8_t col;
    uint8_t show;
};

enum
{
    BLOCKY_FONT,
    BLOCKY_SMALL_FONT,
    SMOOTH_FONT,
    SMOOTH_SMALL_FONT,
};

struct
{
    Window *window;
    uint8_t bgcol;
    int hour, min, sec;

    int showsec;
    int seccount;
    int num_scanlines;
    struct scanline *scanlines;

    struct {
        int32_t r;
        uint8_t col;
    } center[2];

    bool outline;
    bool hourhand_below;
    uint8_t last_tick;

    struct {
        struct bmpset font;
        int ofweek, ofmonth;
        int px, py;
        bool update;
        bool show;
    } day;

    struct
    {
        uint8_t dayofmonth;
        uint8_t weekday;
        uint8_t sunday;
        uint8_t today;
    } daycolors;

    struct hand_conf hour_hand, min_hand, sec_hand;

    struct tick_conf hour_tick, min_tick;

    // uint8_t rounded_rect;

    struct {
        uint8_t col;
        uint8_t show;
    } dialnumbers;

    struct {
        uint8_t day;
        uint8_t dial;
    } fontconf;

    struct bmpset dialfont;

    struct {
        BatteryChargeState batstate;
        bool connected;
    } status;

    struct {
        uint8_t warnlevel;
        uint8_t color;
        uint8_t vibepattern;
        bool showconn;
    } statusconf;
} g;

static inline uint32_t get_aa_colors(uint8_t bg, uint8_t col)
{
    int od = g.outline ? 3 : 4;
    uint32_t b = bg;
    uint32_t c = col;
    if (! dark_color(bg))
        return (uint32_t)blend_inv(b, c, 1, od)
            | (((uint32_t)blend_inv(b, c, 2, od)) << 8)
            | (((uint32_t)blend_inv(b, c, 3, od)) << 16)
            | (c << 24);
    else
        return (uint32_t)blend(b, c, 1, od)
            | (((uint32_t)blend(b, c, 2, od)) << 8)
            | (((uint32_t)blend(b, c, 3, od)) << 16)
            | (c << 24);
}

static inline uint32_t get_colors(uint8_t bg, uint8_t col)
{
    uint32_t b = bg;
    uint32_t c = col;
    return b
        | (((uint32_t)blend(b, c, 2, 5)) << 8)
        | (((uint32_t)blend(b, c, 3, 5)) << 16)
        | (c << 24);
}

static void draw_week(GBitmap *bmp, int x, int y)
{
    const int w = 4;
    const int dx = 8;
    const int dy = 7;

    for (int i = 0; i < 4; ++i)
    {
        uint8_t color = i == g.day.ofweek
                            ? g.daycolors.today
                            : i == 0 ? g.daycolors.sunday : g.daycolors.weekday;
        draw_box(bmp, color, x + i * dx, y, w, w);
    }

    for (int i = 1; i < 4; ++i)
    {
        uint8_t color =
            i + 3 == g.day.ofweek ? g.daycolors.today : g.daycolors.weekday;
        draw_box(bmp, color, x + i * dx, y + dy, w, w);
    }
}

static void draw_day(GBitmap *bmp, int x, int y)
{
    int x0 = (x - g.day.font.w - 2);
    int x1 = x0 + g.day.font.w + 4;
    int my = 2;

    draw_week(bmp, x0, y + my);

    int d10 = g.day.ofmonth / 10;
    int d01 = g.day.ofmonth - d10 * 10;
    uint32_t colors = get_colors(g.bgcol, g.daycolors.dayofmonth);
    int y0 = y - g.day.font.h - my;
    if (x0 & 0x3)
    {
        draw_2bit_bmp(bmp, &g.day.font, d10, x0, y0, colors);
        draw_2bit_bmp(bmp, &g.day.font, d01, x1, y0, colors);
    }
    else
    {
        draw_2bit_bmp_aligned(bmp, &g.day.font, d10, x0, y0, colors);
        draw_2bit_bmp_aligned(bmp, &g.day.font, d01, x1, y0, colors);
    }
}

struct rect { int x0, y0, x1, y1; };

static struct rect get_day_rect(int px, int py)
{
    int my = 2;
    return (struct rect){
        (px - g.day.font.w - 2) >> 2,
        py - g.day.font.h - my,
        (px + g.day.font.w + 4) >> 2,
        py + 11 + my,
    };
}

static void clear_day(GBitmap *bmp, int px, int py)
{
    struct rect r = get_day_rect(px, py);
    uint32_t col4 =
        (g.bgcol << 24) | (g.bgcol << 16) | (g.bgcol << 8) | g.bgcol;

    for (int y = r.y0; y < r.y1; ++y)
    {
        GBitmapDataRowInfo row = gbitmap_get_data_row_info(bmp, y);
        uint32_t *line = (uint32_t *)row.data;
        for (int x = r.x0; x < r.x1; ++x)
            line[x] = col4;
    }
}

static inline bool show_battery(void)
{
    return g.status.batstate.charge_percent <= g.statusconf.warnlevel;
}

static inline bool show_disconnected(void)
{
    return g.statusconf.showconn && ! g.status.connected;
}

static inline bool show_status(void)
{
    return show_disconnected() || show_battery();
}

static inline void update_scanlines(struct scanline *scanlines,
                                    int y0, int y1, int x0, int x1)
{
    int start = x0 >> 2;
    int end = (x1 + 3) >> 2;
    for (int y = y0; y < y1; ++y)
    {
        struct scanline *line = scanlines + y;
        if (line->start > start) line->start = start;
        if (line->end < end) line->end = end;
    }
}

static void draw_dial_digits(GBitmap *bmp, int x, int y, int n, bool pad)
{
    int y0 = (y - g.dialfont.h / 2);
    int d10 = n / 10;
    int d01 = n - d10 * 10;

    if (!pad && n == 0)
    {
        d10 = 1;
        d01 = 2;
    }

    uint32_t colors = get_colors(g.bgcol, g.dialnumbers.col);

    if (pad || d10 != 0)
    {
        int spc = 2;
        int x0 = (x - g.dialfont.w - spc / 2);
        int x1 = x0 + g.dialfont.w + spc;
        draw_2bit_bmp(bmp, &g.dialfont, d10, x0, y0, colors);
        draw_2bit_bmp(bmp, &g.dialfont, d01, x1, y0, colors);
        update_scanlines(g.scanlines, y0, y0 + g.dialfont.h,
                         x0, x0 + 2 * g.dialfont.w + spc);
    }
    else
    {
        int x0 = (x - g.dialfont.w / 2);
        draw_2bit_bmp(bmp, &g.dialfont, d01, x0, y0, colors);
        update_scanlines(g.scanlines, y0, y0 + g.dialfont.h,
                         x0, x0 + g.dialfont.w);
    }
}

static inline int absi(int i)
{
    return i < 0 ? -i : i;
}

static void draw_circle_tick(GBitmap *bmp, struct tick_conf *conf,
                             int32_t cx, int32_t cy, int a, int s)
{
    if (conf->h > 0 && conf->w > 0)
    {
        int32_t sina = sin_lookup(a);
        int32_t cosa = cos_lookup(a);

        int32_t px = cx + sina * s / TRIG_MAX_RATIO;
        int32_t py = cy - cosa * s / TRIG_MAX_RATIO;
        int32_t dx = -sina * fixed(256) / TRIG_MAX_RATIO;
        int32_t dy = cosa * fixed(256) / TRIG_MAX_RATIO;

        uint32_t colors = get_aa_colors(g.bgcol, conf->col);

        draw_bg_rect(bmp, g.scanlines, colors, px, py, dx, dy, conf->h,
                     conf->w / 2);
    }
}

/*
static void draw_rect_tick(GBitmap *bmp, struct tick_conf *conf,
                           int32_t cx, int32_t cy, int a,
                           int32_t w2, int32_t h2, int32_t r)
{
    if (conf->h > 0 && conf->w > 0)
    {
        int32_t sina = sin_lookup(a);
        int32_t cosa = cos_lookup(a);

        int32_t ax = absi(sina);
        int32_t ay = absi(cosa);

        int32_t dx = -sina * fixed(256) / TRIG_MAX_RATIO;
        int32_t dy = cosa * fixed(256) / TRIG_MAX_RATIO;

        uint32_t colors = get_aa_colors(g.bgcol, conf->col);

        if (ax * (h2 - r) > ay * w2)
        {
            int32_t w2h = w2 - conf->h;
            int32_t px = cx + (dx < 0 ? w2h : -w2h);
            int32_t py = cy - cosa * w2h / ax;

            draw_hstrip(bmp, g.scanlines, colors, px, py, -dx, -dy,
                        conf->h, conf->w / 2);
        }
        else if (ax * h2 < ay * (w2 - r))
        {
            int32_t h2h = h2 - conf->h;
            int32_t px = cx + sina * h2h / ay;
            int32_t py = cy + (dy < 0 ? h2h : -h2h);

            draw_vstrip(bmp, g.scanlines, colors, px, py, -dx, -dy,
                        conf->h, conf->w / 2);
        }
        else
        {
            int32_t w2h = w2 - conf->h;
            int32_t h2h = h2 - conf->h;

            // normal of direction
            // shorten normal to avoid overflow
            int32_t nx = dy >> 1;
            int32_t ny = -dx >> 1;
            int32_t snx2 = (nx * nx);
            int32_t ny2 = (ny * ny) >> FIXED_SHIFT;
            // vector from corner circle center to watchface center
            int32_t ex = (dx > 0 ? w2h - r : r - w2h);
            int32_t ey = (dy > 0 ? h2h - r : r - h2h);
            int32_t sen = (ex * nx + ey * ny);
            int32_t en = sen >> FIXED_SHIFT;
            int32_t r2 = (r * r) >> FIXED_SHIFT;

            int32_t a = fixed(1) + snx2 / ny2;
            int32_t b = -en * nx / ny2;
            int32_t c = en * en / ny2 - r2;

            int32_t sqrt = sqrti(b * b - a * c);

            // (x,y) of intersection relative to corner circle center
            int32_t x = (((dx < 0 ? sqrt : - sqrt) - b) << FIXED_SHIFT) / a;
            int32_t y = (sen - x * nx) / ny;
            int32_t px = cx - ex + x;
            int32_t py = cy - ey + y;

            draw_bg_rect(bmp, g.scanlines, colors, px, py, -dx, -dy,
                         conf->h, conf->w / 2);
        }
    }
}
*/

static void draw_tick(GBitmap *bmp, struct tick_conf *conf,
                      int32_t cx, int32_t cy, int a,
                      int w2, int h2, int32_t s)
{
    int r = w2 < h2 ? w2 : h2;

    /*
    if (g.rounded_rect > 0 && g.rounded_rect < r)
    {
        int32_t sw = s * w2;
        int32_t sh = s * h2;
        int32_t fr = fixed(g.rounded_rect);
        draw_rect_tick(bmp, conf, cx, cy, a, sw, sh, fr);
    }
    else
    */
    {
        draw_circle_tick(bmp, conf, cx, cy, a, s * r);
    }
}

static void draw_dial_number(GBitmap *bmp, int n, bool pad,
                             int cx, int cy, int a, int32_t r)
{
    int32_t sina = sin_lookup(a);
    int32_t cosa = cos_lookup(a);
    int32_t w = g.dialfont.w;
    int32_t h = g.dialfont.h;
    int32_t t = r - fixed(w + h / 2);
    int32_t half = 1 << (FIXED_SHIFT - 1);
    int nx = (cx + sina * t / TRIG_MAX_RATIO + half) >> FIXED_SHIFT;
    int ny = (cy - cosa * t / TRIG_MAX_RATIO + half) >> FIXED_SHIFT;
    draw_dial_digits(bmp, nx, ny, n, pad);
}

static void update_time(struct tm *t)
{
    g.hour = t->tm_hour;
    g.min = t->tm_min;
    g.sec = t->tm_sec;
    if (t->tm_wday != g.day.ofweek || t->tm_mday != g.day.ofmonth)
    {
        g.day.ofweek = t->tm_wday;
        g.day.ofmonth = t->tm_mday;
        g.day.update = true;
    }
#if DEMO
    g.hour = g.min % 24;
    g.min = g.sec;
    // g.day.ofmonth = g.sec % 32;
    // g.day.update = true;
#endif
}

static inline int sector(int dx, int dy)
{
    return (absi(dx) < absi(dy) ? 1 : 0) | (dx + dy < 0 ? 2 : 0);
}

static inline int sector_x(int s)
{
    static const int dx[4] = { 1, 0, -1, 0 };
    return dx[s & 0x3];
}

static inline int sector_y(int s)
{
    static const int dy[4] = { 0, 1, 0, -1 };
    return dy[s & 0x3];
}

static void draw_status(GBitmap *bmp, int cx, int cy, int r,
                        int *blocked, int first)
{

    int i;
    for (i = 0; i < 4; ++i)
        if (!blocked[(first + i) % 4])
            break;
    int s = (i + first) % 4;

    int px = cx + sector_x(s) * r;
    int py = cy + sector_y(s) * r;

    if (show_disconnected())
    {
        draw_disconnected(bmp, g.scanlines, g.statusconf.color, px, py);
        blocked[s] = 2;
        for (i = 0; i < 4; ++i)
            if (blocked[(first + i) % 4] < 2)
                break;
        s = (i + first) % 4;
        px = cx + sector_x(s) * r;
        py = cy + sector_y(s) * r;
    }

    if (show_battery())
        draw_battery(bmp, g.scanlines, g.statusconf.color, px, py,
                     g.status.batstate.charge_percent);
}

static void draw_hand(struct GBitmap *bmp,struct hand_conf *conf, int32_t mr,
                      int32_t cx, int32_t cy, int32_t dx, int32_t dy, bool bg)
{
    int32_t px = cx - dx * mr * conf->r0 / (fixed(256) * 256);
    int32_t py = cy - dy * mr * conf->r0 / (fixed(256) * 256);
    int32_t len = mr * (conf->r1 + conf->r0) / 256;
    if (bg)
    {
        uint32_t colors = get_aa_colors(g.bgcol, conf->col);
        draw_bg_rect(bmp, g.scanlines, colors, px, py, dx, dy, len,
                     conf->w / 2);
    }
    else
        draw_rect(bmp, g.scanlines, conf->col, px, py, dx, dy, len, conf->w / 2,
                  g.outline, dark_color(g.bgcol));
}

static bool show_seconds(void)
{
    return g.showsec < 0 || (g.showsec > 0 && g.seccount > 0);
}

static void render(GContext *ctx, GRect bounds)
{
    GBitmap *bmp = graphics_capture_frame_buffer(ctx);
    if (bmp == NULL)
    {
        APP_LOG(APP_LOG_LEVEL_ERROR, "failed to capture framebuffer");
        return;
    }

    // get date and time if unset
    if (g.day.ofmonth == 0)
    {
        time_t t = time(NULL);
        update_time(localtime(&t));
    }

    GRect bmpbounds = gbitmap_get_bounds(bmp);
    grect_clip(&bounds, &bmpbounds);

    int16_t w2 = bounds.size.w / 2;
    int16_t h2 = bounds.size.h / 2;
    // max radius
    int32_t mr = fixed(w2 < h2 ? w2 : h2);

    // first clear
    if (g.scanlines == NULL || g.num_scanlines < bounds.size.h)
    {
        free(g.scanlines);

        g.num_scanlines = bounds.size.h;
        g.scanlines = calloc(g.num_scanlines, sizeof(*g.scanlines));

        // clear background
        for (int y = 0; y < bounds.size.h; ++y)
        {
            GBitmapDataRowInfo row = gbitmap_get_data_row_info(bmp, y);
            memset(row.data + row.min_x, g.bgcol, row.max_x - row.min_x + 1);
            struct scanline *sl = g.scanlines + y;
            sl->start = bounds.size.w;
            sl->end = 0;
        }
    }
    else
    {
        // check if we need to redraw day due to cleared scanlines
        if (! g.day.update)
        {
            struct rect r = get_day_rect(g.day.px, g.day.py);
            for (int y = r.y0; y < r.y1; ++y)
            {
                struct scanline *sl = g.scanlines + y;
                if (sl->start < r.x1 && sl->end > r.x0)
                {
                    g.day.update = true;
                    break;
                }
            }
        }

        // clear scanlines
        uint32_t col4 =
            (g.bgcol << 24) | (g.bgcol << 16) | (g.bgcol << 8) | g.bgcol;
        for (int y = 0; y < g.num_scanlines; ++y)
        {
            struct scanline *sl = g.scanlines + y;
            GBitmapDataRowInfo row = gbitmap_get_data_row_info(bmp, y);
            uint32_t *line = (uint32_t *)row.data;
            for (int x = sl->start; x < sl->end; ++x)
                line[x] = col4;
            sl->start = bounds.size.w;
            sl->end = 0;
        }
    }
    int fr = g.center[0].r > g.center[1].r ? g.center[0].r : g.center[1].r;
    int r = (fr + 0xf) >> FIXED_SHIFT;
    for (int y = h2 - r - 1; y < h2 + r + 1; ++y)
    {
        struct scanline *sl = g.scanlines + y;
        sl->start = (w2 - r - 1) >> 2;
        sl->end = (w2 + r + 1 + 3) >> 2;
    }

    int32_t cx = fixed(w2);
    int32_t cy = fixed(h2);

    struct
    {
        int32_t dx, dy;
    } hour, min, sec = { 0 };

    // hour
    {
        int32_t a = ((g.hour * 60 + g.min) * TRIG_MAX_ANGLE) / 720;
        int32_t sina = sin_lookup(a);
        int32_t cosa = cos_lookup(a);
        hour.dx = sina * fixed(256) / TRIG_MAX_RATIO;
        hour.dy = -cosa * fixed(256) / TRIG_MAX_RATIO;
    }

    // minute
    {
        int32_t a = (g.min * TRIG_MAX_ANGLE) / 60;
        int32_t sina = sin_lookup(a);
        int32_t cosa = cos_lookup(a);
        min.dx = sina * fixed(256) / TRIG_MAX_RATIO;
        min.dy = -cosa * fixed(256) / TRIG_MAX_RATIO;
    }

    // second
    if (show_seconds())
    {
        int32_t a = (g.sec * TRIG_MAX_ANGLE) / 60;
        int32_t sina = sin_lookup(a);
        int32_t cosa = cos_lookup(a);
        sec.dx = sina * fixed(256) / TRIG_MAX_RATIO;
        sec.dy = -cosa * fixed(256) / TRIG_MAX_RATIO;
    }

    // day
    if (g.day.show || show_status())
    {
        int r = (mr >> FIXED_SHIFT) * 9 / 16;

        int hdx = hour.dx;
        int hdy = hour.dy;
        int mdx = min.dx;
        int mdy = min.dy;

        if (g.last_tick & 0x1)
        {
            int32_t a = ((g.hour % 12) * TRIG_MAX_ANGLE) / 12;
            int32_t sina = sin_lookup(a);
            int32_t cosa = cos_lookup(a);
            hdx = sina * fixed(256) / TRIG_MAX_RATIO;
            hdy = -cosa * fixed(256) / TRIG_MAX_RATIO;
        }

        if (g.last_tick & 0x2)
        {
            int32_t a = ((g.min / 5) * TRIG_MAX_ANGLE) / 12;
            int32_t sina = sin_lookup(a);
            int32_t cosa = cos_lookup(a);
            mdx = sina * fixed(256) / TRIG_MAX_RATIO;
            mdy = -cosa * fixed(256) / TRIG_MAX_RATIO;
        }

        int dx = -(hdx + mdx) / 2;
        int dy = -(hdy + mdy) / 2;
        if (dx == 0 && dy == 0)
        {
            dx = hdy;
            dy = -hdx;
        }

        if (absi(dx) > absi(dy))
        {
            dx = dx > 0 ? r : -r;
            dy = 0;
        }
        else
        {
            dx = 0;
            dy = dy > 0 ? r : -r;
        }

        int px = w2 + dx;
        int py = h2 + dy;

        if (g.day.show)
        {
            if (g.day.px != px || g.day.py != py)
            {
                if (g.day.px || g.day.py)
                    clear_day(bmp, g.day.px, g.day.py);
                draw_day(bmp, px, py);
                g.day.px = px;
                g.day.py = py;
            }
            else if (g.day.update)
                draw_day(bmp, g.day.px, g.day.py);
        }

        // find places for status icons
        if (show_status())
        {
            int blocked[4] = { 0 };
            if (g.day.show) blocked[sector(dx, dy)] = 2;
            blocked[sector(hour.dx, hour.dy)] = 1;
            blocked[sector(min.dx, min.dy)] = 1;
            int first = sector(-dx, -dy);

            draw_status(bmp, w2, h2, r, blocked, first);
        }
    }

    if (! g.day.show && g.day.update && (g.day.px || g.day.py))
    {
        clear_day(bmp, g.day.px, g.day.py);
    }

    g.day.update = false;


    // dial marker
    {
        int32_t s = fixed(15) / 16;

        int round60 = (g.last_tick & 0x1) == 0 ? 30 : 0;
        int round5 = (g.last_tick & 0x2) == 0 ? 2 : 0;
        int hourmark = ((g.hour * 60 + g.min + round60) % 720) * 12 / 720;
        int32_t c = show_seconds()
                        ? ((g.sec + 2) % 60) * 12 / 60 * TRIG_MAX_ANGLE / 12
                        : -1;
        int32_t b = g.hour_tick.show ? hourmark * TRIG_MAX_ANGLE / 12 : -1;
        if (c == b) c = -1;

        {
            int minmark = (g.min + round5) / 5;
            int a = (minmark * TRIG_MAX_ANGLE / 12) % TRIG_MAX_ANGLE;
            if (b == a || b == a) b = -1;
            if (c == a || c == a) c = -1;

            if (g.hour_tick.show)
                draw_tick(bmp, &g.hour_tick, cx, cy, a, w2, h2, s);

            if (g.min_tick.show)
            {
                int mm = minmark * 5;
                int m1 = g.min < mm ? g.min : mm + 1;
                int m2 = g.min < mm ? mm - 1 : g.min;

                for (int i = m1; i <= m2; ++i)
                {
                    int32_t a = (i * TRIG_MAX_ANGLE / 60) % TRIG_MAX_ANGLE;
                    draw_tick(bmp, &g.min_tick, cx, cy, a, w2, h2, s);
                }
            }
        }

        if (b >= 0)
            draw_tick(bmp, &g.hour_tick, cx, cy, b, w2, h2, s);
        if (c >= 0)
        {
            // workaround for missing sec_tick config
            struct tick_conf sec_tick = g.hour_tick;
            sec_tick.col = g.sec_hand.col;
            draw_tick(bmp, &sec_tick, cx, cy, c, w2, h2, s);
        }

        if (g.dialnumbers.show)
        {
            int b = -1;
            int h = -1;

            if (g.dialnumbers.show & 0x1)
            {
                int f = clock_is_24h_style() ? 24 : 12;
                h = ((g.hour * 60 + g.min + round60) / 60) % f;
                if (h == 0) h = f;
                b = (h % 12) * TRIG_MAX_ANGLE / 12;
            }

            int32_t sr = (s * mr) >> FIXED_SHIFT;
            int32_t sn = g.hour_tick.show ? sr - g.hour_tick.h : sr;

            if (g.dialnumbers.show & 0x2)
            {
                int m = (((g.min + round5) / 5) * 5) % 60;
                int a = m * TRIG_MAX_ANGLE / 60;
                draw_dial_number(bmp, m, true, cx, cy, a, sn);
                if (b == a) b = -1;
            }

            if (b >= 0)
                draw_dial_number(bmp, h, false, cx, cy, b, sn);
        }

    }

    if (g.hourhand_below)
    {
        draw_hand(bmp, &g.hour_hand, mr, cx, cy, hour.dx, hour.dy, true);
        draw_hand(bmp, &g.min_hand, mr, cx, cy, min.dx, min.dy, false);
    }
    else
    {
        draw_hand(bmp, &g.min_hand, mr, cx, cy, min.dx, min.dy, true);
        draw_hand(bmp, &g.hour_hand, mr, cx, cy, hour.dx, hour.dy, false);
    }

    draw_circle(bmp, g.center[0].col, cx, cy, g.center[0].r, g.outline,
                dark_color(g.bgcol));

    if (show_seconds())
    {
        draw_hand(bmp, &g.sec_hand, mr, cx, cy, sec.dx, sec.dy, false);
        draw_circle(bmp, g.center[1].col, cx, cy, g.center[1].r, g.outline,
                    dark_color(g.bgcol));
    }


    graphics_release_frame_buffer(ctx, bmp);
}

static void redraw(struct Layer *layer, GContext *ctx)
{
    // APP_LOG(APP_LOG_LEVEL_DEBUG, "redraw");
    GRect bounds = layer_get_unobstructed_bounds(layer);
#if BENCH
    uint16_t start = time_ms(NULL, NULL);
    for (int i = 0; i < 12; ++i)
    {
        g.hour = i;
        for (int j = 0; j < 10; ++j)
        {
            g.min = j * 6;
            render(ctx, bounds);
        }
    }
    uint16_t end = time_ms(NULL, NULL);

    if (end < start) end += 1000;
    APP_LOG(APP_LOG_LEVEL_DEBUG, "dt: %d", (end - start) * 10 / 12);
#else
    render(ctx, bounds);
#endif
}

static void tick_handler(struct tm *t, TimeUnits units_changed)
{
    if (g.seccount > 0 && --g.seccount == 0) {
        tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
    }
    update_time(t);
    g.status.batstate = battery_state_service_peek();
    layer_mark_dirty(window_get_root_layer(g.window));
}

static void tap_handler(AccelAxisType axis, int32_t direction)
{
    g.seccount = g.showsec + 1;
    tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
}

static void load_bmpset(struct bmpset *set, uint32_t resid, int size)
{
    set->bmp = gbitmap_create_with_resource(resid);
    struct GRect bounds = gbitmap_get_bounds(set->bmp);
    set->w = bounds.size.w;
    set->h = bounds.size.h / size;
}

static uint32_t font_to_resource_id(uint8_t fontid)
{
    switch (fontid)
    {
    case BLOCKY_FONT: return RESOURCE_ID_BLOCKY13;
    case BLOCKY_SMALL_FONT: return RESOURCE_ID_BLOCKY9;
    case SMOOTH_FONT: return RESOURCE_ID_DIGITS15;
    case SMOOTH_SMALL_FONT: return RESOURCE_ID_DIGITS13;
    default: return 0;
    }
}

static void cleanup_fonts(void)
{
    if (g.day.font.bmp) gbitmap_destroy(g.day.font.bmp);
    if (g.dialfont.bmp) gbitmap_destroy(g.dialfont.bmp);
}

static void load_fonts(void)
{
    cleanup_fonts();

    uint32_t dayfontid = font_to_resource_id(g.fontconf.day);
    uint32_t dialfontid = font_to_resource_id(g.fontconf.dial);

    load_bmpset(&g.day.font, dayfontid, 10);
    load_bmpset(&g.dialfont, dialfontid, 10);
}

static void read_settings(void)
{
    APP_LOG(APP_LOG_LEVEL_DEBUG, "reading settings");
    if (persist_exists(SHOWSEC_KEY))
    {
        g.showsec = persist_read_int(SHOWSEC_KEY);
        // convert old setting to new timeout setting
        if (g.showsec == 1)
            g.showsec = -1;
        APP_LOG(APP_LOG_LEVEL_DEBUG, "showsec: %i", g.showsec);
    }
    if (persist_exists(SHOWDAY_KEY))
    {
        g.day.show = persist_read_bool(SHOWDAY_KEY);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "day.show: %i", g.day.show);
    }
    if (persist_exists(OUTLINE_KEY))
    {
        g.outline = persist_read_bool(OUTLINE_KEY);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "outline: %i", g.outline);
    }
    if (persist_exists(HOURBELOW_KEY))
    {
        g.hourhand_below = persist_read_bool(HOURBELOW_KEY);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "hourbelow: %i", g.hourhand_below);
    }
    if (persist_exists(BGCOL_KEY))
    {
        g.bgcol = (uint8_t)persist_read_int(BGCOL_KEY);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "bgcol: 0x%x", g.bgcol);
    }
    if (persist_exists(HOUR_KEY))
    {
        persist_read_data(HOUR_KEY, &g.hour_hand, sizeof(g.hour_hand));
        APP_LOG(APP_LOG_LEVEL_DEBUG, "hour: %d, %d, %d, 0x%x",
                (int)g.hour_hand.r0, (int)g.hour_hand.r1,
                (int)g.hour_hand.w, g.hour_hand.col);
    }
    if (persist_exists(MIN_KEY))
    {
        persist_read_data(MIN_KEY, &g.min_hand, sizeof(g.min_hand));
        APP_LOG(APP_LOG_LEVEL_DEBUG, "min: %d, %d, %d, 0x%x",
                (int)g.min_hand.r0, (int)g.min_hand.r1,
                (int)g.min_hand.w, g.min_hand.col);
    }
    if (persist_exists(SEC_KEY))
    {
        persist_read_data(SEC_KEY, &g.sec_hand, sizeof(g.sec_hand));
        APP_LOG(APP_LOG_LEVEL_DEBUG, "sec: %d, %d, %d, 0x%x",
                (int)g.sec_hand.r0, (int)g.sec_hand.r1,
                (int)g.sec_hand.w, g.sec_hand.col);
    }
    if (persist_exists(CENTER_KEY))
    {
        persist_read_data(CENTER_KEY, &g.center, sizeof(g.center));
        APP_LOG(APP_LOG_LEVEL_DEBUG, "center: %d, 0x%x, %d, 0x%x",
                (int)g.center[0].r, g.center[0].col,
                (int)g.center[1].r, g.center[1].col);
    }
    if (persist_exists(HOURTICK_KEY))
    {
        persist_read_data(HOURTICK_KEY, &g.hour_tick, sizeof(g.hour_tick));
        // legacy support
        if (g.hour_tick.col == 0)
        {
            g.hour_tick.show = g.hour_tick.w > 0 && g.hour_tick.h > 0;
            g.hour_tick.col = g.hour_hand.col;
        }
        APP_LOG(APP_LOG_LEVEL_DEBUG, "hourtick: %d, %d, %x, %d",
                (int)g.hour_tick.w, (int)g.hour_tick.h,
                g.hour_tick.col, (int)g.hour_tick.show);
    }
    if (persist_exists(DAYCOLORS_KEY))
    {
        persist_read_data(DAYCOLORS_KEY, &g.daycolors, sizeof(g.daycolors));
        APP_LOG(APP_LOG_LEVEL_DEBUG, "daycolors: 0x%x, 0x%x, 0x%x, 0x%x",
                g.daycolors.dayofmonth, g.daycolors.weekday,
                g.daycolors.sunday, g.daycolors.today);
    }
    if (persist_exists(STATUSCONF_KEY))
    {
        persist_read_data(STATUSCONF_KEY, &g.statusconf, sizeof(g.statusconf));
        APP_LOG(APP_LOG_LEVEL_DEBUG, "statusconf: 0x%x, %d, 0x%x, %d",
                g.statusconf.color, g.statusconf.showconn,
                g.statusconf.vibepattern, g.statusconf.warnlevel);
    }
    if (persist_exists(DIALNUMBERS_KEY))
    {
        persist_read_data(DIALNUMBERS_KEY, &g.dialnumbers,
                          sizeof(g.dialnumbers));
        APP_LOG(APP_LOG_LEVEL_DEBUG, "dialnumbers: %d, 0x%x",
                g.dialnumbers.show, g.dialnumbers.col);
    }
    if (persist_exists(MINTICK_KEY))
    {
        persist_read_data(MINTICK_KEY, &g.min_tick, sizeof(g.min_tick));
        APP_LOG(APP_LOG_LEVEL_DEBUG, "mintick: %d, %d, %x, %d",
                (int)g.min_tick.w, (int)g.min_tick.h,
                g.min_tick.col, (int)g.min_tick.show);
    }
    if (persist_exists(LASTTICK_KEY))
    {
        g.last_tick = (uint8_t)persist_read_int(LASTTICK_KEY);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "lasttick: 0x%x", (int)g.last_tick);
    }
    if (persist_exists(FONTS_KEY))
    {
        persist_read_data(FONTS_KEY, &g.fontconf, sizeof(g.fontconf));
        APP_LOG(APP_LOG_LEVEL_DEBUG, "fonts: %d, %d",
                (int)g.fontconf.day, (int)g.fontconf.dial);
    }
}

static void save_settings(void)
{
    APP_LOG(APP_LOG_LEVEL_DEBUG, "saving settings");
    persist_write_int(SHOWSEC_KEY, g.showsec);
    persist_write_bool(SHOWDAY_KEY, g.day.show);
    persist_write_bool(OUTLINE_KEY, g.outline);
    persist_write_bool(HOURBELOW_KEY, g.hourhand_below);
    persist_write_int(BGCOL_KEY, (int)(unsigned)g.bgcol);
    persist_write_data(HOUR_KEY, &g.hour_hand, sizeof(g.hour_hand));
    persist_write_data(MIN_KEY, &g.min_hand, sizeof(g.min_hand));
    persist_write_data(SEC_KEY, &g.sec_hand, sizeof(g.sec_hand));
    persist_write_data(CENTER_KEY, &g.center, sizeof(g.center));
    persist_write_data(HOURTICK_KEY, &g.hour_tick, sizeof(g.hour_tick));
    persist_write_data(DAYCOLORS_KEY, &g.daycolors, sizeof(g.daycolors));
    persist_write_data(STATUSCONF_KEY, &g.statusconf, sizeof(g.statusconf));
    persist_write_data(DIALNUMBERS_KEY, &g.dialnumbers, sizeof(g.dialnumbers));
    persist_write_data(MINTICK_KEY, &g.min_tick, sizeof(g.min_tick));
    persist_write_int(LASTTICK_KEY, (int)(unsigned)g.last_tick);
    persist_write_data(FONTS_KEY, &g.fontconf, sizeof(g.fontconf));
}

static inline uint32_t clamp(uint32_t val, uint32_t max)
{
    return val < max ? val : max;
}

#define CONFIG_SET_UINT(C, K, M) ({\
    if ((t = dict_find(iter, MESSAGE_KEY_##K))) { \
        int32_t n = t->value->int32; \
        if (t->type == TUPLE_CSTRING) \
            n = atoi(t->value->cstring); \
        APP_LOG(APP_LOG_LEVEL_DEBUG, #K ": 0x%x", (int)n); \
        C = clamp(n & 0xFF, M); \
    } \
    t != NULL; \
})

#define CONFIG_SET_LENGTH(C, K, M) ({\
    if ((t = dict_find(iter, MESSAGE_KEY_##K))) { \
        APP_LOG(APP_LOG_LEVEL_DEBUG, #K ": 0x%x", (int)t->value->uint32); \
        C = clamp((t->value->uint32 * 255 / 100), M); \
    } \
    t != NULL; \
})

#define CONFIG_SET_WIDTH(C, K, M, S) ({\
    if ((t = dict_find(iter, MESSAGE_KEY_##K))) { \
        APP_LOG(APP_LOG_LEVEL_DEBUG, #K ": 0x%x", (int)t->value->uint32); \
        C = clamp(t->value->int32 & 0xFF, M) << (FIXED_SHIFT - S); \
    } \
    t != NULL; \
})

#define CONFIG_SET_TOGGLE(C, K) ({\
    if ((t = dict_find(iter, MESSAGE_KEY_##K))) { \
        APP_LOG(APP_LOG_LEVEL_DEBUG, #K": %d", (int)t->value->int32); \
        C = t->value->int32 != 0; \
    } \
    t != NULL; \
})

#define CONFIG_SET_COLOR(C, K) ({\
    if ((t = dict_find(iter, MESSAGE_KEY_##K))) { \
        C = GColorFromHEX(t->value->int32).argb; \
        APP_LOG(APP_LOG_LEVEL_DEBUG, #K": 0x%x", (int)C); \
    } \
    t != NULL; \
})

static void message_received(DictionaryIterator *iter, void *context)
{
    Tuple *t;

    uint32_t secshow = 0;
    uint32_t sectimeout = 0;
    CONFIG_SET_UINT(secshow, showsec, 2);
    CONFIG_SET_UINT(sectimeout, sectimeout, 120);

    switch (secshow) {
    default:
    case 0: g.showsec = 0; break;
    case 1: g.showsec = -1; break;
    case 2: g.showsec = sectimeout; break;
    }
    APP_LOG(APP_LOG_LEVEL_DEBUG, "showsec: %d", (int)g.showsec);

    g.seccount = 0;
    if (g.showsec > 0) accel_tap_service_subscribe(tap_handler);
    else accel_tap_service_unsubscribe();

    tick_timer_service_subscribe(show_seconds() ? SECOND_UNIT : MINUTE_UNIT,
                                 tick_handler);
 #if DEMO || BENCH
    tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
 #endif

    if (CONFIG_SET_TOGGLE(g.day.show, dayshow))
        g.day.update = true;

    CONFIG_SET_TOGGLE(g.outline, outline);

    if (CONFIG_SET_COLOR(g.bgcol, bgcol))
    {
        free(g.scanlines);
        g.scanlines = NULL;
    }

    CONFIG_SET_TOGGLE(g.hourhand_below, hourbelowmin);

    CONFIG_SET_LENGTH(g.hour_hand.r0, hourext, 85);
    CONFIG_SET_LENGTH(g.hour_hand.r1, hourlen, 230);
    CONFIG_SET_WIDTH(g.hour_hand.w, hourwidth, 16, 0);
    CONFIG_SET_COLOR(g.hour_hand.col, hourcol);

    CONFIG_SET_LENGTH(g.min_hand.r0, minext, 85);
    CONFIG_SET_LENGTH(g.min_hand.r1, minlen, 230);
    CONFIG_SET_WIDTH(g.min_hand.w, minwidth, 16, 0);
    CONFIG_SET_COLOR(g.min_hand.col, mincol);

    CONFIG_SET_LENGTH(g.sec_hand.r0, secext, 85);
    CONFIG_SET_LENGTH(g.sec_hand.r1, seclen, 230);
    CONFIG_SET_WIDTH(g.sec_hand.w, secwidth, 16, 0);
    CONFIG_SET_COLOR(g.sec_hand.col, seccol);

    CONFIG_SET_WIDTH(g.center[0].r, centerwidth, 32, 1);
    CONFIG_SET_COLOR(g.center[0].col, centercol);
    CONFIG_SET_WIDTH(g.center[1].r, seccenterwidth, 32, 1);
    CONFIG_SET_COLOR(g.center[1].col, seccentercol);

    uint32_t hourtick = 0;
    CONFIG_SET_UINT(hourtick, hourtickshow, 2);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "hourtick: 0x%x", (int)hourtick);
    CONFIG_SET_COLOR(g.hour_tick.col, hourtickcol);
    CONFIG_SET_WIDTH(g.hour_tick.w, hourtickwidth, 16, 0);
    CONFIG_SET_WIDTH(g.hour_tick.h, hourticklen, 32, 0);
    g.hour_tick.show = hourtick != 0;

    uint32_t mintick = 0;
    CONFIG_SET_UINT(mintick, mintickshow, 2);
    CONFIG_SET_COLOR(g.min_tick.col, mintickcol);
    CONFIG_SET_WIDTH(g.min_tick.w, mintickwidth, 16, 0);
    CONFIG_SET_WIDTH(g.min_tick.h, minticklen, 32, 0);
    g.min_tick.show = mintick != 0;

    g.last_tick = (hourtick & 1) | ((mintick & 1) << 1);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "lasttick: 0x%x", (int)g.last_tick);

    CONFIG_SET_COLOR(g.daycolors.dayofmonth, daycol);
    CONFIG_SET_COLOR(g.daycolors.weekday, weekcol);
    CONFIG_SET_COLOR(g.daycolors.sunday, sundaycol);
    CONFIG_SET_COLOR(g.daycolors.today, todaycol);

    CONFIG_SET_TOGGLE(g.statusconf.showconn, statusshow);
    CONFIG_SET_COLOR(g.statusconf.color, statuscol);
    CONFIG_SET_UINT(g.statusconf.vibepattern, statusvibe, 3);
    CONFIG_SET_UINT(g.statusconf.warnlevel, batwarn, 100);

    bool showhournum = false, showminnum = false;
    CONFIG_SET_TOGGLE(showhournum, hournumshow);
    CONFIG_SET_TOGGLE(showminnum, minnumshow);
    CONFIG_SET_COLOR(g.dialnumbers.col, dialnumcol);
    g.dialnumbers.show = (int)showhournum | ((int)showminnum << 1);

    uint32_t dayfont = 0;
    CONFIG_SET_UINT(dayfont, dayfont, 1);
    g.fontconf.day = dayfont * 2;

    uint32_t dialfont = 0;
    CONFIG_SET_UINT(dialfont, dialfont, 1);
    g.fontconf.dial = dialfont * 2 + 1;

    load_fonts();

    save_settings();

    layer_mark_dirty(window_get_root_layer(g.window));
}

static void connection_handler(bool connected)
{
    APP_LOG(APP_LOG_LEVEL_DEBUG, "bt: %s",
            connected ? "connected" : "disconnected");
    if (connected != g.status.connected)
    {
        g.status.connected = connected;
        if (! connected && g.statusconf.vibepattern)
        {
            switch (g.statusconf.vibepattern)
            {
            case 0: break;
            default:
            case 1: vibes_short_pulse(); break;
            case 2: vibes_long_pulse(); break;
            case 3: vibes_double_pulse(); break;
            }
        }
        layer_mark_dirty(window_get_root_layer(g.window));
    }
}

static void window_load(Window *window)
{
    Layer *window_layer = window_get_root_layer(window);
    layer_set_update_proc(window_layer, redraw);

    if (g.showsec > 0) accel_tap_service_subscribe(tap_handler);

    tick_timer_service_subscribe(show_seconds() ? SECOND_UNIT : MINUTE_UNIT,
                                 tick_handler);
#if DEMO || BENCH
    tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
#endif
    g.scanlines = NULL;
    g.status.batstate = battery_state_service_peek();

    g.status.connected = connection_service_peek_pebble_app_connection();
    ConnectionHandlers handlers = {
        .pebble_app_connection_handler = connection_handler,
    };
    connection_service_subscribe(handlers);
}

static void window_unload(Window *window)
{
    battery_state_service_unsubscribe();
    connection_service_unsubscribe();
    tick_timer_service_unsubscribe();
    accel_tap_service_unsubscribe();
    free(g.scanlines);
    g.scanlines = NULL;
}

static void init()
{
    app_message_register_inbox_received(message_received);
    // needed inbox size, see note at dict_calc_buffer_size
    uint32_t insize = NUM_MESSAGE_KEYS * (7 + sizeof(int32_t)) + 1;
    app_message_open(insize, 0);

    g.bgcol = 0xC0;
    g.outline = true;
    g.showsec = 0;
    g.seccount = 0;
    g.day.show = true;
    g.sec_hand.w = fixed(3);
    g.sec_hand.r0 = 40;
    g.sec_hand.r1 = 210;
    g.sec_hand.col = 0xF0;
    g.min_hand.w = fixed(8);
    g.min_hand.r0 = 0;
    g.min_hand.r1 = 210;
    g.min_hand.col = 0xFF;
    g.hour_hand.r0 = 0;
    g.hour_hand.r1 = 130;
    g.hour_hand.col = 0xFF;
    g.hour_hand.w = fixed(8);
    g.hourhand_below = false;
    g.last_tick = 2;
    g.hour_tick.w = fixed(4);
    g.hour_tick.h = fixed(6);
    g.hour_tick.col = 0xFF;
    g.hour_tick.show = 1;
    g.min_tick.w = fixed(3);
    g.min_tick.h = fixed(5);
    g.min_tick.col = 0xDB;
    g.min_tick.show = 1;
    g.center[0].col = 0xFF;
    g.center[0].r = fixed(7);
    g.center[1].col = 0xF0;
    g.center[1].r = fixed(4);
    g.daycolors.dayofmonth = 0xFF;
    g.daycolors.weekday = 0xFF;
    g.daycolors.sunday = 0xCB;
    g.daycolors.today = 0xF0;
    g.statusconf.color = 0xFF;
    g.statusconf.showconn = true;
    g.statusconf.warnlevel = 10;
    g.statusconf.vibepattern = 3;
    g.dialnumbers.col = 0xAA;
    g.dialnumbers.show = 0;
    g.fontconf.day = SMOOTH_FONT;
    g.fontconf.dial = SMOOTH_SMALL_FONT;
    // g.rounded_rect = 0;

    read_settings();

    load_fonts();

    g.window = window_create();
    window_set_window_handlers(g.window,
                               (WindowHandlers){
                                   .load = window_load, .unload = window_unload,
                               });
    window_stack_push(g.window, true);
}

static void deinit()
{
    window_destroy(g.window);
    cleanup_fonts();
}

int main(void)
{
    init();
    app_event_loop();
    deinit();
}
