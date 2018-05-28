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
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <libc.h>

MODULE_NAME("pc/char/uart");
MODULE_DEPS("arch/x86");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


#if defined(__i386__) || defined(__x86_64__)
#    if defined(__i386__)
#        include <arch/i386/i386.h>
#    elif defined(__x86_64__)
#        include <arch/x86_64/x86_64.h>
#    endif

static int COM[] = {
    0x3F8, 0x2F8, 0x3E8, 0x2E8
};


static int uart_ioctl(struct inode* inode, int req, void* buf) {
    errno = ENOSYS;
    return -1;
}

static int uart_read(struct inode* inode, void* buf, off_t pos, size_t size) {

    int idx = '0' - inode->name[4];

    char* bc = (char*) buf;
    size_t i;
    for(i = 0; i < size; i++) {
        while((inb(COM[idx] + 5) & 1) == 0)
            ;

        *bc++ = inb(COM[idx]);
    }

    return size;
}

static int uart_write(struct inode* inode, void* buf, off_t pos, size_t size) {
    int idx = '0' - inode->name[4];

    char* bc = (char*) buf;
    size_t i;
    for(i = 0; i < size; i++) {
        while((inb(COM[idx] + 5) & 0x20) == 0)
            ;

        

        outb(COM[idx], *bc++);
    }

    return size;
}

int init(void) {

    int i;
    for(i = 0; i < 4; i++) {
        outb(COM[i] + 1, 0x00);
        outb(COM[i] + 3, 0x80);
        outb(COM[i] + 0, 0x03);
        outb(COM[i] + 1, 0x00);
        outb(COM[i] + 3, 0x03);
        outb(COM[i] + 2, 0xC7);
        outb(COM[i] + 4, 0x0B);

        inode_t* ino = vfs_mkdev("uart", i, S_IFCHR | 0666);
        ino->ioctl = uart_ioctl;
        ino->read = uart_read;
        ino->write = uart_write;
    }
    
    return 0;
}


#else

int init(void) {
    return -1;
}

#endif


int dnit(void) {
    return 0;
}
