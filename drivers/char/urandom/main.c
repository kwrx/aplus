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
#include <aplus/vfs.h>
#include <libc.h>

MODULE_NAME("char/urandom");
MODULE_DEPS("");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


static int urandom_read(struct inode* inode, void* buf, off_t pos, size_t size) {
    char* bc = (char*) buf;
    size_t i;
    for(i = 0; i < size; i++)
        *bc++ = rand() % 256;

    return size;
}

int init(void) {
    inode_t* ino;
    if(unlikely((ino = vfs_mkdev("urandom", -1, S_IFCHR | 0444)) == NULL))
        return -1;


    ino->read = urandom_read;
    return 0;
}



int dnit(void) {
    sys_unlink("/dev/urandom");
    return 0;
}
