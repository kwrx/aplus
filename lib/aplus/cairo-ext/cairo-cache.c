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


#ifndef NO_CAIRO_EXTENSION

    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include <sys/stat.h>


    #include <cairo/cairo.h>

    #include <aplus/base.h>
    #include <aplus/cairo-ext/cairo-cache.h>
    #include <aplus/utils/hashmap.h>

hashmap_t hm_cache = NULL;

    #define __init()                      \
        {                                 \
            if (!hm_cache)                \
                hm_cache = hashmap_new(); \
        }


cairo_surface_t *cairo_cache_obtain_resource(const char *path) {
    __init();

    cairo_surface_t *R;
    if (hashmap_get(hm_cache, (char *)path, (any_t *)&R) != HM_OK)
        return NULL;

    void *out = (void *)__libaplus_malloc(cairo_image_surface_get_stride(R) * cairo_image_surface_get_height(R));

    if (!out)
        return NULL;

    memcpy(out, cairo_image_surface_get_data(R), cairo_image_surface_get_stride(R) * cairo_image_surface_get_height(R));


    return cairo_image_surface_create_for_data(out, cairo_image_surface_get_format(R), cairo_image_surface_get_width(R), cairo_image_surface_get_height(R), cairo_image_surface_get_stride(R));
}


void cairo_cache_put_resource(const char *path, cairo_surface_t *surface) {
    __init();

    hashmap_put(hm_cache, (char *)path, (any_t)cairo_surface_reference(surface));
}

#endif
