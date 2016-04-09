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

struct
{
    Window *window;
    uint8_t bgcol;
    int hour, min, sec;

    int num_scanlines;
    struct scanline *scanlines;

    struct {
        int ofweek, ofmonth;
        int px, py;
        bool update;
    } day;

    struct {
        int32_t w, h;
    } marker;
} g;

static void draw_week(GBitmap *bmp, int x, int y)
{
    const int w = 4;
    const int dx = 8;
    const int dy = 7;

    for (int i = 0; i < 4; ++i)
    {
        uint8_t color = i == g.day.ofweek ? 0xF0 : i == 0 ? 0xDB : 0xFF;
        draw_box(bmp, color, x + i * dx, y, w, w);
    }

    for (int i = 1; i < 4; ++i)
    {
        uint8_t color = i + 3 == g.day.ofweek ? 0xF0 : 0xFF;
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
    draw_digit(bmp, x0, y - DIGIT_HEIGHT - my, d10);
    draw_digit(bmp, x1, y - DIGIT_HEIGHT - my, d01);
}

static void clear_day(GBitmap *bmp, int px, int py)
{
    int my = 2;
    int iy0 = py - DIGIT_HEIGHT - my;
    int iy1 = py + 11 + my;
    int ix0 = (px - DIGIT_WIDTH) >> 2;
    int ix1 = ix0 + ((2 * DIGIT_WIDTH + 4) >> 2);

    for (int y = iy0; y < iy1; ++y)
    {
        GBitmapDataRowInfo row = gbitmap_get_data_row_info(bmp, y);
        uint32_t *line = (uint32_t *)row.data;
        for (int x = ix0; x < ix1; ++x)
            line[x] = 0;
    }
}

static void draw_marker(GBitmap *bmp, int cx, int cy, int a, int r, int s)
{
    int32_t sina = sin_lookup(a);
    int32_t cosa = cos_lookup(a);
    int32_t px = cx + sina * s / TRIG_MAX_RATIO;
    int32_t py = cy - cosa * s / TRIG_MAX_RATIO;
    int32_t dx = -sina * fixed(256) / TRIG_MAX_RATIO;
    int32_t dy = cosa * fixed(256) / TRIG_MAX_RATIO;

    draw_white_rect(bmp, g.scanlines, px, py, dx, dy, r, g.marker.w);
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

static void redraw(struct Layer *layer, GContext *ctx)
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
            memset(row.data + row.min_x, 0, row.max_x - row.min_x + 1);
            struct scanline *sl = g.scanlines + y;
            sl->start = bounds.size.w;
            sl->end = 0;
        }
    }
    else
    {
        // clear scanlines
        for (int y = 0; y < g.num_scanlines; ++y)
        {
            struct scanline *sl = g.scanlines + y;
            GBitmapDataRowInfo row = gbitmap_get_data_row_info(bmp, y);
            uint32_t *line = (uint32_t *)row.data;
            for (int x = sl->start; x < sl->end; ++x)
                line[x] = 0;
            sl->start = bounds.size.w;
            sl->end = 0;
        }
    }

    int32_t cx = fixed(w2);
    int32_t cy = fixed(h2);

    struct
    {
        int32_t dx, dy, r;
    } hour, min;

    // hour
    {
        hour.r = mr * 5 / 8;
        int32_t a = ((g.hour * 60 + g.min) * TRIG_MAX_ANGLE) / 720;
        int32_t sina = sin_lookup(a);
        int32_t cosa = cos_lookup(a);
        hour.dx = sina * fixed(256) / TRIG_MAX_RATIO;
        hour.dy = -cosa * fixed(256) / TRIG_MAX_RATIO;
    }

    // minute
    {
        min.r = mr * 7 / 8;
        int32_t a = (g.min * TRIG_MAX_ANGLE) / 60;
        int32_t sina = sin_lookup(a);
        int32_t cosa = cos_lookup(a);
        min.dx = sina * fixed(256) / TRIG_MAX_RATIO;
        min.dy = -cosa * fixed(256) / TRIG_MAX_RATIO;
    }

    // day
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

        if (g.day.update || g.day.px != px || g.day.py != py)
        {
            clear_day(bmp, g.day.px, g.day.py);
            draw_day(bmp, px, py);
            g.day.px = px;
            g.day.py = py;
            g.day.update = false;
        }
    }

    draw_white_circle(bmp, cx, cy, fixed(7));

    // dial marker
    {
        int32_t s = mr * 15 / 16;
        int32_t r = g.marker.h;

        int32_t a = (g.min + 2) * 12 / 60 * TRIG_MAX_ANGLE / 12;
        int32_t b = (g.hour * 60 + g.min + 30) * 12 / 720 * TRIG_MAX_ANGLE / 12;

        draw_marker(bmp, cx, cy, a, r, s);
        if (a != b) draw_marker(bmp, cx, cy, b, r, s);
    }

    draw_white_rect(bmp, g.scanlines, cx, cy,
                    hour.dx, hour.dy, hour.r, fixed(4));
    draw_white_rect(bmp, g.scanlines,
                    cx - min.dx / fixed(2), cy - min.dy / fixed(2),
                    min.dx, min.dy, min.r, fixed(4));


    graphics_release_frame_buffer(ctx, bmp);
}

static void tick_handler(struct tm *t, TimeUnits units_changed)
{
    update_time(t);
    layer_mark_dirty(window_get_root_layer(g.window));
}

static void window_load(Window *window)
{
    Layer *window_layer = window_get_root_layer(window);
    layer_set_update_proc(window_layer, redraw);
    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
    g.scanlines = NULL;
}

static void window_unload(Window *window)
{
    free(g.scanlines);
}

static void init()
{
    g.marker.w = fixed(2);
    g.marker.h = fixed(5);
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
