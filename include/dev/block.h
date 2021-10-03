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

#ifndef _DEV_BLOCK_H
#define _DEV_BLOCK_H

#ifndef __ASSEMBLY__


#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/syscall.h>
#include <stdint.h>

#include <dev/interface.h>


__BEGIN_DECLS

ssize_t block_write(device_t*, const void*, off_t, size_t);
ssize_t block_read(device_t*, void*, off_t, size_t);
void block_inode(device_t*, inode_t*, void (*) (device_t*, mode_t));
void block_init(device_t*);
void block_dnit(device_t*);

__END_DECLS

#endif
#endif