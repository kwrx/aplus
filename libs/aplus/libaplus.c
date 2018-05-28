/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2018 Antonino Natale
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


#include <aplus/base.h>
#include <stdlib.h>
#include <string.h>


void* (*__libaplus_malloc) (size_t) = malloc;
void* (*__libaplus_calloc) (size_t, size_t) = calloc;
void (*__libaplus_free) (void*) = free;


int libaplus_init(void* (*mallocfp) (size_t), void* (*callocfp) (size_t, size_t), void (*freefp) (void*)) {
    __libaplus_malloc = mallocfp;
    __libaplus_calloc = callocfp;
    __libaplus_free = freefp;

    return 0;
}