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


#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <stdint.h>
#include <errno.h>

MODULE_NAME("char/null");
MODULE_DEPS("");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


static int null_read_write(struct inode* inode, void* buf, off_t pos, size_t size) {
    return size;
}


void init(const char* args) {
    /*inode_t* ino;
    if(unlikely((ino = vfs_mkdev("null", -1, S_IFCHR | 0666)) == NULL))
        kpanic("null: could not create device");

    ino->ops.read =
    ino->ops.write = null_read_write;

    return 0;*/
}



void dnit(void) {
    //sys_unlink("/dev/null");
}