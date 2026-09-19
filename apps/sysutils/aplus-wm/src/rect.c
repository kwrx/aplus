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
 * @brief Rectangle arithmetic, shared by hit testing, damage tracking and compositing.
 *
 * The merging the damage set does is libui's, in ui_damage_add(); what is left here is the
 * handful of tests every module would otherwise write out by hand.
 */

#include <wm.h>


/**
 * @brief Reports whether a point falls inside a rectangle.
 *
 * @param rect The rectangle to test against.
 * @param x The point.
 * @param y The point.
 * @return true when the point is inside.
 */
bool wm_rect_contains_point(const wm_rect_t* rect, int x, int y) {

    return x >= rect->x && y >= rect->y && x < rect->x + rect->width && y < rect->y + rect->height;
}


/**
 * @brief Reports whether one rectangle covers the whole of another.
 *
 * @param outer The rectangle that would do the covering.
 * @param inner The rectangle that would be covered.
 * @return true when nothing of the inner one sticks out.
 */
bool wm_rect_contains(const wm_rect_t* outer, const wm_rect_t* inner) {

    return inner->x >= outer->x && inner->y >= outer->y && inner->x + inner->width <= outer->x + outer->width && inner->y + inner->height <= outer->y + outer->height;
}


/**
 * @brief Reports whether two rectangles share any area at all.
 *
 * @param a The first rectangle.
 * @param b The second rectangle.
 * @return true when they overlap.
 */
bool wm_rect_intersects(const wm_rect_t* a, const wm_rect_t* b) {

    return a->x < b->x + b->width && b->x < a->x + a->width && a->y < b->y + b->height && b->y < a->y + a->height;
}


/**
 * @brief Cuts a rectangle down to a size, which is what keeps damage inside the screen.
 *
 * @param rect The rectangle to clip.
 * @param width The width to clip to.
 * @param height The height to clip to.
 * @return The part of the rectangle inside it, empty when there is none.
 */
wm_rect_t wm_rect_clip(const wm_rect_t* rect, int width, int height) {

    const int x0 = WM_CLAMP(rect->x, 0, width);
    const int y0 = WM_CLAMP(rect->y, 0, height);
    const int x1 = WM_CLAMP(rect->x + rect->width, 0, width);
    const int y1 = WM_CLAMP(rect->y + rect->height, 0, height);

    wm_rect_t r = {x0, y0, x1 - x0, y1 - y0};

    return r;
}
