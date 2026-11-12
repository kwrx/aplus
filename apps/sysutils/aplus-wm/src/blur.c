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

#if defined(__SSE2__)
    #include <emmintrin.h>
#endif

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

    return blur->rect.x != reach->x || blur->rect.y != reach->y || cairo_image_surface_get_width(blur->surface) != (reach->width + 1) / 2 || cairo_image_surface_get_height(blur->surface) != (reach->height + 1) / 2;
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


#define WM_BLUR_SPAN (2 * WM_BLUR_BOX + 1)
#define WM_BLUR_DIV  (65536 / WM_BLUR_SPAN + 1)


static inline uint8_t wm_blur_divide(uint32_t sum) {
    return (uint8_t)((sum * WM_BLUR_DIV) >> 16);
}


#if defined(__SSE2__)

static inline __m128i wm_blur_pack(__m128i lo, __m128i hi) {

    const __m128i div = _mm_set1_epi16(WM_BLUR_DIV);

    return _mm_packus_epi16(_mm_mulhi_epu16(lo, div), _mm_mulhi_epu16(hi, div));
}

#endif


static size_t wm_blur_row_bulk(const uint8_t* line, uint8_t* out, size_t size) {

#if defined(__SSE2__)

    const __m128i zero = _mm_setzero_si128();

    size_t i = 0;

    for (; i + 16 <= size; i += 16) {

        __m128i lo = _mm_setzero_si128();
        __m128i hi = _mm_setzero_si128();

        for (int k = 0; k < WM_BLUR_SPAN; k++) {

            const __m128i v = _mm_loadu_si128((const __m128i*)(line + i + 4 * (size_t)k));

            lo = _mm_add_epi16(lo, _mm_unpacklo_epi8(v, zero));
            hi = _mm_add_epi16(hi, _mm_unpackhi_epi8(v, zero));
        }

        _mm_storeu_si128((__m128i*)(out + i), wm_blur_pack(lo, hi));
    }

    return i;

#else

    (void)line;
    (void)out;
    (void)size;

    return 0;

#endif
}


static void wm_blur_row(uint32_t* pixels, int length, uint8_t* line) {

    const size_t size = 4 * (size_t)length;

    for (int i = 0; i < WM_BLUR_BOX; i++) {
        memcpy(line + 4 * (size_t)i, &pixels[0], 4);
        memcpy(line + 4 * (size_t)(WM_BLUR_BOX + length + i), &pixels[length - 1], 4);
    }

    memcpy(line + 4 * (size_t)WM_BLUR_BOX, pixels, size);

    uint8_t* out = (uint8_t*)pixels;

    for (size_t i = wm_blur_row_bulk(line, out, size); i < size; i++) {

        uint32_t sum = 0;

        for (int k = 0; k < WM_BLUR_SPAN; k++) {
            sum += line[i + 4 * (size_t)k];
        }

        out[i] = wm_blur_divide(sum);
    }
}


static void wm_blur_columns(uint32_t* data, size_t pitch, int width, int height, uint8_t* ring, uint16_t* sums) {

    const int slots   = WM_BLUR_BOX + 1;
    const size_t size = 4 * (size_t)width;

    memset(sums, 0, sizeof(uint16_t) * size);

    for (int k = -WM_BLUR_BOX; k <= WM_BLUR_BOX; k++) {

        const uint8_t* row = (const uint8_t*)(data + (size_t)WM_CLAMP(k, 0, height - 1) * pitch);

        for (size_t i = 0; i < size; i++) {
            sums[i] += row[i];
        }
    }

    for (int y = 0; y < height; y++) {

        uint8_t* dst   = (uint8_t*)(data + (size_t)y * pitch);
        uint8_t* saved = ring + (size_t)(y % slots) * size;

        memcpy(saved, dst, size);

        const int below    = WM_MIN(y + WM_BLUR_BOX + 1, height - 1);
        const uint8_t* in  = below > y ? (const uint8_t*)(data + (size_t)below * pitch) : ring + (size_t)(below % slots) * size;
        const uint8_t* out = ring + (size_t)(WM_MAX(y - WM_BLUR_BOX, 0) % slots) * size;

        size_t i = 0;

#if defined(__SSE2__)

        const __m128i zero = _mm_setzero_si128();

        for (; i + 16 <= size; i += 16) {

            __m128i lo = _mm_loadu_si128((const __m128i*)(sums + i));
            __m128i hi = _mm_loadu_si128((const __m128i*)(sums + i + 8));

            _mm_storeu_si128((__m128i*)(dst + i), wm_blur_pack(lo, hi));

            const __m128i a = _mm_loadu_si128((const __m128i*)(in + i));
            const __m128i b = _mm_loadu_si128((const __m128i*)(out + i));

            lo = _mm_sub_epi16(_mm_add_epi16(lo, _mm_unpacklo_epi8(a, zero)), _mm_unpacklo_epi8(b, zero));
            hi = _mm_sub_epi16(_mm_add_epi16(hi, _mm_unpackhi_epi8(a, zero)), _mm_unpackhi_epi8(b, zero));

            _mm_storeu_si128((__m128i*)(sums + i), lo);
            _mm_storeu_si128((__m128i*)(sums + i + 8), hi);
        }

#endif

        for (; i < size; i++) {
            dst[i]  = wm_blur_divide(sums[i]);
            sums[i] = (uint16_t)(sums[i] + in[i] - out[i]);
        }
    }
}


static void wm_blur_sample(cairo_surface_t* target, const wm_rect_t* reach, int u, int v) {

    cairo_t* cr = cairo_create(target);

    cairo_scale(cr, 0.5, 0.5);
    cairo_set_source_surface(cr, wm.display.back, -(reach->x + 2 * u), -(reach->y + 2 * v));
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_BILINEAR);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint(cr);
    cairo_destroy(cr);

    cairo_surface_flush(target);
}


static bool wm_blur_smooth(cairo_surface_t* target) {

    uint32_t* data     = (uint32_t*)cairo_image_surface_get_data(target);
    const size_t pitch = (size_t)cairo_image_surface_get_stride(target) / sizeof(uint32_t);
    const int width    = cairo_image_surface_get_width(target);
    const int height   = cairo_image_surface_get_height(target);

    uint32_t* scratch = wm_blur_scratch_get(2 * (size_t)width + (size_t)width + 2 * WM_BLUR_BOX + (size_t)(WM_BLUR_BOX + 1) * width);

    if (!scratch) {
        return false;
    }

    uint16_t* sums = (uint16_t*)scratch;
    uint8_t* line  = (uint8_t*)(scratch + 2 * (size_t)width);
    uint8_t* ring  = (uint8_t*)(scratch + 3 * (size_t)width + 2 * WM_BLUR_BOX);

    for (int pass = 0; pass < WM_BLUR_PASSES; pass++) {

        for (int y = 0; y < height; y++) {
            wm_blur_row(data + y * pitch, width, line);
        }

        wm_blur_columns(data, pitch, width, height, ring, sums);
    }

    cairo_surface_mark_dirty(target);

    return true;
}


static bool wm_blur_read(cairo_surface_t* target, const wm_rect_t* reach, int u, int v) {

    wm_blur_sample(target, reach, u, v);

    return wm_blur_smooth(target);
}


static bool wm_blur_unchanged(wm_blur_t* blur, cairo_surface_t* part, const wm_rect_t* rect) {

    const uint8_t* data = cairo_image_surface_get_data(part);
    const size_t size   = (size_t)cairo_image_surface_get_stride(part) * (size_t)cairo_image_surface_get_height(part);

    if (blur->input_size == size && !memcmp(&blur->input_rect, rect, sizeof(*rect)) && !memcmp(blur->input, data, size)) {
        return true;
    }

    blur->input_size = 0;

    if (size > blur->input_capacity) {

        uint8_t* grown = realloc(blur->input, size);

        if (!grown) {
            return false;
        }

        blur->input          = grown;
        blur->input_capacity = size;
    }

    memcpy(blur->input, data, size);

    blur->input_size = size;
    blur->input_rect = *rect;

    return false;
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

    if (cairo_surface_status(part) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(part);
        return;
    }

    wm_blur_sample(part, &blur->rect, a0, b0);

    const wm_rect_t rect = {u0, v0, u1 - u0, v1 - v0};

    if (wm_blur_unchanged(blur, part, &rect)) {
        cairo_surface_destroy(part);
        return;
    }

    if (!wm_blur_smooth(part)) {
        blur->input_size = 0;
        cairo_surface_destroy(part);
        return;
    }


    cairo_surface_flush(blur->surface);

    const uint32_t* src = (const uint32_t*)cairo_image_surface_get_data(part);
    const size_t spitch = (size_t)cairo_image_surface_get_stride(part) / sizeof(uint32_t);
    uint32_t* dst       = (uint32_t*)cairo_image_surface_get_data(blur->surface);
    const size_t dpitch = (size_t)cairo_image_surface_get_stride(blur->surface) / sizeof(uint32_t);

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

    blur->rect       = r;
    blur->input_size = 0;

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

    free(blur->input);

    blur->input          = NULL;
    blur->input_size     = 0;
    blur->input_capacity = 0;
}
