/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2019 Antonino Natale
 * 
 * 
 * This file is part of aPlus.
 * 
 * aPlus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * aPlus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with aPlus.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#include <sys/config.h>
#include <reent.h>



#define HAVE_MORECORE                       1
#define HAVE_MMAP                           0
#define HAVE_MREMAP                         0

#define USE_BUILTIN_FFS                     0

#define MORECORE_CANNOT_TRIM                1
#define ABORT_ON_ASSERT_FAILURE             1
#define NO_MALLOC_STATS                     0
#define REALLOC_ZERO_BYTES_FREES            1
#define MMAP_CLEARS                         0
#define MALLOC_ALIGNMENT                    16


#define malloc_getpagesize                  \
    (4096L) /* FIXME */

#define DLMALLOC_EXPORT                     \
    __attribute__((weak))

#define MORECORE(size)                      \
    _sbrk_r(_REENT, (size))

#define POINTER_UINT                        \
    unsigned _POINTER_INT

#define MALLOC_LOCK                         \
    __malloc_lock(_REENT)

#define MALLOC_UNLOCK                       \
    __malloc_unlock(_REENT)


extern void __malloc_lock(struct _reent *);
extern void __malloc_unlock(struct _reent *);



#include "dlmalloc.h"



void* _calloc_r(struct _reent* reent, size_t a, size_t b) {
    return calloc(a, b);
}

void* _malloc_r(struct _reent* reent, size_t s) {
    return malloc(s);
}

void* _realloc_r(struct _reent* reent, void* p, size_t s) {
    return realloc(p, s);
}

void _free_r(struct _reent* reent, void* p) {
    return free(p);
}