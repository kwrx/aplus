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

#include <stdio.h>
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
#include <ctype.h>


MODULE_NAME("tty/pts");
MODULE_DEPS("dev/interface,tty/pty");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");



static int pts_getattr(inode_t* inode, struct stat* st) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->userdata);
    DEBUG_ASSERT(st);

    pty_t* pty = (pty_t*) inode->userdata;


    st->st_ino     = pty->index;
    st->st_mode    = S_IFCHR | 0666;
    st->st_dev     = makedev(136, pty->index);
    st->st_rdev    = makedev(136, pty->index);
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

static ssize_t pts_readdir(inode_t* inode, struct dirent* e, off_t position, size_t count) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(e);

    if(unlikely(count == 0))
        return 0;


    size_t i = 0;


    pty_queue_lock();

    {
    
        for(pty_t* queue = pty_queue(); queue; queue = queue->next) {

            if(position-- > 0)
                continue;


            e[i].d_ino    = queue->index;
            e[i].d_off    = position;
            e[i].d_reclen = sizeof(struct dirent);
            e[i].d_type   = DT_CHR;

            snprintf(e[i].d_name, sizeof(e[i].d_name), "%lld", queue->index);


            if(++i == count)
                break;

        }

    }

    pty_queue_unlock();

    return i;

}

static inode_t* pts_finddir(inode_t* inode, const char* name) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(name);
    DEBUG_ASSERT(name[0] != '\0');


    if(unlikely(!isdigit(name[0]))) {
        return errno = ENOENT, NULL;
    }


    uint64_t index = atoll(name);

    pty_queue_lock();

    {
    
        for(pty_t* queue = pty_queue(); queue; queue = queue->next) {

            if(queue->index != index)
                continue;

            
            inode_t* d = (inode_t*) kcalloc(sizeof(inode_t), 1, GFP_USER);

            strncpy(d->name, name, CONFIG_MAXNAMLEN);

            d->ino    = index;
            d->sb     = inode->sb;
            d->parent = inode;

            spinlock_init(&d->lock);


            d->ops.getattr = pts_getattr;
            d->ops.setattr = NULL;

            d->ops.read  = pty_slave_read;
            d->ops.write = pty_slave_write;
            d->ops.ioctl = pty_ioctl;

            d->userdata = queue;

            return d;

        }

    }

    pty_queue_unlock();

    
    return errno = ENOENT, NULL;

}


void init(const char* args) {


    int fd;

    if((fd = sys_creat("/dev/pts", S_IFDIR | 0666)) < 0) {
        kpanicf("ptmx: failed to create /dev/pts: %s", strerror(-fd));
    }


    DEBUG_ASSERT(current_task->fd->descriptors[fd].ref);
    DEBUG_ASSERT(current_task->fd->descriptors[fd].ref->inode);

    inode_t* inode = current_task->fd->descriptors[fd].ref->inode;

    inode->ops.open     = NULL;
    inode->ops.read     = NULL;
    inode->ops.readdir  = pts_readdir;
    inode->ops.finddir  = pts_finddir;
    inode->ops.unlink   = NULL;
    inode->ops.rename   = NULL;
    inode->ops.creat    = NULL;
    inode->flags        = INODE_FLAGS_DCACHE_DISABLED;


    if((fd = sys_close(fd)) < 0) {
        kpanicf("ptmx: failed to close /dev/ptmx: %s", strerror(-fd));
    }



#if DEBUG_LEVEL_INFO
    kprintf("tty/pts: initialized '/dev/pts'\n");
#endif

}

void dnit(void) {

    sys_unlink("/dev/ptmx");

}