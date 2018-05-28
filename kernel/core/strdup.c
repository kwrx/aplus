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


#include <aplus.h>
#include <aplus/mm.h>
#include <libc.h>

char* strdup(const char* s) {
    char* p = (char*) kmalloc(strlen(s) + 1, GFP_KERNEL);
    strcpy(p, s);

    return p;
}

char* strndup(const char* s, size_t n) {

    size_t k;
    if(strlen(s) > n)
        k = n;
    else
        k = strlen(s);

    char* p = (char*) kmalloc(k + 1, GFP_KERNEL);
    strncpy(p, s, k);

    return p;
}

EXPORT(strdup);
EXPORT(strndup);
