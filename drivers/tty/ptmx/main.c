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
#include <aplus/endian.h>
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/memory.h>
#include <aplus/module.h>
#include <aplus/pty.h>
#include <aplus/smp.h>
#include <aplus/syscall.h>

#include <fcntl.h>
#include <sys/sysmacros.h>


MODULE_NAME("tty/ptmx");
MODULE_DEPS("dev/interface,tty/pty,tty/pts");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


/**
 * @brief Hangs the pty up when the last descriptor on its master closes, and lets the master inode be freed.
 *
 * @param inode The master inode.
 * @return 0.
 */
static int ptmx_close(inode_t* inode) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->userdata);

    pty_hangup((pty_t*)inode->userdata);

    inode->flags |= INODE_FLAGS_ANONYMOUS;

    return 0;
}


/**
 * @brief Drops the reference a master inode holds on its pty as the inode is freed.
 *
 * @param inode The master inode.
 */
static void ptmx_release(inode_t* inode) {

    DEBUG_ASSERT(inode);

    if (inode->userdata)
        pty_put((pty_t*)inode->userdata);
}


/**
 * @brief Opens /dev/ptmx: creates a pty and returns the master inode private to this open.
 *
 * @param inode The /dev/ptmx inode.
 * @param flags The open() flags.
 * @return The master inode, or NULL with errno set.
 */
static inode_t* ptmx_open(inode_t* inode, int flags) {

    DEBUG_ASSERT(inode);


    inode_t* ptmx = (inode_t*)kcalloc(1, sizeof(inode_t), GFP_USER);

    if (unlikely(!ptmx))
        return errno = ENOMEM, NULL;


    strncpy(ptmx->name, inode->name, CONFIG_MAXNAMLEN - 1);

    ptmx->ino    = inode->ino;
    ptmx->sb     = inode->sb;
    ptmx->parent = inode->parent;
    ptmx->ops    = inode->ops;

    ptmx->ops.open    = NULL;
    ptmx->ops.close   = ptmx_close;
    ptmx->ops.read    = pty_master_read;
    ptmx->ops.write   = pty_master_write;
    ptmx->ops.ioctl   = pty_ioctl;
    ptmx->ops.poll    = pty_master_poll;
    ptmx->ops.release = ptmx_release;

    spinlock_init(&ptmx->lock);

    if (unlikely(!(ptmx->ev = shared_ptr_new(struct inode_events, GFP_KERNEL)))) {
        kfree(ptmx);
        return errno = ENOMEM, NULL;
    }


    pty_t* pty = pty_create(ptmx, flags);

    if (unlikely(!pty)) {
        shared_ptr_free(ptmx->ev);
        kfree(ptmx);
        return errno = ENOMEM, NULL;
    }

    ptmx->userdata = pty;

    return ptmx;
}


void init(const char* args) {


    int fd;

    if ((fd = sys_open("/dev/ptmx", O_CREAT | O_EXCL | O_WRONLY, S_IFCHR | 0666)) < 0) {
        kpanicf("ptmx: failed to create /dev/ptmx: %s\n", strerror(-fd));
    }


    inode_t* inode = NULL;

    shared_ptr_access(current_task->fd, fds, {
        DEBUG_ASSERT(fds->descriptors[fd].ref);
        DEBUG_ASSERT(fds->descriptors[fd].ref->inode);

        inode = fds->descriptors[fd].ref->inode;
    });

    DEBUG_ASSERT(inode);


    inode->ops.open  = ptmx_open;
    inode->ops.read  = NULL;
    inode->ops.write = NULL;
    inode->ops.ioctl = NULL;


    if ((fd = sys_close(fd)) < 0) {
        kpanicf("ptmx: failed to close /dev/ptmx: %s\n", strerror(-fd));
    }



    struct stat st;

    if (vfs_getattr(inode, &st) < 0) {
        kpanicf("ptmx: failed to get attributes of /dev/ptmx\n");
    }

    st.st_dev  = makedev(5, 2);
    st.st_rdev = makedev(5, 2);

    if (vfs_setattr(inode, &st) < 0) {
        kpanicf("ptmx: failed to set attributes of /dev/ptmx\n");
    }


#if DEBUG_LEVEL_INFO
    kprintf("tty/ptmx: initialized '/dev/ptmx' (major: %d, minor: %d)\n", major(st.st_dev), minor(st.st_dev));
#endif
}

void dnit(void) {

    sys_unlink("/dev/ptmx");
}
