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

#include "__ubsan.h"



__noreturn __nosanitize("undefined") void __ubsan_handle_shift_out_of_bounds(struct shift_out_of_bounds_data* data, uintptr_t lhs, uintptr_t rhs) {

#if DEBUG_LEVEL_TRACE
    kprintf("ubsan: caught " __FILE__ " exception!\n");
#endif

    DEBUG_ASSERT(data);
    DEBUG_ASSERT(data->location.file);
    DEBUG_ASSERT(data->lhs_type);
    DEBUG_ASSERT(data->lhs_type->name);
    DEBUG_ASSERT(data->rhs_type);
    DEBUG_ASSERT(data->rhs_type->name);

    kpanicf("PANIC! UBSAN: shift out of bounds for operation '%ld <<>> %ld' of object types %s and %s on %s:%d:%d\n", lhs, rhs, data->lhs_type->name, data->rhs_type->name, data->location.file, data->location.line, data->location.column);
}
