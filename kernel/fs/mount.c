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
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <aplus/mm.h>
#include <aplus/ipc.h>
#include <aplus/timer.h>
#include <aplus/debug.h>
#include <libc.h>

list(mountpoint_t*, mnt_queue);


static int __vfs_bind(struct inode* dev, struct inode* dir, int flags) {
    if(unlikely(!dev || !dir)) {
        errno = EINVAL;
        return -1;
    }

    struct mountinfo* info = (struct mountinfo*) kmalloc(sizeof(struct mountinfo), GFP_KERNEL);
    if(!info) {
        errno = ENOMEM;
        return -1;
    }
    
    memcpy(info, dev->mtinfo, sizeof(struct mountinfo));
    dir->mtinfo = info;

    if(dir->mtinfo)
        dir->mtinfo->flags |= flags;


    dir->userdata = dev->userdata;
    dir->open = dev->open;
    dir->close = dev->close;
    dir->read = dev->read;
    dir->write = dev->write;
    dir->finddir = dev->finddir;
    dir->mknod = dev->mknod;
    dir->unlink = dev->unlink;
    dir->chown = dev->chown;
    dir->chmod = dev->chmod;
    dir->ioctl = dev->ioctl;
    dir->fsync = dev->fsync;

    dir->childs = NULL;
    dir->link = NULL;

    return 0;
}

static int __vfs_mount(struct inode* dev, struct inode* dir, const char* fstype, int flags) {

    if(unlikely(!dir)) {
        errno = EINVAL;
        return -1;
    }
    
    if(flags & MS_BIND)
        return __vfs_bind(dev, dir, flags);

    if(flags & MS_REMOUNT)
        vfs_umount(dir);


    fsys_t* fsys = vfs_fsys_find(fstype);
    if(unlikely(!fsys)) {
        errno = ENODEV;
        return -1;
    }

    struct mountinfo* info = (struct mountinfo*) kmalloc(sizeof(struct mountinfo), GFP_KERNEL);
    if(!info) {
        errno = ENOMEM;
        return -1;
    }



    strncpy(info->fstype, fstype, sizeof(info->fstype) - 1);
    info->flags = flags;
    info->dev = dev;
    info->stat.f_fsid = fsys->id;

    #define _(x, y)                         \
        if(flags & x)                       \
            info->stat.f_flag |= y

    
    _(MS_MANDLOCK, ST_MANDLOCK);
    _(MS_NOATIME, ST_NOATIME);
    _(MS_NODEV, ST_NODEV);
    _(MS_NODIRATIME, ST_NODIRATIME);
    _(MS_NOEXEC, ST_NOEXEC);
    _(MS_RDONLY, ST_RDONLY);
    _(MS_SYNCHRONOUS, ST_SYNCHRONOUS);

    #undef _




    if(fsys->mount(dev, dir, info) != 0) {
        kfree(info);
        return -1;
    }

    return 0;
}



int vfs_mount(struct inode* dev, struct inode* dir, const char* fstype, int flags) {
    int e;
    if((e = __vfs_mount(dev, dir, fstype, flags)) != 0)
        return e;

    if(flags & MS_KERNMOUNT)
        return 0;

    mountpoint_t* mt = (mountpoint_t*) kmalloc(sizeof(mountpoint_t*), GFP_KERNEL);
    if(!mt) {
        errno = ENOMEM;
        return -1;
    }

    mt->id = vfs_inode();
    mt->root = dir;
    mt->info = dir->mtinfo;
    mt->parent = NULL;

    if(dir->parent && dir->parent->mtinfo) {
        list_each(mnt_queue, m) {
            if(m->info != dir->parent->mtinfo)
                continue;

            mt->parent = m;
            break;
        }
    }

    list_push(mnt_queue, mt);
    return 0;
}




int vfs_umount(struct inode* dir) {

    mountpoint_t* mt = NULL;
    list_each(mnt_queue, tmp) {
        if(tmp->root != dir)
            continue;

        mt = tmp;
        break;
    }

    if(unlikely(!mt)) {
        errno = EINVAL;
        return -1;
    }

    list_each(mnt_queue, child)
        if(child->parent == mt)
            vfs_umount(child->root);

    



    struct inode_childs* t0 = dir->childs;
    struct inode_childs* t1 = NULL;
    for(; t0; ) {
        t1 = t0;
        t0 = t0->next;

        kfree(t1);
    }

    kfree(dir->mtinfo);


    dir->childs = NULL;

    dir->read = NULL;
    dir->write = NULL;
    dir->finddir = NULL;
    dir->mknod = NULL;
    dir->unlink = NULL;
    dir->chown = NULL;
    dir->chmod = NULL;
    dir->ioctl = NULL;
    dir->open = NULL;
    dir->close = NULL;
    dir->mtinfo = NULL;

    return 0;
}


EXPORT(vfs_mount);
EXPORT(vfs_umount);
EXPORT(mnt_queue);