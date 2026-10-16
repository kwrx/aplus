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
 * @brief Soft shadows: a blurred rounded rectangle rendered once, and stretched around a frame of any size.
 */

#include <stdlib.h>
#include <string.h>

#include <wm.h>


/**
 * @brief How many box passes approximate the gaussian, each a third as wide as the whole blur.
 */
#define WM_SHADOW_PASSES 3


/**
 * @brief One stretch of a frame's edge along one axis, and the piece of the tile that covers it.
 */
typedef struct {

    int dst;
    int len;

    int piece;
    int anchor;

} wm_slice_t;


/**
 * @brief Runs one box blur along every row of an A8 image, in place.
 *
 * @param data The pixels.
 * @param stride The bytes between two rows.
 * @param width The width in pixels.
 * @param height The height in pixels.
 * @param radius How far either side of a pixel the box reaches.
 * @param line A scratch row of at least `width` bytes.
 */
static void wm_shadow_blur_rows(uint8_t* data, int stride, int width, int height, int radius, uint8_t* line) {

    const int span = 2 * radius + 1;

    for (int y = 0; y < height; y++) {

        uint8_t* row = data + (size_t)y * stride;

        memcpy(line, row, (size_t)width);

        int sum = 0;

        for (int i = 0; i <= radius && i < width; i++) {
            sum += line[i];
        }

        for (int x = 0; x < width; x++) {

            row[x] = (uint8_t)(sum / span);

            if (x + radius + 1 < width) {
                sum += line[x + radius + 1];
            }

            if (x - radius >= 0) {
                sum -= line[x - radius];
            }
        }
    }
}


/**
 * @brief Runs one box blur down every column of an A8 image, in place.
 *
 * @param data The pixels.
 * @param stride The bytes between two rows.
 * @param width The width in pixels.
 * @param height The height in pixels.
 * @param radius How far either side of a pixel the box reaches.
 * @param line A scratch column of at least `height` bytes.
 */
static void wm_shadow_blur_cols(uint8_t* data, int stride, int width, int height, int radius, uint8_t* line) {

    const int span = 2 * radius + 1;

    for (int x = 0; x < width; x++) {

        for (int y = 0; y < height; y++) {
            line[y] = data[(size_t)y * stride + x];
        }

        int sum = 0;

        for (int i = 0; i <= radius && i < height; i++) {
            sum += line[i];
        }

        for (int y = 0; y < height; y++) {

            data[(size_t)y * stride + x] = (uint8_t)(sum / span);

            if (y + radius + 1 < height) {
                sum += line[y + radius + 1];
            }

            if (y - radius >= 0) {
                sum -= line[y - radius];
            }
        }
    }
}


/**
 * @brief Copies a rectangle of the tile into a surface of its own.
 *
 * @param tile The blurred tile.
 * @param x The left edge of the rectangle.
 * @param y The top edge of the rectangle.
 * @param width The width of the rectangle.
 * @param height The height of the rectangle.
 * @return The piece, or NULL.
 */
static cairo_surface_t* wm_shadow_cut(cairo_surface_t* tile, int x, int y, int width, int height) {

    cairo_surface_t* piece = wm_surface_create(CAIRO_FORMAT_A8, width, height);

    if (!piece) {
        return NULL;
    }


    cairo_t* cr = cairo_create(piece);

    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_surface(cr, tile, -x, -y);
    cairo_paint(cr);

    cairo_destroy(cr);

    return piece;
}


/**
 * @brief Renders the blurred tile and cuts it into the pieces a frame is surrounded with.
 *
 * The straight run in the middle of each side is twice the blur long, so the strip cut from
 * its centre is untouched by either rounded corner and can be stretched along any edge.
 *
 * @param shadow The shadow to fill in.
 * @param blur How far the shadow spreads past the edge it is cast by.
 * @param radius The corner radius of that edge.
 * @return 0 on success, or -1 when the surfaces cannot be allocated.
 */
int wm_shadow_init(wm_shadow_t* shadow, int blur, int radius) {

    memset(shadow, 0, sizeof(*shadow));

    const int corner = 2 * blur + radius;
    const int size   = 2 * corner + 1;

    shadow->blur   = blur;
    shadow->corner = corner;


    cairo_surface_t* tile = wm_surface_create(CAIRO_FORMAT_A8, size, size);

    if (!tile) {
        return -1;
    }


    cairo_t* cr = cairo_create(tile);

    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0);
    ui_draw_rounded_rect_d(cr, blur, blur, size - 2 * blur, size - 2 * blur, radius);
    cairo_fill(cr);

    cairo_destroy(cr);

    cairo_surface_flush(tile);


    uint8_t* line = (uint8_t*)malloc((size_t)size);

    if (!line) {
        cairo_surface_destroy(tile);
        return -1;
    }


    uint8_t* data    = cairo_image_surface_get_data(tile);
    const int stride = cairo_image_surface_get_stride(tile);
    const int box    = blur / WM_SHADOW_PASSES > 0 ? blur / WM_SHADOW_PASSES : 1;

    for (int i = 0; i < WM_SHADOW_PASSES; i++) {
        wm_shadow_blur_rows(data, stride, size, size, box, line);
        wm_shadow_blur_cols(data, stride, size, size, box, line);
    }

    free(line);

    cairo_surface_mark_dirty(tile);


    const int origin[3] = {0, corner, corner + 1};
    const int extent[3] = {corner, 1, corner};

    for (int row = 0; row < 3; row++) {

        for (int col = 0; col < 3; col++) {

            if (row == 1 && col == 1) {
                continue;
            }

            if (!(shadow->pieces[row][col] = wm_shadow_cut(tile, origin[col], origin[row], extent[col], extent[row]))) {
                cairo_surface_destroy(tile);
                wm_shadow_fini(shadow);
                return -1;
            }
        }
    }

    cairo_surface_destroy(tile);

    return 0;
}


/**
 * @brief Releases the pieces a shadow was cut into.
 *
 * @param shadow The shadow.
 */
void wm_shadow_fini(wm_shadow_t* shadow) {

    for (int row = 0; row < 3; row++) {

        for (int col = 0; col < 3; col++) {

            if (shadow->pieces[row][col]) {
                cairo_surface_destroy(shadow->pieces[row][col]);
                shadow->pieces[row][col] = NULL;
            }
        }
    }
}


/**
 * @brief Splits one axis of the shadow into the stretches each piece of the tile covers.
 *
 * A side too short for both corners and a strip gets the outer half of each corner instead.
 *
 * @param origin Where the shadow starts on this axis.
 * @param length How long it is on this axis.
 * @param corner How long a corner piece is.
 * @param out Receives the stretches.
 * @return How many stretches there are, 2 or 3.
 */
static int wm_shadow_slices(int origin, int length, int corner, wm_slice_t out[3]) {

    if (length >= 2 * corner + 1) {

        out[0] = (wm_slice_t){origin, corner, 0, origin};
        out[1] = (wm_slice_t){origin + corner, length - 2 * corner, 1, origin + corner};
        out[2] = (wm_slice_t){origin + length - corner, corner, 2, origin + length - corner};

        return 3;
    }


    const int head = length / 2;

    out[0] = (wm_slice_t){origin, head, 0, origin};
    out[1] = (wm_slice_t){origin + head, length - head, 2, origin + length - corner};

    return 2;
}


/**
 * @brief Paints a shadow cast by a frame, in black at a given strength.
 *
 * @param cr The cairo context to draw with.
 * @param shadow The shadow to paint.
 * @param frame The frame casting it.
 * @param offset How far down the shadow falls.
 * @param alpha How dark it is at its darkest.
 */
void wm_shadow_paint(cairo_t* cr, const wm_shadow_t* shadow, const wm_rect_t* frame, int offset, double alpha) {

    if (alpha <= 0.0 || !shadow->pieces[0][0]) {
        return;
    }


    wm_slice_t cols[3];
    wm_slice_t rows[3];

    const int ncols = wm_shadow_slices(frame->x - shadow->blur, frame->width + 2 * shadow->blur, shadow->corner, cols);
    const int nrows = wm_shadow_slices(frame->y - shadow->blur + offset, frame->height + 2 * shadow->blur, shadow->corner, rows);


    cairo_save(cr);

    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, alpha);

    for (int r = 0; r < nrows; r++) {

        for (int c = 0; c < ncols; c++) {

            if (rows[r].piece == 1 && cols[c].piece == 1) {
                continue;
            }

            if (rows[r].len <= 0 || cols[c].len <= 0) {
                continue;
            }


            cairo_pattern_t* pattern = cairo_pattern_create_for_surface(shadow->pieces[rows[r].piece][cols[c].piece]);

            cairo_matrix_t matrix;

            cairo_matrix_init_translate(&matrix, -cols[c].anchor, -rows[r].anchor);
            cairo_pattern_set_matrix(pattern, &matrix);
            cairo_pattern_set_extend(pattern, CAIRO_EXTEND_PAD);
            cairo_pattern_set_filter(pattern, CAIRO_FILTER_BILINEAR);

            cairo_save(cr);

            cairo_rectangle(cr, cols[c].dst, rows[r].dst, cols[c].len, rows[r].len);
            cairo_clip(cr);

            cairo_mask(cr, pattern);

            cairo_restore(cr);

            cairo_pattern_destroy(pattern);
        }
    }

    cairo_restore(cr);
}
