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
    MARKER_KEY,
    DAYCOLORS_KEY,
    STATUSCONF_KEY,
    DIALNUMBERS_KEY,
    NUM_KEYS
};

struct
{
    Window *window;
    uint8_t bgcol;
    int hour, min, sec;

    int showsec;

    int num_scanlines;
    struct scanline *scanlines;

    struct {
        int32_t r;
        uint8_t col;
    } center[2];

    bool outline;

    struct {
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

    struct {
        int32_t w, r0, r1;
        uint8_t col;
    } hour_hand, min_hand, sec_hand;

    struct {
        int32_t w, h;
    } marker;

    struct {
        uint8_t col;
        bool show;
    } dialnumbers;

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
    int x0 = (x - DIGIT_WIDTH) & ~0x3;
    int x1 = x0 + DIGIT_WIDTH + 4;
    int my = 2;

    draw_week(bmp, x0, y + my);

    int d10 = g.day.ofmonth / 10;
    int d01 = g.day.ofmonth - d10 * 10;
    draw_digit(bmp, g.daycolors.dayofmonth, x0, y - DIGIT_HEIGHT - my, d10);
    draw_digit(bmp, g.daycolors.dayofmonth, x1, y - DIGIT_HEIGHT - my, d01);
}

static void clear_day(GBitmap *bmp, int px, int py)
{
    int my = 2;
    int iy0 = py - DIGIT_HEIGHT - my;
    int iy1 = py + 11 + my;
    int ix0 = (px - DIGIT_WIDTH) >> 2;
    int ix1 = ix0 + ((2 * DIGIT_WIDTH + 4) >> 2);
    uint32_t col4 =
        (g.bgcol << 24) | (g.bgcol << 16) | (g.bgcol << 8) | g.bgcol;

    for (int y = iy0; y < iy1; ++y)
    {
        GBitmapDataRowInfo row = gbitmap_get_data_row_info(bmp, y);
        uint32_t *line = (uint32_t *)row.data;
        for (int x = ix0; x < ix1; ++x)
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
    int y0 = (y - SMALL_DIGIT_HEIGHT / 2);
    int d10 = n / 10;
    int d01 = n - d10 * 10;

    if (!pad && n == 0)
    {
        d10 = 1;
        d01 = 2;
    }

    if (pad || d10 != 0)
    {
        int spc = SMALL_DIGIT_WIDTH / 3;
        int x0 = (x - SMALL_DIGIT_WIDTH - spc / 2);
        draw_small_digit(bmp, g.dialnumbers.col, x0, y0, d10);
        draw_small_digit(bmp, g.dialnumbers.col,
                         x0 + SMALL_DIGIT_WIDTH + spc, y0, d01);
        update_scanlines(g.scanlines, y0, y0 + SMALL_DIGIT_HEIGHT,
                         x0, x0 + 2 * SMALL_DIGIT_WIDTH + spc);
    }
    else
    {
        int x0 = (x - SMALL_DIGIT_WIDTH /2);
        draw_small_digit(bmp, g.dialnumbers.col, x0, y0, d01);
        update_scanlines(g.scanlines, y0, y0 + SMALL_DIGIT_HEIGHT,
                         x0, x0 + SMALL_DIGIT_WIDTH);
    }
}

static void draw_marker(GBitmap *bmp, uint8_t col,
                        int cx, int cy, int a, int r, int s, int n, bool pad)
{
    int32_t sina = sin_lookup(a);
    int32_t cosa = cos_lookup(a);

    if (g.marker.h > 0 && g.marker.w > 0)
    {
        int32_t px = cx + sina * s / TRIG_MAX_RATIO;
        int32_t py = cy - cosa * s / TRIG_MAX_RATIO;
        int32_t dx = -sina * fixed(256) / TRIG_MAX_RATIO;
        int32_t dy = cosa * fixed(256) / TRIG_MAX_RATIO;

        draw_rect(bmp, g.scanlines, col, px, py, dx, dy, r,
                  g.marker.w / 2, g.outline);
    }

    if (g.dialnumbers.show && n >= 0)
    {
        int32_t t = (s - fixed(11) - r);
        int32_t half = 1 << (FIXED_SHIFT - 1);
        int nx = (cx + sina * t / TRIG_MAX_RATIO + half) >> FIXED_SHIFT;
        int ny = (cy - cosa * t / TRIG_MAX_RATIO + half) >> FIXED_SHIFT;
        draw_dial_digits(bmp, nx, ny, n, pad);
    }
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
}

static inline int absi(int i)
{
    return i < 0 ? -i : i;
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

static void render(GContext *ctx)
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

    GRect bounds = gbitmap_get_bounds(bmp);

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
    if (g.showsec)
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

        int dx = -(hour.dx + min.dx) / 2;
        int dy = -(hour.dy + min.dy) / 2;
        if (dx == 0 && dy == 0)
        {
            dx = hour.dy;
            dy = -hour.dx;
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
            else if (g.day.update || g.showsec)
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
            int i;
            for (i = 0; i < 4; ++i)
                if (!blocked[(first + i) % 4])
                    break;
            int s = (i + first) % 4;

            int px = w2 + sector_x(s) * r;
            int py = h2 + sector_y(s) * r;

            if (show_disconnected())
            {
                draw_disconnected(bmp, g.scanlines, g.statusconf.color, px, py);
                blocked[s] = 2;
                for (i = 0; i < 4; ++i)
                    if (blocked[(first + i) % 4] < 2)
                        break;
                s = (i + first) % 4;
                px = w2 + sector_x(s) * r;
                py = h2 + sector_y(s) * r;
            }

            if (show_battery())
                draw_battery(bmp, g.scanlines, g.statusconf.color, px, py,
                             g.status.batstate.charge_percent);
        }
    }

    if (! g.day.show && g.day.update && (g.day.px || g.day.py))
    {
        clear_day(bmp, g.day.px, g.day.py);
    }

    g.day.update = false;


    // dial marker
    {
        int32_t s = mr * 15 / 16;
        int32_t r = g.marker.h;

        int minmark = ((g.min + 2) % 60) * 12 / 60;
        int hourmark = ((g.hour * 60 + g.min + 30) % 720) * 12 / 720;

        int32_t c =
            g.showsec ? ((g.sec + 2) % 60) * 12 / 60 * TRIG_MAX_ANGLE / 12 : -1;
        int32_t a = minmark * TRIG_MAX_ANGLE / 12;
        int32_t b = hourmark * TRIG_MAX_ANGLE / 12;

        draw_marker(bmp, g.min_hand.col, cx, cy, a, r, s, minmark * 5, true);
        if (b != a)
            draw_marker(bmp, g.hour_hand.col, cx, cy, b, r, s, hourmark, false);
        if (c >= 0 && c != a && c != b)
            draw_marker(bmp, g.sec_hand.col, cx, cy, c, r, s, -1, false);
    }

    // draw hour hand
    {
        int32_t px = cx - hour.dx * mr * g.hour_hand.r0 / (fixed(256) * 256);
        int32_t py = cy - hour.dy * mr * g.hour_hand.r0 / (fixed(256) * 256);
        int32_t len = mr * (g.hour_hand.r1 + g.hour_hand.r0) / 256;
        draw_rect(bmp, g.scanlines, g.hour_hand.col, px, py,
                           hour.dx, hour.dy, len, g.hour_hand.w / 2, g.outline);
    }

    // draw minute hand
    {
        int32_t px = cx - min.dx * mr * g.min_hand.r0 / (fixed(256) * 256);
        int32_t py = cy - min.dy * mr * g.min_hand.r0 / (fixed(256) * 256);
        int32_t len = mr * (g.min_hand.r1 + g.min_hand.r0) / 256;
        draw_rect(bmp, g.scanlines, g.min_hand.col, px, py,
                           min.dx, min.dy, len, g.min_hand.w / 2, g.outline);
    }

    draw_circle(bmp, g.center[0].col, cx, cy, g.center[0].r, g.outline);

    if (g.showsec)
    {
        int32_t px = cx - sec.dx * mr * g.sec_hand.r0 / (fixed(256) * 256);
        int32_t py = cy - sec.dy * mr * g.sec_hand.r0 / (fixed(256) * 256);
        int32_t len = mr * (g.sec_hand.r1 + g.sec_hand.r0) / 256;
        draw_rect(bmp, g.scanlines, g.sec_hand.col,
                  px, py, sec.dx, sec.dy, len, g.sec_hand.w / 2, g.outline);
        draw_circle(bmp, g.center[1].col, cx, cy, g.center[1].r, g.outline);
    }


    graphics_release_frame_buffer(ctx, bmp);
}

static void redraw(struct Layer *layer, GContext *ctx)
{
    render(ctx);
}

static void tick_handler(struct tm *t, TimeUnits units_changed)
{
    if (g.showsec > 0 && --g.showsec == 0)
        tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
    update_time(t);
    g.status.batstate = battery_state_service_peek();
    layer_mark_dirty(window_get_root_layer(g.window));
}

static void read_settings(void)
{
    APP_LOG(APP_LOG_LEVEL_DEBUG, "reading settings");
    if (persist_exists(SHOWSEC_KEY))
    {
        g.showsec = persist_read_bool(SHOWSEC_KEY) ? -1 : 0;
        APP_LOG(APP_LOG_LEVEL_DEBUG, "showsec: %i", g.showsec != 0);
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
    if (persist_exists(MARKER_KEY))
    {
        persist_read_data(MARKER_KEY, &g.marker, sizeof(g.marker));
        APP_LOG(APP_LOG_LEVEL_DEBUG, "marker: %d, %d",
                (int)g.marker.w, (int)g.marker.h);
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
}

static void save_settings(void)
{
    APP_LOG(APP_LOG_LEVEL_DEBUG, "saving settings");
    persist_write_bool(SHOWSEC_KEY, g.showsec != 0);
    persist_write_bool(SHOWDAY_KEY, g.day.show);
    persist_write_bool(OUTLINE_KEY, g.outline);
    persist_write_int(BGCOL_KEY, (int)(unsigned)g.bgcol);
    persist_write_data(HOUR_KEY, &g.hour_hand, sizeof(g.hour_hand));
    persist_write_data(MIN_KEY, &g.min_hand, sizeof(g.min_hand));
    persist_write_data(SEC_KEY, &g.sec_hand, sizeof(g.sec_hand));
    persist_write_data(CENTER_KEY, &g.center, sizeof(g.center));
    persist_write_data(MARKER_KEY, &g.marker, sizeof(g.marker));
    persist_write_data(DAYCOLORS_KEY, &g.daycolors, sizeof(g.daycolors));
    persist_write_data(STATUSCONF_KEY, &g.statusconf, sizeof(g.statusconf));
    persist_write_data(DIALNUMBERS_KEY, &g.dialnumbers, sizeof(g.dialnumbers));
}

static inline int32_t clamp(int32_t val, int32_t max)
{
    return val < max ? val : max;
}

static void message_received(DictionaryIterator *iter, void *context)
{
    Tuple *t;
    if ((t = dict_find(iter, SHOWSEC_KEY))) {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "showsec: %d", (int)t->value->int32);
        g.showsec = t->value->int32 != 0 ? -1 : 0;
        tick_timer_service_subscribe(g.showsec ? SECOND_UNIT : MINUTE_UNIT,
                                     tick_handler);
    }
    if ((t = dict_find(iter, SHOWDAY_KEY))) {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "showday: %d", (int)t->value->int32);
        g.day.show = t->value->int32 != 0;
        g.day.update = true;
    }
    if ((t = dict_find(iter, OUTLINE_KEY))) {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "outline: %d", (int)t->value->int32);
        g.outline = t->value->int32 != 0;
    }
    if ((t = dict_find(iter, BGCOL_KEY))) {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "bgcol: 0x%x", (int)t->value->uint8);
        g.bgcol = t->value->uint8;
        free(g.scanlines);
        g.scanlines = NULL;
    }
    if ((t = dict_find(iter, HOUR_KEY))) {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "hour: 0x%x", (int)t->value->uint32);
        g.hour_hand.r0 = clamp(t->value->int32 & 0xFF, 230);
        g.hour_hand.r1 = clamp((t->value->int32 >> 8) & 0xFF, 230);
        g.hour_hand.w = (t->value->int32 >> 16) & 0xFF;
        g.hour_hand.col = (t->value->uint32 >> 24) & 0xFF;
    }
    if ((t = dict_find(iter, MIN_KEY))) {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "min: 0x%x", (int)t->value->uint32);
        g.min_hand.r0 = clamp(t->value->int32 & 0xFF, 230);
        g.min_hand.r1 = clamp((t->value->int32 >> 8) & 0xFF, 230);
        g.min_hand.w = (t->value->int32 >> 16) & 0xFF;
        g.min_hand.col = (t->value->uint32 >> 24) & 0xFF;
    }
    if ((t = dict_find(iter, SEC_KEY))) {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "sec: 0x%x", (int)t->value->uint32);
        g.sec_hand.r0 = clamp(t->value->int32 & 0xFF, 230);
        g.sec_hand.r1 = clamp((t->value->int32 >> 8) & 0xFF, 230);
        g.sec_hand.w = (t->value->int32 >> 16) & 0xFF;
        g.sec_hand.col = (t->value->uint32 >> 24) & 0xFF;
    }
    if ((t = dict_find(iter, CENTER_KEY))) {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "center: 0x%x", (int)t->value->uint32);
        g.center[0].r =
            clamp((t->value->int32 >> 24) & 0xFF, 32) << (FIXED_SHIFT - 1);
        g.center[0].col = (t->value->uint32 >> 16) & 0xFF;
        g.center[1].r =
            clamp((t->value->int32 >> 8) & 0xFF, 32) << (FIXED_SHIFT - 1);
        g.center[1].col = t->value->uint32 & 0xFF;
    }
    if ((t = dict_find(iter, MARKER_KEY))) {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "marker: 0x%x", (int)t->value->uint32);
        g.marker.w = fixed(clamp((t->value->int32 >> 8) & 0xFF, 16));
        g.marker.h = fixed(clamp(t->value->int32 & 0xFF, 32));
    }
    if ((t = dict_find(iter, DAYCOLORS_KEY))) {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "daycolors: 0x%x", (int)t->value->uint32);
        g.daycolors.dayofmonth = (t->value->uint32 >> 24) & 0xFF;
        g.daycolors.weekday = (t->value->uint32 >> 16) & 0xFF;
        g.daycolors.sunday = (t->value->uint32 >> 8) & 0xFF;
        g.daycolors.today = t->value->uint32 & 0xFF;
    }
    if ((t = dict_find(iter, STATUSCONF_KEY))) {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "statusconf: 0x%x", (int)t->value->uint32);
        g.statusconf.showconn = ((t->value->uint32 >> 24) & 0xFF) != 0;
        g.statusconf.vibepattern = (t->value->uint32 >> 16) & 0xFF;
        g.statusconf.warnlevel = (t->value->uint32 >> 8) & 0xFF;
        g.statusconf.color = t->value->uint32 & 0xFF;
    }
    if ((t = dict_find(iter, DIALNUMBERS_KEY))) {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "dialnums: 0x%x", (int)t->value->uint32);
        g.dialnumbers.show = ((t->value->uint32 >> 8) & 0xFF) != 0;
        g.dialnumbers.col = t->value->uint32 & 0xFF;
    }

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
    tick_timer_service_subscribe(g.showsec ? SECOND_UNIT : MINUTE_UNIT,
                                 tick_handler);
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
    free(g.scanlines);
    g.scanlines = NULL;
}

static void init()
{
    app_message_register_inbox_received(message_received);
    // needed inbox size, see note at dict_calc_buffer_size
    uint32_t insize = NUM_KEYS * (7 + sizeof(int32_t)) + 1;
    app_message_open(insize, 0);

    g.bgcol = 0xC0;
    g.outline = true;
    g.showsec = 0;
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
    g.hour_hand.r1 = 160;
    g.hour_hand.col = 0xFF;
    g.hour_hand.w = fixed(8);
    g.marker.w = fixed(4);
    g.marker.h = fixed(5);
    g.center[0].col = 0xFF;
    g.center[0].r = fixed(7);
    g.center[1].col = 0xF0;
    g.center[1].r = fixed(4);
    g.daycolors.dayofmonth = 0xFF;
    g.daycolors.weekday = 0xFF;
    g.daycolors.sunday = 0xDB;
    g.daycolors.today = 0xF0;
    g.statusconf.color = 0xFF;
    g.statusconf.showconn = true;
    g.statusconf.warnlevel = 10;
    g.statusconf.vibepattern = 3;
    g.dialnumbers.col = 0xAA;
    g.dialnumbers.show = false;

    read_settings();

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
}

int main(void)
{
    init();
    app_event_loop();
    deinit();
}
