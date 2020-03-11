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

#ifndef _DEV_CHAR_H
#define _DEV_CHAR_H

#ifndef __ASSEMBLY__
#include <sys/cdefs.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/syscall.h>
#include <stdint.h>

#include <dev/interface.h>

#define CHAR_IO_NBF             0   // No Buffering
#define CHAR_IO_LBF             1   // Line Buffered
#define CHAR_IO_FBF             2   // Full Buffered


__BEGIN_DECLS

ssize_t char_write(device_t*, const void*, size_t);
ssize_t char_read(device_t*, void*, size_t);
int char_flush(device_t*);
void char_init(device_t*);
void char_dnit(device_t*);

__END_DECLS

#endif
#endif