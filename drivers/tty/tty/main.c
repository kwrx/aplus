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
#include <stdio.h>
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

#include <ctype.h>
#include <sys/sysmacros.h>


MODULE_NAME("tty/tty");
MODULE_DEPS("dev/interface,tty/pty,tty/pts");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


static int tty_getattr(inode_t *inode, struct stat *st) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(st);

    st->st_ino     = inode->ino;
    st->st_mode    = S_IFLNK | 0666;
    st->st_dev     = makedev(0, 5);
    st->st_rdev    = makedev(0, 5);
    st->st_nlink   = 1;
    st->st_uid     = 0;
    st->st_gid     = 0;
    st->st_size    = 0;
    st->st_blksize = 0;
    st->st_blocks  = 0;
    st->st_atime   = arch_timer_gettime();
    st->st_mtime   = arch_timer_gettime();
    st->st_ctime   = arch_timer_gettime();

    return 0;
}


static ssize_t tty_readlink(inode_t *inode, char *buf, size_t size) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(buf);

    if (unlikely(size == 0))
        return 0;

    shared_ptr_access(current_task->ctty, ctty, {
        if (unlikely(!(*ctty)))
            return -ENOENT;

        return snprintf(buf, size, "/dev/pts/%lu", (*ctty)->index);
    });
}

void init(const char *args) {


    int fd;

    if ((fd = sys_creat("/dev/tty", S_IFREG | 0666)) < 0) {
        kpanicf("tty: failed to create /dev/tty: %s", strerror(-fd));
    }


    inode_t *inode = NULL;

    shared_ptr_access(current_task->fd, fds, {
        DEBUG_ASSERT(fds->descriptors[fd].ref);
        DEBUG_ASSERT(fds->descriptors[fd].ref->inode);

        inode = fds->descriptors[fd].ref->inode;
    });

    DEBUG_ASSERT(inode);


    inode->ops.open     = NULL;
    inode->ops.read     = NULL;
    inode->ops.readdir  = NULL;
    inode->ops.finddir  = NULL;
    inode->ops.unlink   = NULL;
    inode->ops.rename   = NULL;
    inode->ops.creat    = NULL;
    inode->ops.getattr  = tty_getattr;
    inode->ops.readlink = tty_readlink;

    inode->flags = INODE_FLAGS_DCACHE_DISABLED;


    if ((fd = sys_close(fd)) < 0) {
        kpanicf("tty: failed to close /dev/tty: %s", strerror(-fd));
    }



#if DEBUG_LEVEL_INFO
    kprintf("tty/tty: initialized '/dev/tty'\n");
#endif
}

void dnit(void) {

    sys_unlink("/dev/tty");
}
