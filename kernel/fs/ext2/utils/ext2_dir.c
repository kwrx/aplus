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

#include <stdint.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <unistd.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>
#include <aplus/smp.h>
#include <aplus/vfs.h>

#include "../ext2.h"


mode_t ext2_utils_file_type(uint8_t type) {

    switch (type) {

        case EXT2_FT_REG_FILE:
            return S_IFREG;

        case EXT2_FT_DIR:
            return S_IFDIR;

        case EXT2_FT_CHRDEV:
            return S_IFCHR;

        case EXT2_FT_BLKDEV:
            return S_IFBLK;

        case EXT2_FT_FIFO:
            return S_IFIFO;

        case EXT2_FT_SOCK:
            return S_IFSOCK;

        case EXT2_FT_SYMLINK:
            return S_IFLNK;

        default:
            break;
    }

    kpanicf("ext2: PANIC! invalid file_type: %d\n", type);
    return -1;
}
