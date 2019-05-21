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
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <string.h>
#include <errno.h>

#define VFS_NINODES                 (64 * 1024)


inode_t _vfs_root;
inode_t* vfs_root = &_vfs_root;

static heap_t* vfs_heap;

void vfs_init(void) {
    #define _ _vfs_root
    memset(&_, 0, sizeof(_));

    _.name[0] = '/';
    _.name[1] = '\0';

    _.st.st_ino = 0;
    _.st.st_mode = S_IFDIR;
    _.st.st_nlink = 1;

    //_.st.st_atime = arch_timer_now();
    //_.st.st_ctime = arch_timer_now();
    //_.st.st_mtime = arch_timer_now();

    spinlock_init(&_.lock);
    #undef _


    heap_create(&vfs_heap, sizeof(inode_t), VFS_NINODES);

    if(unlikely(!vfs_heap))
        kpanic("heap_create(): could not create heap for vfs, size %d\n", VFS_NINODES * sizeof(inode_t));
}

int vfs_open(inode_t* inode) {
    DEBUG_ASSERT(inode);

    if(likely(inode->ops.open))
        __lock_return(&inode->lock, int, inode->ops.open(inode));

    errno = ENOSYS;
    return 0;
}

int vfs_close(inode_t* inode) {
    DEBUG_ASSERT(inode);

    if(likely(inode->ops.close))
        __lock_return(&inode->lock, int, inode->ops.close(inode));

    errno = ENOSYS;
    return 0;
}

int vfs_read(inode_t* inode, void* buf, off_t off, size_t size) {
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(buf);

    if(unlikely(size == 0))
        return 0;

    if(likely(inode->ops.read))
        __lock_return(&inode->lock, int, inode->ops.read(inode, buf, off, size));
    
    errno = ENOSYS;
    return 0;
}

int vfs_write(inode_t* inode, const void* buf, off_t off, size_t size) {
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(buf);

    if(unlikely(size == 0))
        return 0;

    if(likely(inode->ops.write))
        __lock_return(&inode->lock, int, inode->ops.write(inode, buf, off, size));
    
    errno = ENOSYS;
    return 0;
}

inode_t* vfs_finddir(inode_t* inode, char* name) {
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(name);

    if(name[0] == '.' && name[1] == '\0')
        return inode;

    if(name[0] == '.' && name[1] == '.' && name[2] == '\0')
        return inode->parent;


    if(likely(inode->ops.finddir))
        __lock_return(&inode->lock, inode_t*, inode->ops.finddir(inode, name));
    
    errno = ENOSYS;
    return NULL;
}


inode_t* vfs_mknod(inode_t* inode, char* name, mode_t mode) {
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(name);

    if(name[0] == '.' && name[1] == '\0')
        return NULL;

    if(name[0] == '.' && name[1] == '.' && name[2] == '\0')
        return NULL;


    if(likely(inode->ops.mknod))
        __lock_return(&inode->lock, inode_t*, inode->ops.mknod(inode, name, mode));
    
    errno = ENOSYS;
    return NULL;
}

int vfs_getdents(inode_t* inode, struct dirent* ent, off_t off, size_t size) {
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(ent);

    if(unlikely(size == 0))
        return 0;

    if(likely(inode->ops.getdents))
        __lock_return(&inode->lock, int, inode->ops.getdents(inode, ent, off, size));
    
    errno = ENOSYS;
    return 0;
}

int vfs_unlink(inode_t* inode, char* name) {
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(name);

    if(name[0] == '.' && name[1] == '\0')
        return -1;

    if(name[0] == '.' && name[1] == '.' && name[2] == '\0')
        return -1;


    if(likely(inode->ops.unlink))
        __lock_return(&inode->lock, int, inode->ops.unlink(inode, name));
    
    errno = EROFS;
    return -1;
}

int vfs_ioctl(inode_t* inode, int req, void* arg) {
    DEBUG_ASSERT(inode);

    if(likely(inode->ops.ioctl))
        __lock_return(&inode->lock, int, inode->ops.ioctl(inode, req, arg));
    
    errno = ENOSYS;
    return -1;
}

int vfs_fsync(inode_t* inode) {
    DEBUG_ASSERT(inode);

    if(likely(inode->ops.fsync))
        __lock_return(&inode->lock, int, inode->ops.fsync(inode));
    
    errno = ENOSYS;
    return -1;
}
