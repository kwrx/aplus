/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2026 Antonino Natale
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

#include <aplus/ui.h>


bool ui_rect_empty(const ui_rect_t* rect) {
    return rect->width <= 0 || rect->height <= 0;
}


bool ui_rect_contains_point(const ui_rect_t* rect, int x, int y) {
    return x >= rect->x && y >= rect->y && x < rect->x + rect->width && y < rect->y + rect->height;
}


bool ui_rect_contains(const ui_rect_t* outer, const ui_rect_t* inner) {
    return inner->x >= outer->x && inner->y >= outer->y && inner->x + inner->width <= outer->x + outer->width && inner->y + inner->height <= outer->y + outer->height;
}


bool ui_rect_intersects(const ui_rect_t* a, const ui_rect_t* b) {
    return a->x < b->x + b->width && b->x < a->x + a->width && a->y < b->y + b->height && b->y < a->y + a->height;
}


ui_rect_t ui_rect_intersect(const ui_rect_t* a, const ui_rect_t* b) {

    const int x0 = a->x > b->x ? a->x : b->x;
    const int y0 = a->y > b->y ? a->y : b->y;
    const int x1 = a->x + a->width < b->x + b->width ? a->x + a->width : b->x + b->width;
    const int y1 = a->y + a->height < b->y + b->height ? a->y + a->height : b->y + b->height;

    if (x1 <= x0 || y1 <= y0) {
        return (ui_rect_t){x0, y0, 0, 0};
    }

    return (ui_rect_t){x0, y0, x1 - x0, y1 - y0};
}


ui_rect_t ui_rect_union(const ui_rect_t* a, const ui_rect_t* b) {

    const int x0 = a->x < b->x ? a->x : b->x;
    const int y0 = a->y < b->y ? a->y : b->y;
    const int x1 = a->x + a->width > b->x + b->width ? a->x + a->width : b->x + b->width;
    const int y1 = a->y + a->height > b->y + b->height ? a->y + a->height : b->y + b->height;

    return (ui_rect_t){x0, y0, x1 - x0, y1 - y0};
}


ui_rect_t ui_rect_clip(const ui_rect_t* rect, int width, int height) {

    const ui_rect_t bounds = {0, 0, width, height};

    return ui_rect_intersect(rect, &bounds);
}
