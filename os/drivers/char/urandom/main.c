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
#include <stdlib.h>
#include <errno.h>

MODULE_NAME("char/urandom");
MODULE_DEPS("");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");



static int random_read(inode_t* inode, void __user * buf, off_t pos, size_t size) {

    srand(sys_times(NULL));

    char* bc = (char*) buf;
    size_t i;
    for(i = 0; i < size; i++)
        *bc++ = rand() % 256;

    return size;

}

void init(const char* args) {
    /*inode_t* ino;
    if(unlikely((ino = vfs_mkdev("random", -1, S_IFCHR | 0444)) == NULL))
        kpanic("urandom: could not create device");

    ino->ops.read = random_read;*/
}



void dnit(void) {
    //sys_unlink("/dev/random");
}
