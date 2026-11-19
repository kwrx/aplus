/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2026 Antonino Natale
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

#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/memory.h>
#include <aplus/vfs.h>
#include <aplus/vmobj.h>


#define MEMFD_FSID      0xDEADCAFF
#define MEMFD_FIRST_INO 0xFFFFFFFFE000000


static struct superblock memfd_superblock = {
    .fsid   = MEMFD_FSID,
    .flags  = ST_NOATIME | ST_NODEV | ST_NOSUID | ST_NOEXEC,
    .dev    = NULL,
    .root   = NULL,
    .ino    = MEMFD_FIRST_INO,
    .fsinfo = NULL,
    .st     = {.f_bsize = PML1_PAGESIZE, .f_frsize = PML1_PAGESIZE, .f_fsid = MEMFD_FSID, .f_namemax = CONFIG_MAXNAMLEN},
};

static _Atomic ino64_t __memfd_next_ino = MEMFD_FIRST_INO + 1;


static ssize_t memfd_read(inode_t* inode, void* buf, off_t pos, size_t len) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->userdata);

    return vmobj_read((vmobj_t*)inode->userdata, buf, pos, len);
}


static ssize_t memfd_write(inode_t* inode, const void* buf, off_t pos, size_t len) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->userdata);

    return vmobj_write((vmobj_t*)inode->userdata, buf, pos, len);
}


static int memfd_truncate(inode_t* inode, off_t len) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->userdata);

    if (unlikely(len < 0)) {
        return errno = EINVAL, -1;
    }

    int e = vmobj_resize((vmobj_t*)inode->userdata, (size_t)len);

    if (unlikely(e < 0)) {
        return errno = -e, -1;
    }

    return 0;
}


static int memfd_getattr(inode_t* inode, struct stat* st) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->userdata);
    DEBUG_ASSERT(st);

    st->st_ino     = inode->ino;
    st->st_mode    = S_IFREG | 0777;
    st->st_rdev    = 0;
    st->st_nlink   = 1;
    st->st_size    = (off_t)vmobj_size((vmobj_t*)inode->userdata);
    st->st_blksize = PML1_PAGESIZE;
    st->st_blocks  = (st->st_size + 511) / 512;

    st->st_mtime = arch_timer_gettime();
    st->st_atime = arch_timer_gettime();
    st->st_ctime = arch_timer_gettime();

    return 0;
}


static struct vmobj* memfd_mmap(inode_t* inode) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->userdata);

    vmobj_get((vmobj_t*)inode->userdata);

    return (vmobj_t*)inode->userdata;
}


static void memfd_release(inode_t* inode) {

    DEBUG_ASSERT(inode);

    if (inode->userdata) {
        vmobj_put((vmobj_t*)inode->userdata);
    }

    inode->userdata = NULL;
}


inode_t* memfd_inode(const char* name) {

    DEBUG_ASSERT(name);

    inode_t* inode = (inode_t*)kcalloc(1, sizeof(inode_t), GFP_KERNEL);

    if (unlikely(!inode)) {
        return errno = ENOMEM, NULL;
    }

    vmobj_t* obj = vmobj_create(0);

    if (unlikely(!obj)) {
        kfree(inode);
        return errno = ENOMEM, NULL;
    }


    snprintf(inode->name, sizeof(inode->name), "memfd:%s", name);

    inode->ino      = atomic_fetch_add(&__memfd_next_ino, 1);
    inode->sb       = &memfd_superblock;
    inode->parent   = NULL;
    inode->flags    = INODE_FLAGS_ANONYMOUS;
    inode->userdata = obj;

    inode->ops.read     = memfd_read;
    inode->ops.write    = memfd_write;
    inode->ops.truncate = memfd_truncate;
    inode->ops.getattr  = memfd_getattr;
    inode->ops.mmap     = memfd_mmap;
    inode->ops.release  = memfd_release;

    spinlock_init(&inode->lock);

    return inode;
}
