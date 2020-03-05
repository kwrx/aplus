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

#include <stdint.h>
#include <string.h>
#include <aplus.h>
#include <aplus/multiboot.h>
#include <aplus/debug.h>
#include <aplus/memory.h>

#include <aplus/vfs.h>
#include <aplus/errno.h>



#define bind_fn(name, type, params, params2)    \
    static type bindfs_##name params {          \
        DEBUG_ASSERT(inode);                    \
        DEBUG_ASSERT(inode->sb);                \
        DEBUG_ASSERT(inode->sb->fsid == 1);     \
        DEBUG_ASSERT(inode->sb->dev);           \
                                                \
        inode_t* d = inode->sb->dev;            \
        return vfs_##name params2;              \
    }



bind_fn (getattr,    int, (inode_t* inode, struct stat* st), (d, st))
bind_fn (setattr,    int, (inode_t* inode, struct stat* st), (d, st))
bind_fn (truncate,   int, (inode_t* inode, off_t off), (d, off))

bind_fn (read,       ssize_t, (inode_t* inode, void* buf, off_t off, size_t size), (d, buf, off, size))
bind_fn (write,      ssize_t, (inode_t* inode, const void* buf, off_t off, size_t size), (d, buf, off, size))
bind_fn (readlink,   ssize_t, (inode_t* inode, char* buf, size_t size), (d, buf, size))

bind_fn (creat,      inode_t*, (inode_t* inode, const char* name, mode_t mode), (d, name, mode))
bind_fn (finddir,    inode_t*, (inode_t* inode, const char* name), (d, name))
bind_fn (readdir,    ssize_t,  (inode_t* inode, struct dirent* e, off_t off, size_t size), (d, e, off, size))

bind_fn (rename,     int, (inode_t* inode, const char* o, const char* n), (d, o, n))
bind_fn (symlink,    int, (inode_t* inode, const char* o, const char* n), (d, o, n))
bind_fn (unlink,     int, (inode_t* inode, const char* o), (d, o))




int bindfs_mount(inode_t* dev, inode_t* dir, int flags, const char * args) {

    DEBUG_ASSERT(dev);
    DEBUG_ASSERT(dev->sb);
    DEBUG_ASSERT(dir);

    (void) args;


    dir->sb = (struct superblock*) kcalloc(sizeof(struct superblock), 1, GFP_KERNEL);
    dir->sb->fsid = 1;
    dir->sb->dev = dev;
    dir->sb->root = dir;
    dir->sb->flags = flags;
    dir->sb->fsinfo = NULL;
    dir->sb->ino = dir->ino;

    memcpy(&dir->sb->st, &dev->sb->st, sizeof(struct statvfs));
    

    dir->sb->ops.getattr    = bindfs_getattr;
    dir->sb->ops.setattr    = bindfs_setattr;
    dir->sb->ops.truncate   = bindfs_truncate;
    dir->sb->ops.read       = bindfs_read;
    dir->sb->ops.write      = bindfs_write;
    dir->sb->ops.readlink   = bindfs_readlink;
    dir->sb->ops.creat      = bindfs_creat;
    dir->sb->ops.finddir    = bindfs_finddir;
    dir->sb->ops.readdir    = bindfs_readdir;
    dir->sb->ops.rename     = bindfs_rename;
    dir->sb->ops.symlink    = bindfs_symlink;
    dir->sb->ops.unlink     = bindfs_unlink;

    return 0;
}



int bindfs_umount(inode_t* dir) {

    DEBUG_ASSERT(dir);
    DEBUG_ASSERT(dir->sb);

    return 0;
}