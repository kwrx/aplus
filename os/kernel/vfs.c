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


struct {
    int id;
    const char* name;
    int (*mount)(inode_t*, inode_t*, int, const char*);
    int (*umount) (inode_t*);
} vfs_table[32];


inode_t _vfs_root;
inode_t* vfs_root = &_vfs_root;

inode_t* vfs_dev = NULL;


void vfs_init(void) {
    
    memset(&_vfs_root, 0, sizeof(_vfs_root));
    memset(&vfs_table, 0, sizeof(vfs_table));

    int i = 0;
    #include "fs/fstable.c.in"


    _vfs_root.name[0] = '/';
    _vfs_root.name[1] = '\0';

    _vfs_root.st.st_ino = 1;
    _vfs_root.st.st_mode = S_IFDIR;
    _vfs_root.st.st_nlink = 1;

    //_vfs_root.st.st_atime = arch_timer_now();
    //_vfs_root.st.st_ctime = arch_timer_now();
    //_vfs_root.st.st_mtime = arch_timer_now();

    spinlock_init(&_vfs_root.lock);

}



int vfs_mount(inode_t* dev, inode_t* dir, const char __user * fs, int flags, const char __user * args) {
    DEBUG_ASSERT(dir);
    DEBUG_ASSERT(fs);

    if(unlikely(!ptr_check(fs, R_OK)))
        return -EFAULT;

    if(unlikely(!ptr_check(args, R_OK)))
        return -EFAULT;

    
    int i;
    for(i = 0; vfs_table[i].id; i++) {
        if(strcmp(vfs_table[i].name, fs) != 0)
            continue;


        DEBUG_ASSERT(vfs_table[i].mount);
        DEBUG_ASSERT(vfs_table[i].umount);


        int e;
        if((e = vfs_table[i].mount(dev, dir, flags, args) != 0))
            return e;

        __lock(&dir->lock, {

            struct inode_ops ops;
            memcpy(&ops, &dir->ops, sizeof(ops));
            memcpy(&dir->ops, &dir->mount.ops, sizeof(ops));
            memcpy(&dir->mount.ops, &ops, sizeof(ops));

        });


        kprintf("mount: volume %s mounted on %s with %s\n", dev ? dev->name : "nodev", dir->name, fs);
        return 0;
    }

    return -EINVAL;
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