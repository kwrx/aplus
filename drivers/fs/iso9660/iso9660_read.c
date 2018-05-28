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
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <libc.h>

#include "iso9660.h"

int iso9660_read(inode_t* ino, void* buf, off_t pos, size_t size) {
    if(unlikely(!ino))
        return 0;
        
    if(unlikely(!buf))
        return 0;
        
    if(unlikely(size > ino->size))
        size = ino->size;
        
    if(unlikely(pos > ino->size))
        pos = ino->size;
        
    if(unlikely(pos + size > ino->size))
        size = (off_t) (ino->size - pos);
        
    if(unlikely(!size))
        return 0;

    iso9660_t* ctx = (iso9660_t*) ino->userdata;
    if(unlikely(!ctx))
        return 0;

        
    return vfs_read(ctx->dev, buf, (iso9660_getlsb32(ctx->dir.lba) * ISO9660_SECTOR_SIZE) + pos, size);
}
