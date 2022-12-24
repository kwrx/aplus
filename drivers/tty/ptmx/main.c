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

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/module.h>
#include <aplus/memory.h>
#include <aplus/smp.h>
#include <aplus/hal.h>
#include <aplus/pty.h>
#include <aplus/syscall.h>
#include <aplus/errno.h>
#include <aplus/endian.h>

#include <sys/sysmacros.h>


MODULE_NAME("tty/ptmx");
MODULE_DEPS("dev/interface,tty/pty,tty/pts");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


static inode_t* ptmx_open(inode_t* inode, int flags) {

    DEBUG_ASSERT(inode);

    
    inode_t* ptmx = (inode_t*) kcalloc(sizeof(inode_t), 1, GFP_USER);

    if(unlikely(!ptmx))
        goto fail_1;


    pty_t* pty = pty_create(flags);

    if(unlikely(!pty))
        goto fail_2;


    memcpy(ptmx, inode, sizeof(inode_t));


    ptmx->ops.open  = NULL;
    ptmx->ops.close = NULL;
    ptmx->ops.read  = pty_master_read;
    ptmx->ops.write = pty_master_write;
    ptmx->ops.ioctl = pty_ioctl;
    ptmx->userdata  = pty;

    spinlock_init(&ptmx->lock);


    return ptmx;


fail_2:

    if(pty)
        kfree(pty);

fail_1:

    if(ptmx)
        kfree(ptmx);


    return NULL;

}


void init(const char* args) {


    int fd;

    if((fd = sys_creat("/dev/ptmx", S_IFCHR | 0666)) < 0) {
        kpanicf("ptmx: failed to create /dev/ptmx: %s\n", strerror(-fd));
    }


    DEBUG_ASSERT(current_task->fd->descriptors[fd].ref);
    DEBUG_ASSERT(current_task->fd->descriptors[fd].ref->inode);

    inode_t* inode = current_task->fd->descriptors[fd].ref->inode;

    inode->ops.open  = ptmx_open;
    inode->ops.read  = NULL;
    inode->ops.write = NULL;
    inode->ops.ioctl = NULL;


    if((fd = sys_close(fd)) < 0) {
        kpanicf("ptmx: failed to close /dev/ptmx: %s\n", strerror(-fd));
    }



    struct stat st;

    if(vfs_getattr(inode, &st) < 0) {
        kpanicf("ptmx: failed to get attributes of /dev/ptmx\n");
    }
    
    st.st_dev  = makedev(5, 2);
    st.st_rdev = makedev(5, 2);
    
    if(vfs_setattr(inode, &st) < 0) {
        kpanicf("ptmx: failed to set attributes of /dev/ptmx\n");
    }


#if DEBUG_LEVEL_INFO
    kprintf("tty/ptmx: initialized '/dev/ptmx' (major: %d, minor: %d)\n", major(st.st_dev), minor(st.st_dev));
#endif

}

void dnit(void) {

    sys_unlink("/dev/ptmx");

}