/*
 * GPL3 License
 *
 * Author(s):
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 *
 * Copyright (c) 2013-2019 Antonino Natale
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

#include <aplus.h>
#include <aplus/debug.h>
#include <stdint.h>

#define __WITH_KINDS
#include "__ubsan.h"


__noreturn __nosanitize("undefined") void __ubsan_handle_type_mismatch_v1(struct type_mismatch_data* data, void* ptr) {

#if DEBUG_LEVEL_TRACE
    kprintf("ubsan: caught " __FILE__ " exception!\n");
#endif

    DEBUG_ASSERT(data);
    DEBUG_ASSERT(data->location.file);
    DEBUG_ASSERT(data->type);
    DEBUG_ASSERT(data->type->name);


    if (ptr == NULL) {

        kpanicf("PANIC! UBSAN: null pointer access on object of type %s on %s:%d:%d\n", data->type->name, data->location.file, data->location.line, data->location.column);

    } else if (data->alignment != 0 && ((uintptr_t)ptr & ((1L << data->alignment) - 1L))) {

        kpanicf("WARN! UBSAN: type mismatch: misaligned %s on address %p with align 2^%d for object of type %s on %s:%d:%d\n", __kinds[data->type_check_kind], ptr, data->alignment, data->type->name, data->location.file, data->location.line,
                data->location.column);

    } else {

        DEBUG_ASSERT(ptr);
        DEBUG_ASSERT(data->type_check_kind < sizeof(__kinds) / sizeof(__kinds[0]));

        kpanicf("PANIC! UBSAN: type mismatch: %s address %p with insufficent space for object of type %s on %s:%d:%d\n", __kinds[data->type_check_kind], ptr, data->type->name, data->location.file, data->location.line,
                data->location.column);
    }
}
