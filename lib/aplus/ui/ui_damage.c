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
 * @brief A handful of damaged rectangles rather than the one box around them all.
 *
 * A window that changes a character in one corner and a button in the other has no business
 * repainting everything between the two, and the box around them is exactly that.
 */

#include <stdint.h>

#include "ui_internal.h"


static int64_t ui_rect_area(const ui_rect_t* r) {

    return (int64_t)r->width * (int64_t)r->height;
}


static ui_rect_t ui_rect_union(const ui_rect_t* a, const ui_rect_t* b) {

    const int x0 = a->x < b->x ? a->x : b->x;
    const int y0 = a->y < b->y ? a->y : b->y;

    const int ax1 = a->x + a->width;
    const int ay1 = a->y + a->height;
    const int bx1 = b->x + b->width;
    const int by1 = b->y + b->height;

    const int x1 = ax1 > bx1 ? ax1 : bx1;
    const int y1 = ay1 > by1 ? ay1 : by1;

    ui_rect_t r = {x0, y0, x1 - x0, y1 - y0};

    return r;
}


static int64_t ui_rect_overlap(const ui_rect_t* a, const ui_rect_t* b) {

    const int x0 = a->x > b->x ? a->x : b->x;
    const int y0 = a->y > b->y ? a->y : b->y;

    const int ax1 = a->x + a->width;
    const int ay1 = a->y + a->height;
    const int bx1 = b->x + b->width;
    const int by1 = b->y + b->height;

    const int x1 = ax1 < bx1 ? ax1 : bx1;
    const int y1 = ay1 < by1 ? ay1 : by1;

    if (x0 >= x1 || y0 >= y1) {
        return 0;
    }

    return (int64_t)(x1 - x0) * (int64_t)(y1 - y0);
}


/**
 * @brief Reports what merging two rectangles would cost, as the area neither of them covers today.
 *
 * @param a The first rectangle.
 * @param b The second rectangle.
 * @return The area the merged rectangle would repaint for nothing; zero when one contains the other.
 */
static int64_t ui_rect_merge_cost(const ui_rect_t* a, const ui_rect_t* b) {

    const ui_rect_t u = ui_rect_union(a, b);

    return ui_rect_area(&u) - (ui_rect_area(a) + ui_rect_area(b) - ui_rect_overlap(a, b));
}


void ui_damage_reset(ui_damage_t* damage) {

    damage->count = 0;
}


/**
 * @brief Adds a rectangle to the set, merging it into one already there wherever that is cheaper.
 *
 * Two rectangles are worth merging exactly when their union is no larger than the two of them
 * added up, since whatever they overlap on would otherwise be painted twice.
 *
 * @param damage The set to add to.
 * @param rect The rectangle that has to be repainted; an empty one is ignored.
 */
void ui_damage_add(ui_damage_t* damage, ui_rect_t rect) {

    if (rect.width <= 0 || rect.height <= 0) {
        return;
    }


    for (size_t i = 0; i < damage->count;) {

        if (ui_rect_merge_cost(&damage->rects[i], &rect) > ui_rect_overlap(&damage->rects[i], &rect)) {
            i++;
            continue;
        }

        rect = ui_rect_union(&damage->rects[i], &rect);

        damage->rects[i] = damage->rects[--damage->count];

        i = 0;
    }

    if (damage->count < UI_DAMAGE_MAX) {

        damage->rects[damage->count++] = rect;

        return;
    }


    ui_rect_t pool[UI_DAMAGE_MAX + 1];

    for (size_t i = 0; i < UI_DAMAGE_MAX; i++) {
        pool[i] = damage->rects[i];
    }

    pool[UI_DAMAGE_MAX] = rect;


    size_t best_a = 0;
    size_t best_b = 1;
    int64_t best  = ui_rect_merge_cost(&pool[0], &pool[1]);

    for (size_t i = 0; i < UI_DAMAGE_MAX + 1; i++) {

        for (size_t j = i + 1; j < UI_DAMAGE_MAX + 1; j++) {

            const int64_t cost = ui_rect_merge_cost(&pool[i], &pool[j]);

            if (cost < best) {

                best   = cost;
                best_a = i;
                best_b = j;
            }
        }
    }

    pool[best_a] = ui_rect_union(&pool[best_a], &pool[best_b]);
    pool[best_b] = pool[UI_DAMAGE_MAX];

    for (size_t i = 0; i < UI_DAMAGE_MAX; i++) {
        damage->rects[i] = pool[i];
    }
}


/**
 * @brief Clips every rectangle in the set to a size, dropping the ones left with nothing in them.
 *
 * @param damage The set to clip.
 * @param width The width to clip to.
 * @param height The height to clip to.
 */
void ui_damage_clip(ui_damage_t* damage, int width, int height) {

    size_t kept = 0;

    for (size_t i = 0; i < damage->count; i++) {

        int x0 = damage->rects[i].x < 0 ? 0 : damage->rects[i].x;
        int y0 = damage->rects[i].y < 0 ? 0 : damage->rects[i].y;

        int x1 = damage->rects[i].x + damage->rects[i].width;
        int y1 = damage->rects[i].y + damage->rects[i].height;

        if (x1 > width) {
            x1 = width;
        }

        if (y1 > height) {
            y1 = height;
        }

        if (x0 >= x1 || y0 >= y1) {
            continue;
        }


        ui_rect_t r = {x0, y0, x1 - x0, y1 - y0};

        damage->rects[kept++] = r;
    }

    damage->count = kept;
}
