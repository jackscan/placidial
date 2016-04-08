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
    struct {
        int32_t dx, dy;
        bool enable;
    } shadow;

    struct {
        int32_t w, h;
    } marker;
} g;

static void draw_marker(GBitmap *bmp, int cx, int cy, int a, int r, int s)
{
    int32_t sina = sin_lookup(a);
    int32_t cosa = cos_lookup(a);
    int32_t px = cx + sina * s / TRIG_MAX_RATIO;
    int32_t py = cy - cosa * s / TRIG_MAX_RATIO;
    int32_t dx = -sina * r / TRIG_MAX_RATIO;
    int32_t dy = cosa * r / TRIG_MAX_RATIO;

    draw_rect(bmp, 0xFF, px, py, dx, dy, r, g.marker.w);
}

static void redraw(struct Layer *layer, GContext *ctx)
{
    GBitmap *bmp = graphics_capture_frame_buffer(ctx);
    if (bmp == NULL)
    {
        APP_LOG(APP_LOG_LEVEL_ERROR, "failed to capture framebuffer");
        return;
    }

    AccelData accel;
    if (g.shadow.enable && accel_service_peek(&accel) >= 0)
    {
        g.shadow.dx = fixed(accel.x) / 256;
        g.shadow.dy = fixed(-accel.y) / 256;
    }

    APP_LOG(APP_LOG_LEVEL_INFO, "framebuffer");

    GRect bounds = gbitmap_get_bounds(bmp);

    int16_t w2 = bounds.size.w / 2;
    int16_t h2 = bounds.size.h / 2;
    // max radius
    int32_t mr = fixed(w2 < h2 ? w2 : h2);

    // clear background
    for (int y = 0; y < bounds.size.h; ++y)
    {
        GBitmapDataRowInfo row = gbitmap_get_data_row_info(bmp, y);
        memset(row.data + row.min_x, g.bgcol, row.max_x - row.min_x + 1);
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
        hour.dx = sina * hour.r / TRIG_MAX_RATIO;
        hour.dy = -cosa * hour.r / TRIG_MAX_RATIO;
    }

    // minute
    {
        min.r = mr * 7 / 8;
        int32_t a = (g.min * TRIG_MAX_ANGLE) / 60;
        int32_t sina = sin_lookup(a);
        int32_t cosa = cos_lookup(a);
        min.dx = sina * min.r / TRIG_MAX_RATIO;
        min.dy = -cosa * min.r / TRIG_MAX_RATIO;
    }

    // dial marker
    {
        int32_t s = mr * 15 / 16;
        int32_t r = g.marker.h;

        int32_t a = (g.min + 2) * 12 / 60 * TRIG_MAX_ANGLE / 12;
        int32_t b = (g.hour * 60 + g.min + 30) * 12 / 720 * TRIG_MAX_ANGLE / 12;

        draw_marker(bmp, cx, cy, a, r, s);
        if (a != b) draw_marker(bmp, cx, cy, b, r, s);
    }

    if (g.shadow.enable)
    {
        draw_rect(bmp, 0xC0, cx + g.shadow.dx - hour.dx / 8,
                  cy + g.shadow.dy - hour.dy / 8,
                  hour.dx, hour.dy, hour.r, fixed(4));
        draw_rect(bmp, 0xC0, cx + g.shadow.dx - min.dx / 8,
                  cy + g.shadow.dy - min.dy / 8,
                  min.dx, min.dy, min.r, fixed(4));
    }

    draw_rect(bmp, 0xFF, cx - hour.dx / 8, cy - hour.dy / 8,
              hour.dx, hour.dy, hour.r, fixed(4));
    draw_rect(bmp, 0xFF, cx - min.dx / 8, cy - min.dy / 8,
              min.dx, min.dy, min.r, fixed(4));

    graphics_release_frame_buffer(ctx, bmp);
}

static void tick_handler(struct tm *t, TimeUnits units_changed)
{
    g.hour = t->tm_hour;
    g.min = t->tm_min;
    g.sec = t->tm_sec;
    layer_mark_dirty(window_get_root_layer(g.window));
}

static void window_load(Window *window)
{
    Layer *window_layer = window_get_root_layer(window);
    layer_set_update_proc(window_layer, redraw);
    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
}

static void window_unload(Window *window)
{
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
