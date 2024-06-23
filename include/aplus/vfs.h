/*
 * Author(s):
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

#ifndef _APLUS_VFS_H
#define _APLUS_VFS_H

#ifndef __ASSEMBLY__

    #include <sched.h>

    #include <dirent.h>
    #include <signal.h>
    #include <stdint.h>
    #include <time.h>

    #include <sys/resource.h>
    #include <sys/stat.h>
    #include <sys/types.h>

    #include <sys/statvfs.h>

    #include <aplus.h>
    #include <aplus/debug.h>
    #include <aplus/ipc.h>

    #include <aplus/utils/cache.h>
    #include <aplus/utils/hashmap.h>
    #include <aplus/utils/list.h>
    #include <aplus/utils/ptr.h>



    #define FSID_TMPFS   0xDEAD1000
    #define FSID_EXT2    0xDEAD1001
    #define FSID_VFAT    0xDEAD1002
    #define FSID_ISO9660 0xDEAD1003
    #define FSID_PROCFS  0xDEAD1004


    #define INODE_FLAGS_DCACHE_DISABLED 0x00000001


    #define MODE_2_DIRENT_TYPE(mode) ((mode & S_IFMT) >> 12)


typedef struct inode inode_t;


struct inode_ops {

        inode_t* (*open)(inode_t*, int);
        int (*close)(inode_t*);
        int (*ioctl)(inode_t*, long, void*);

        /* Inode */
        int (*getattr)(inode_t*, struct stat*);
        int (*setattr)(inode_t*, struct stat*);

        int (*setxattr)(inode_t*, const char*, const void*, size_t, int);
        int (*getxattr)(inode_t*, const char*, void*, size_t);
        int (*listxattr)(inode_t*, char*, size_t);
        int (*removexattr)(inode_t*, const char*);

        int (*truncate)(inode_t*, off_t);
        int (*fsync)(inode_t*, int);

        ssize_t (*read)(inode_t*, void*, off_t, size_t);
        ssize_t (*write)(inode_t*, const void*, off_t, size_t);
        ssize_t (*readlink)(inode_t*, char*, size_t);


        /* Directory */
        inode_t* (*creat)(inode_t*, const char*, mode_t);
        inode_t* (*finddir)(inode_t*, const char*);
        ssize_t (*readdir)(inode_t*, struct dirent*, off_t, size_t);

        int (*rename)(inode_t*, const char*, const char*);
        int (*symlink)(inode_t*, const char*, const char*);
        int (*unlink)(inode_t*, const char*);
};


struct inode_events {
        uint16_t events;
        uint16_t revents;
        volatile uint32_t futex;
};

struct inode {

        char name[CONFIG_MAXNAMLEN];

        ino_t ino;
        int flags;

        struct superblock* sb;
        struct inode_ops ops;
        struct inode* parent;

        void* userdata;
        spinlock_t lock;

        shared_ptr(struct inode_events) ev;

        HASHMAP(char, inode_t) dcache;
};


struct superblock {

        id_t fsid;
        int flags;

        inode_t* dev;
        inode_t* root;

        ino_t ino;
        struct statvfs st;
        struct inode_ops ops;
        struct cache cache;

        void* fsinfo;
};


struct file {

        inode_t* inode;
        off_t position;

        int status;
        int refcount;

        spinlock_t lock;
};



__BEGIN_DECLS


void vfs_init(void);

//* os/kernel/fs/vfs.c
int vfs_mount(inode_t* dev, inode_t* dir, const char* fs, int flags, const char* args);

inode_t* vfs_open(inode_t*, int);
int vfs_close(inode_t*);
int vfs_ioctl(inode_t*, long, void*);

int vfs_getattr(inode_t*, struct stat*);
int vfs_setattr(inode_t*, struct stat*);

int vfs_setxattr(inode_t*, const char*, const void*, size_t, int);
int vfs_getxattr(inode_t*, const char*, void*, size_t);
int vfs_listxattr(inode_t*, char*, size_t);
int vfs_removexattr(inode_t*, const char*);

int vfs_truncate(inode_t*, off_t);
int vfs_fsync(inode_t*, int);

ssize_t vfs_read(inode_t*, void*, off_t, size_t);
ssize_t vfs_write(inode_t*, const void*, off_t, size_t);
ssize_t vfs_readlink(inode_t*, char*, size_t);

inode_t* vfs_creat(inode_t*, const char*, mode_t);
inode_t* vfs_finddir(inode_t*, const char*);
ssize_t vfs_readdir(inode_t*, struct dirent*, off_t, size_t);

int vfs_rename(inode_t*, const char*, const char*);
int vfs_symlink(inode_t*, const char*, const char*);
int vfs_unlink(inode_t*, const char*);


// kernel/fs/pipefs.c
inode_t* pipefs_inode(void);
inode_t* vfs_mkfifo(inode_t*, size_t, int);


// kernel/fs/dcache.c
void vfs_dcache_init(inode_t*);
void vfs_dcache_free(inode_t*);
void vfs_dcache_add(inode_t*, inode_t*);
void vfs_dcache_remove(inode_t*, inode_t*);
inode_t* vfs_dcache_find(inode_t*, const char*);


// kernel/fs/rootfs.c
extern inode_t* vfs_root;
void rootfs_init(void);


// kernel/fs/fd.c
void fd_init(void);
void fd_ref(struct file*);
void fd_remove(struct file*, bool);
struct file* fd_append(inode_t*, off_t, int);


// kernel/fs/path.c
inode_t* path_follows(inode_t*);
inode_t* path_lookup(inode_t*, const char*, int, mode_t);


__END_DECLS

#endif
#endif
