/*                                                                      
 * GPL3 License                                                         
 *                                                                      
 * Author(s):                                                              
 *      Antonino Natale <antonio.natale97@hotmail.com>                  
 *                                                                      
 *                                                                      
 * Copyright (c) 2013-2019 Antonino Natale                              
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
#include <poll.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/memory.h>
#include <aplus/vfs.h>
#include <aplus/errno.h>


static struct {

    int id;
    const char* name;

    int (*mount)  (inode_t*, inode_t*, int, const char*);
    int (*umount) (inode_t*);

} fs_table[32];





void vfs_init(void) {

    int i = 0;
    #include "fstable.c.in"


#if DEBUG_LEVEL_INFO

    for(i = 0; fs_table[i].id; i++)
        kprintf("vfs: found filesystem '%s'\n", fs_table[i].name);

#endif


    rootfs_init();
    fd_init();

#if DEBUG_LEVEL_INFO
    kprintf("vfs: ready!\n");
#endif

}


int vfs_mount(inode_t* dev, inode_t* dir, const char* fs, int flags, const char* args) {

    DEBUG_ASSERT(dir);
    DEBUG_ASSERT(fs);


    int i;
    for(i = 0; fs_table[i].id; i++) {

        if(strcmp(fs_table[i].name, fs) != 0)
            continue;


        DEBUG_ASSERT(fs_table[i].mount);
        DEBUG_ASSERT(fs_table[i].umount);


        int e;
        if((e = fs_table[i].mount(dev, dir, flags, args)) < 0)
            return e;

        DEBUG_ASSERT(dir->sb);


        struct inode_ops ops;
        memcpy(&ops, &dir->ops, sizeof(ops));
        memcpy(&dir->ops, &dir->sb->ops, sizeof(ops));
        memcpy(&dir->sb->ops, &ops, sizeof(ops));


        dir->ino ^= dir->sb->ino;
        dir->sb->ino ^= dir->ino;
        dir->ino ^= dir->sb->ino;


#if DEBUG_LEVEL_INFO
        kprintf("mount: volume %s mounted on %s with %s\n", dev ? dev->name : "nodev", dir->name, fs);
#endif

        return 0;
    }


    return errno = EINVAL, -1;
}


int vfs_open(inode_t* inode, int flags) {
    DEBUG_ASSERT(inode);

    if(likely(inode->ops.open))
        __lock_return(&inode->lock, int, inode->ops.open(inode, flags));

    return errno = ENOSYS, -1;
}


int vfs_close(inode_t* inode) {
    DEBUG_ASSERT(inode);

    if(likely(inode->ops.close))
        __lock_return(&inode->lock, int, inode->ops.close(inode));

    return errno = ENOSYS, -1;
}


int vfs_ioctl(inode_t* inode, long req, void* arg) {
    DEBUG_ASSERT(inode);

    if(likely(inode->ops.ioctl))
        __lock_return(&inode->lock, int, inode->ops.ioctl(inode, req, arg));
    
    return errno = ENOSYS, -1;
}


int vfs_getattr (inode_t* inode, struct stat* st) {
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(st);

    if(likely(inode->ops.getattr))
        __lock_return(&inode->lock, int, inode->ops.getattr(inode, st));

    return errno = ENOSYS, -1;
}


int vfs_setattr (inode_t* inode, struct stat* st) {
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(st);

    if(likely(inode->ops.setattr))
        __lock_return(&inode->lock, int, inode->ops.setattr(inode, st));

    return errno = EROFS, -1;
}


int vfs_setxattr (inode_t* inode, const char* name, const void* value, size_t size, int flags) {
    
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(name);
    DEBUG_ASSERT(value);
    DEBUG_ASSERT(size);

    if(likely(inode->ops.setxattr))
        __lock_return(&inode->lock, int, inode->ops.setxattr(inode, name, value, size, flags));

    return errno = EROFS, -1;

}


int vfs_getxattr (inode_t* inode, const char* name, void* value, size_t size) {
    
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(name);
    DEBUG_ASSERT(value);
    DEBUG_ASSERT(size);

    if(likely(inode->ops.getxattr))
        __lock_return(&inode->lock, int, inode->ops.getxattr(inode, name, value, size));

    return errno = ENOSYS, -1;

}


int vfs_listxattr (inode_t* inode, char* buf, size_t size) {
    
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(size);
    
    if(likely(inode->ops.listxattr))
        __lock_return(&inode->lock, int, inode->ops.listxattr(inode, buf, size));

    return errno = ENOSYS, -1;

}


int vfs_removexattr (inode_t* inode, const char* name) {
    
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(name);

    if(likely(inode->ops.removexattr))
        __lock_return(&inode->lock, int, inode->ops.removexattr(inode, name));

    return errno = EROFS, -1;

}


int vfs_truncate (inode_t* inode, off_t len) {
    DEBUG_ASSERT(inode);

    if(likely(inode->ops.truncate))
        __lock_return(&inode->lock, int, inode->ops.truncate(inode, len));

    return errno = EROFS, -1;
}


int vfs_fsync (inode_t* inode, int datasync) {
    DEBUG_ASSERT(inode);

    if(likely(inode->ops.fsync))
        __lock_return(&inode->lock, int, inode->ops.fsync(inode, datasync));    

    return errno = ENOSYS, -1;
}


ssize_t vfs_read (inode_t* inode, void* buf, off_t off, size_t size) {
    
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(size);

    if(likely(inode->ops.read)) {

        __lock_return(&inode->lock, ssize_t, ({

            ssize_t e = inode->ops.read(inode, buf, off, size);

            if(inode->ev.events & POLLOUT) {

                inode->ev.revents |= POLLOUT;
                inode->ev.futex   |= 1;

            }

            e;

        }));

    }

    return errno = ENOSYS, -1;

}


ssize_t vfs_write (inode_t* inode, const void* buf, off_t off, size_t size) {
    
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(size);


    if(likely(inode->ops.write)) {

        __lock_return(&inode->lock, ssize_t, ({

            ssize_t e = inode->ops.write(inode, buf, off, size);

            if(inode->ev.events & POLLIN) {

                inode->ev.revents |= POLLIN;
                inode->ev.futex   |= 1;

            }

            e;

        }));

    }

    return errno = ENOSYS, -1;

}


ssize_t vfs_readlink (inode_t* inode, char* buf, size_t size) {
    
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(size);

    if(likely(inode->ops.readlink))
        __lock_return(&inode->lock, ssize_t, inode->ops.readlink(inode, buf, size));
    
    return errno = ENOSYS, -1;

}


inode_t* vfs_creat (inode_t* inode, const char* name, mode_t mode) {
    
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(name);

    if(name[0] == '.' && name[1] == '\0')
        return errno = EEXIST, NULL;

    if(name[0] == '.' && name[1] == '.' && name[2] == '\0')
        return errno = EEXIST, NULL;



    if(likely(inode->ops.creat)) {

        __lock_return(&inode->lock, inode_t*, ({

            inode_t* r = NULL;
            
            if((r = inode->ops.creat(inode, name, mode)) != NULL) {

                if(likely(r->parent == inode))
                    vfs_dcache_add(inode, r);

            }

            r;
        
        }));

    }


    return errno = ENOSYS, NULL;
}


inode_t* vfs_finddir (inode_t* inode, const char * name) {
    
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(name);


    if(name[0] == '.' && name[1] == '\0')
        return inode;


    if(name[0] == '.' && name[1] == '.' && name[2] == '\0') {
       
        if(current_task->fs->root != inode || inode->parent)
            return inode->parent;
        else
            return errno = ENOENT, NULL;

    }


    
    inode_t* r = NULL;

    if((r = vfs_dcache_find(inode, name)) != NULL)
        return r;


    if(likely(inode->ops.finddir)) {

        __lock(&inode->lock, {
            
            if((r = inode->ops.finddir(inode, name)) != NULL) {

                if(likely(r->parent == inode))
                    vfs_dcache_add(inode, r);
        
            }

        });

        return r;

    }
    
    return errno = ENOSYS, NULL;

}


ssize_t vfs_readdir (inode_t* inode, struct dirent* ent, off_t off, size_t size) {
    
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(ent);
    DEBUG_ASSERT(size);

    if(likely(inode->ops.readdir))
        __lock_return(&inode->lock, ssize_t, inode->ops.readdir(inode, ent, off, size));
    
    return errno = ENOSYS, -1;

}


int vfs_rename (inode_t* inode, const char* name, const char* newname) {
   
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(name);
    DEBUG_ASSERT(newname);


    if(name[0] == '.' && name[1] == '\0')
        return errno = EINVAL, -1;

    if(name[0] == '.' && name[1] == '.' && name[2] == '\0')
        return errno = EINVAL, -1;

    if(newname[0] == '.' && newname[1] == '\0')
        return errno = EEXIST, -1;

    if(newname[0] == '.' && newname[1] == '.' && newname[2] == '\0')
        return errno = EEXIST, -1;



    if(likely(inode->ops.rename))
        __lock_return(&inode->lock, int, inode->ops.rename(inode, name, newname));    

    return errno = EROFS, -1;

}


int vfs_symlink (inode_t* inode, const char* name, const char* target) {
    
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(name);
    DEBUG_ASSERT(target);


    if(name[0] == '.' && name[1] == '\0')
        return errno = EEXIST, -1;

    if(name[0] == '.' && name[1] == '.' && name[2] == '\0')
        return errno = EEXIST, -1;



    if(likely(inode->ops.symlink))
        __lock_return(&inode->lock, int, inode->ops.symlink(inode, name, target));

    return errno = EROFS, -1;

}


int vfs_unlink (inode_t* inode, const char* name) {
    
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(name);


    if(name[0] == '.' && name[1] == '\0')
        return errno = ENOTEMPTY, -1;

    if(name[0] == '.' && name[1] == '.' && name[2] == '\0')
        return errno = ENOTEMPTY, -1;



    if(likely(inode->ops.unlink)) {

        int r = -1;

        __lock(&inode->lock, {
            
            if((r = inode->ops.unlink(inode, name)) == 0)
                vfs_dcache_remove(inode, vfs_dcache_find(inode, name));
        
        });

        return r;
    
    }
    
    return errno = EROFS, -1;
}
