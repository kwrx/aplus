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
#include <aplus/module.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <libc.h>

MODULE_NAME("char/zero");
MODULE_DEPS("");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


static int zero_read(struct inode* inode, void* buf, off_t pos, size_t size) {
    if(unlikely(!buf || !size)) {
        errno = EINVAL;
        return -1;
    }
    
    memset(buf, 0, size);
    return size;
}

int init(void) {
    inode_t* ino;
    if(unlikely((ino = vfs_mkdev("zero", -1, S_IFCHR | 0444)) == NULL))
        return -1;


    ino->read = zero_read;
    return 0;
}



int dnit(void) {
    sys_unlink("/dev/zero");
    return 0;
}
