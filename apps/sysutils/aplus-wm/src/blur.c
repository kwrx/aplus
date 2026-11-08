/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2019 Antonino Natale
 *
 *
 * This file is part of aplus.
 *
 * aplus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * aplus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with aplus.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @brief Frosted glass: what the frame being composited has already drawn under something, read back from the back buffer
 * at half size and blurred, for that something to be painted over.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <wm.h>


/**
 * @brief How many frosted things damage is widened for at once: the menu and the windows.
 */
#define WM_BLUR_AREAS_MAX (65 + WM_TASKBAR_GLASS_MAX)

#define WM_BLUR_BOX     WM_MAX(WM_BLUR_RADIUS / 2 / WM_BLUR_PASSES, 1)
#define WM_BLUR_SUPPORT (2 * WM_BLUR_PASSES * WM_BLUR_BOX + 4)


typedef struct {

    wm_rect_t shape;
    wm_rect_t reach;
    wm_blur_t* blur;
    int z;

} wm_blur_area_t;


typedef struct {

    wm_rect_t rect;
    int z;

} wm_blur_change_t;


static uint64_t wm_blur_frame;

static uint32_t* wm_blur_scratch;
static size_t wm_blur_scratch_size;


/**
 * @brief Reports what frosting a shape reads back: the shape and as far around it as the blur reaches, inside the display.
 *
 * @param shape The shape.
 * @return The rectangle.
 */
static wm_rect_t wm_blur_reach(const wm_rect_t* shape) {

    const wm_rect_t r = {shape->x - WM_BLUR_RADIUS, shape->y - WM_BLUR_RADIUS, shape->width + 2 * WM_BLUR_RADIUS, shape->height + 2 * WM_BLUR_RADIUS};

    return ui_rect_clip(&r, wm.display.width, wm.display.height);
}


static wm_rect_t wm_blur_grow(const wm_rect_t* rect, int by) {
    return (wm_rect_t){rect->x - by, rect->y - by, rect->width + 2 * by, rect->height + 2 * by};
}


static bool wm_blur_holds(const ui_damage_t* damage, const wm_rect_t* rect) {

    for (size_t i = 0; i < damage->count; i++) {

        if (ui_rect_contains(&damage->rects[i], rect)) {
            return true;
        }
    }

    return false;
}


static int wm_blur_glass_z(void) {

    int windows = 0;

    for (wm_window_t* win = wm.windows; win; win = win->next) {
        windows++;
    }

    return windows;
}


static void wm_blur_area_add(wm_blur_area_t* areas, size_t max, size_t* count, wm_rect_t shape, wm_blur_t* blur, int z) {

    if (*count >= max) {
        return;
    }

    size_t at = *count;

    while (at > 0 && areas[at - 1].z > z) {
        areas[at] = areas[at - 1];
        at--;
    }

    areas[at] = (wm_blur_area_t){shape, wm_blur_reach(&shape), blur, z};

    (*count)++;
}


/**
 * @brief Collects the shapes frosted this frame: a client's menu and every frosted window.
 *
 * @param areas Receives what each of them reads back, in the order they are read back in.
 * @param max How many fit.
 * @return How many there are.
 */
static size_t wm_blur_areas(wm_blur_area_t* areas, size_t max) {

    size_t count      = 0;
    const int windows = wm_blur_glass_z();

    const int glass_z  = windows;
    const int menu_z   = windows + 1;
    const int drawer_z = windows + 2;


    int z = windows;

    for (wm_window_t* win = wm.windows; win; win = win->next) {

        z--;

        if (wm_window_frosted(win)) {
            wm_blur_area_add(areas, max, &count, wm_window_frame(win), &win->backdrop, wm_window_drawer(win) ? drawer_z : z);
        }
    }

    wm_rect_t glass[WM_TASKBAR_GLASS_MAX];
    wm_blur_t* backdrops[WM_TASKBAR_GLASS_MAX];

    const size_t panes = wm_taskbar_glass_shapes(glass, backdrops, WM_TASKBAR_GLASS_MAX);

    for (size_t i = 0; i < panes; i++) {
        wm_blur_area_add(areas, max, &count, glass[i], backdrops[i], glass_z);
    }

    wm_rect_t shape;

    if (wm_menu_frosted(&shape)) {
        wm_blur_area_add(areas, max, &count, shape, &wm.menu.backdrop, menu_z);
    }

    return count;
}


static bool wm_blur_outdated(const wm_blur_t* blur, const wm_rect_t* reach) {

    if (blur->stale || blur->dirty.count || blur->seen + 1 != wm_blur_frame || !blur->surface) {
        return true;
    }

    return blur->rect.x != reach->x || blur->rect.y != reach->y || cairo_image_surface_get_width(blur->surface) != (reach->width + 1) / 2 ||
           cairo_image_surface_get_height(blur->surface) != (reach->height + 1) / 2;
}


/**
 * @brief Works out, for every frosted backdrop, which part of what it reads back this frame's damage changed, and widens
 * the damage just enough for that part to be read back clean: so that no backdrop read back holds what the last frame
 * painted over it, and none is blurred again where nothing under it moved.
 *
 * Backdrops are visited in the order they are read back, so that a change one frosted thing makes to what it shows is
 * seen by every one read back after it, and by none before.
 */
void wm_blur_damage(void) {

    static wm_blur_area_t areas[WM_BLUR_AREAS_MAX];
    static wm_blur_change_t changes[2 * UI_DAMAGE_MAX + WM_BLUR_AREAS_MAX * (UI_DAMAGE_MAX + 1)];

    const size_t count = wm_blur_areas(areas, WM_BLUR_AREAS_MAX);

    wm_blur_frame++;


    size_t total = 0;

    for (size_t i = 0; i < wm.damage.count; i++) {
        changes[total++] = (wm_blur_change_t){wm.damage.rects[i], INT32_MIN};
    }

    for (size_t i = 0; i < wm.glass_damage.count; i++) {
        changes[total++] = (wm_blur_change_t){wm.glass_damage.rects[i], wm_blur_glass_z()};
    }

    for (size_t i = 0; i < count; i++) {

        wm_blur_t* blur = areas[i].blur;

        blur->stale = wm_blur_outdated(blur, &areas[i].reach);
        blur->seen  = wm_blur_frame;

        ui_damage_reset(&blur->dirty);

        if (blur->stale) {
            changes[total++] = (wm_blur_change_t){areas[i].shape, areas[i].z};
        }
    }


    for (size_t i = 0; i < count; i++) {

        wm_blur_t* blur = areas[i].blur;

        if (blur->stale) {
            wm_damage(&areas[i].reach);
            continue;
        }

        for (size_t j = 0; j < total; j++) {

            if (changes[j].z >= areas[i].z) {
                continue;
            }

            const wm_rect_t spread = wm_blur_grow(&changes[j].rect, WM_BLUR_SUPPORT);
            const wm_rect_t dirty  = ui_rect_intersect(&spread, &areas[i].reach);

            if (!ui_rect_empty(&dirty) && !wm_blur_holds(&blur->dirty, &dirty)) {
                ui_damage_add(&blur->dirty, dirty);
            }
        }

        for (size_t j = 0; j < blur->dirty.count; j++) {

            const wm_rect_t shown = ui_rect_intersect(&blur->dirty.rects[j], &areas[i].shape);

            if (!ui_rect_empty(&shown)) {
                changes[total++] = (wm_blur_change_t){shown, areas[i].z};
            }

            const wm_rect_t read = wm_blur_grow(&blur->dirty.rects[j], WM_BLUR_SUPPORT);
            const wm_rect_t clip = ui_rect_intersect(&read, &areas[i].reach);

            wm_damage(&clip);
        }
    }
}


static uint32_t* wm_blur_scratch_get(size_t pixels) {

    if (pixels > wm_blur_scratch_size) {

        uint32_t* grown = realloc(wm_blur_scratch, pixels * sizeof(uint32_t));

        if (!grown) {
            return NULL;
        }

        wm_blur_scratch      = grown;
        wm_blur_scratch_size = pixels;
    }

    return wm_blur_scratch;
}


static inline uint32_t wm_blur_pack(int r, int g, int b, uint32_t inv) {
    return 0xFF000000u | ((uint32_t)r * inv >> 20) << 16 | ((uint32_t)g * inv >> 20) << 8 | ((uint32_t)b * inv >> 20);
}


/**
 * @brief Runs one box blur along a row of RGB24 pixels, in place, its ends held at the first and last pixel.
 *
 * @param pixels The first pixel.
 * @param length How many pixels the row has.
 * @param radius How far either side of a pixel the box reaches.
 * @param line A scratch line of at least `length` pixels.
 */
static void wm_blur_row(uint32_t* pixels, int length, int radius, uint32_t* line) {

    const uint32_t inv = ((1u << 20) + (uint32_t)(2 * radius)) / (uint32_t)(2 * radius + 1);

    memcpy(line, pixels, sizeof(uint32_t) * (size_t)length);

    int r = 0;
    int g = 0;
    int b = 0;

    for (int i = -radius; i <= radius; i++) {

        const uint32_t p = line[WM_CLAMP(i, 0, length - 1)];

        r += (int)((p >> 16) & 0xFF);
        g += (int)((p >> 8) & 0xFF);
        b += (int)(p & 0xFF);
    }

    for (int x = 0; x < length; x++) {

        pixels[x] = wm_blur_pack(r, g, b, inv);

        const uint32_t in  = line[WM_MIN(x + radius + 1, length - 1)];
        const uint32_t out = line[WM_MAX(x - radius, 0)];

        r += (int)((in >> 16) & 0xFF) - (int)((out >> 16) & 0xFF);
        g += (int)((in >> 8) & 0xFF) - (int)((out >> 8) & 0xFF);
        b += (int)(in & 0xFF) - (int)(out & 0xFF);
    }
}


static void wm_blur_columns(uint32_t* data, size_t pitch, int width, int height, int radius, uint32_t* copy, int* sums) {

    const uint32_t inv = ((1u << 20) + (uint32_t)(2 * radius)) / (uint32_t)(2 * radius + 1);

    int* sr = sums;
    int* sg = sums + width;
    int* sb = sums + 2 * width;

    for (int y = 0; y < height; y++) {
        memcpy(copy + (size_t)y * width, data + (size_t)y * pitch, sizeof(uint32_t) * (size_t)width);
    }

    memset(sums, 0, sizeof(int) * 3 * (size_t)width);

    for (int i = -radius; i <= radius; i++) {

        const uint32_t* row = copy + (size_t)WM_CLAMP(i, 0, height - 1) * width;

        for (int x = 0; x < width; x++) {
            sr[x] += (int)((row[x] >> 16) & 0xFF);
            sg[x] += (int)((row[x] >> 8) & 0xFF);
            sb[x] += (int)(row[x] & 0xFF);
        }
    }

    for (int y = 0; y < height; y++) {

        uint32_t* dst       = data + (size_t)y * pitch;
        const uint32_t* in  = copy + (size_t)WM_MIN(y + radius + 1, height - 1) * width;
        const uint32_t* out = copy + (size_t)WM_MAX(y - radius, 0) * width;

        for (int x = 0; x < width; x++) {

            dst[x] = wm_blur_pack(sr[x], sg[x], sb[x], inv);

            sr[x] += (int)((in[x] >> 16) & 0xFF) - (int)((out[x] >> 16) & 0xFF);
            sg[x] += (int)((in[x] >> 8) & 0xFF) - (int)((out[x] >> 8) & 0xFF);
            sb[x] += (int)(in[x] & 0xFF) - (int)(out[x] & 0xFF);
        }
    }
}


static bool wm_blur_read(cairo_surface_t* target, const wm_rect_t* reach, int u, int v) {

    cairo_t* cr = cairo_create(target);

    cairo_scale(cr, 0.5, 0.5);
    cairo_set_source_surface(cr, wm.display.back, -(reach->x + 2 * u), -(reach->y + 2 * v));
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_BILINEAR);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint(cr);
    cairo_destroy(cr);

    cairo_surface_flush(target);

    uint32_t* data     = (uint32_t*)cairo_image_surface_get_data(target);
    const size_t pitch = (size_t)cairo_image_surface_get_stride(target) / sizeof(uint32_t);
    const int width    = cairo_image_surface_get_width(target);
    const int height   = cairo_image_surface_get_height(target);

    uint32_t* scratch = wm_blur_scratch_get((size_t)width * height + 4 * (size_t)WM_MAX(width, height));

    if (!scratch) {
        return false;
    }

    uint32_t* copy = scratch;
    uint32_t* line = scratch + (size_t)width * height;
    int* sums      = (int*)(line + WM_MAX(width, height));

    for (int pass = 0; pass < WM_BLUR_PASSES; pass++) {

        for (int y = 0; y < height; y++) {
            wm_blur_row(data + y * pitch, width, WM_BLUR_BOX, line);
        }

        wm_blur_columns(data, pitch, width, height, WM_BLUR_BOX, copy, sums);
    }

    cairo_surface_mark_dirty(target);

    return true;
}


static void wm_blur_refresh(wm_blur_t* blur, const wm_rect_t* dirty) {

    const int width  = cairo_image_surface_get_width(blur->surface);
    const int height = cairo_image_surface_get_height(blur->surface);
    const int margin = (WM_BLUR_SUPPORT + 1) / 2;

    const int u0 = WM_CLAMP((dirty->x - blur->rect.x) / 2, 0, width);
    const int v0 = WM_CLAMP((dirty->y - blur->rect.y) / 2, 0, height);
    const int u1 = WM_CLAMP((dirty->x + dirty->width - blur->rect.x + 1) / 2, 0, width);
    const int v1 = WM_CLAMP((dirty->y + dirty->height - blur->rect.y + 1) / 2, 0, height);

    if (u1 <= u0 || v1 <= v0) {
        return;
    }

    const int a0 = WM_MAX(u0 - margin, 0);
    const int b0 = WM_MAX(v0 - margin, 0);
    const int a1 = WM_MIN(u1 + margin, width);
    const int b1 = WM_MIN(v1 + margin, height);


    cairo_surface_t* part = cairo_image_surface_create(CAIRO_FORMAT_RGB24, a1 - a0, b1 - b0);

    if (cairo_surface_status(part) != CAIRO_STATUS_SUCCESS || !wm_blur_read(part, &blur->rect, a0, b0)) {
        cairo_surface_destroy(part);
        return;
    }


    cairo_surface_flush(blur->surface);

    const uint32_t* src  = (const uint32_t*)cairo_image_surface_get_data(part);
    const size_t spitch  = (size_t)cairo_image_surface_get_stride(part) / sizeof(uint32_t);
    uint32_t* dst        = (uint32_t*)cairo_image_surface_get_data(blur->surface);
    const size_t dpitch  = (size_t)cairo_image_surface_get_stride(blur->surface) / sizeof(uint32_t);

    for (int v = v0; v < v1; v++) {
        memcpy(dst + v * dpitch + u0, src + (v - b0) * spitch + (u0 - a0), sizeof(uint32_t) * (size_t)(u1 - u0));
    }

    cairo_surface_mark_dirty(blur->surface);
    cairo_surface_destroy(part);
}


/**
 * @brief Reads back what this frame has drawn under a shape, at half size, and blurs it.
 *
 * @param blur Receives the blurred picture and where it goes, keeping its surface from one frame to the next.
 * @param shape The shape to be frosted.
 * @return false when there is nothing to read, or no memory to blur it in.
 */
bool wm_blur_capture(wm_blur_t* blur, const wm_rect_t* shape) {

    const wm_rect_t r = wm_blur_reach(shape);

    const int width  = (r.width + 1) / 2;
    const int height = (r.height + 1) / 2;

    if (width <= 0 || height <= 0) {
        return false;
    }

    const bool fits = blur->surface && cairo_image_surface_get_width(blur->surface) == width && cairo_image_surface_get_height(blur->surface) == height;

    if (fits && !blur->stale && blur->rect.x == r.x && blur->rect.y == r.y) {

        if (blur->dirty.count) {

            cairo_surface_flush(wm.display.back);

            for (size_t i = 0; i < blur->dirty.count; i++) {
                wm_blur_refresh(blur, &blur->dirty.rects[i]);
            }

            ui_damage_reset(&blur->dirty);
        }

        return true;
    }

    if (!fits) {

        wm_blur_fini(blur);

        if (!(blur->surface = wm_surface_create(CAIRO_FORMAT_RGB24, width, height))) {
            return false;
        }
    }

    blur->rect = r;

    cairo_surface_flush(wm.display.back);

    if (!wm_blur_read(blur->surface, &r, 0, 0)) {
        return false;
    }

    blur->stale = false;

    ui_damage_reset(&blur->dirty);

    return true;
}


/**
 * @brief Paints a blurred backdrop back where it was read from, inside the current clip.
 *
 * @param cr The cairo context, clipped to the shape being frosted.
 * @param blur The backdrop, from wm_blur_capture().
 */
void wm_blur_paint(cairo_t* cr, const wm_blur_t* blur) {

    if (!blur->surface) {
        return;
    }

    cairo_save(cr);

    cairo_translate(cr, blur->rect.x, blur->rect.y);
    cairo_scale(cr, 2.0, 2.0);
    cairo_set_source_surface(cr, blur->surface, 0, 0);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_BILINEAR);
    cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_PAD);
    cairo_paint(cr);

    cairo_restore(cr);
}


/**
 * @brief Lets go of a backdrop's picture.
 *
 * @param blur The backdrop.
 */
void wm_blur_fini(wm_blur_t* blur) {

    if (blur->surface) {
        cairo_surface_destroy(blur->surface);
        blur->surface = NULL;
    }
}
