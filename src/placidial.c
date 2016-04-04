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
    int hour, min, sec;
} g;

static void redraw(struct Layer *layer, GContext *ctx)
{
    GBitmap *bmp = graphics_capture_frame_buffer(ctx);
    if (bmp == NULL)
    {
        APP_LOG(APP_LOG_LEVEL_ERROR, "failed to capture framebuffer");
        return;
    }

    APP_LOG(APP_LOG_LEVEL_INFO, "framebuffer");

    GRect bounds = gbitmap_get_bounds(bmp);
    GSize size = bounds.size;
    int stride = gbitmap_get_bytes_per_row(bmp);

    int16_t w2 = bounds.size.w / 2;
    int16_t h2 = bounds.size.h / 2;
    // max radius
    int32_t mr = fixed(w2 < h2 ? w2 : h2);

    // clear background
    for (int y = 0; y < bounds.size.h; ++y)
    {
        GBitmapDataRowInfo row = gbitmap_get_data_row_info(bmp, y);
        memset(row.data + row.min_x, 0x00, row.max_x - row.min_x + 1);
    }

    // render hour
    {
        int32_t r = mr * 5 / 8;
        int32_t a = ((g.hour * 60 + g.min) * TRIG_MAX_ANGLE) / 720;
        int32_t sina = sin_lookup(a);
        int32_t cosa = cos_lookup(a);

        int32_t dx = sina * r / TRIG_MAX_RATIO;
        int32_t dy = -cosa * r / TRIG_MAX_RATIO;

        draw_rect(bmp, 0xFF, fixed(w2) - dx / 8, fixed(h2) - dy / 8, dx, dy, r,
                  fixed(4));
    }

    // render minute
    {
        int32_t r = mr * 7 / 8;
        int32_t a = (g.min * TRIG_MAX_ANGLE) / 60;
        int32_t sina = sin_lookup(a);
        int32_t cosa = cos_lookup(a);

        int32_t dx = sina * r / TRIG_MAX_RATIO;
        int32_t dy = -cosa * r / TRIG_MAX_RATIO;

        draw_rect(bmp, 0xFF, fixed(w2) - dx / 8, fixed(h2) - dy / 8, dx, dy, r,
                  fixed(4));
    }

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
