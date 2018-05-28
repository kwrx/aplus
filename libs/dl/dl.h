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


#pragma once

#include <aplus/base.h>
#include <aplus/utils/list.h>

typedef struct {
    char filename[BUFSIZ];
    int flags;
} dl_t;

extern int __dlerrno;
extern list(dl_t*, __dl_loaded);

#define DL_SUCCESS                          0
#define DL_ERR_CANNOT_LOAD_LIBRARY          1
#define DL_ERR_INVALID_LIBRARY_HANDLE       2
#define DL_ERR_BAD_SYMBOL_NAME              3
#define DL_ERR_SYMBOL_NOT_FOUND             4
#define DL_ERR_SYMBOL_NOT_GLOBAL            5