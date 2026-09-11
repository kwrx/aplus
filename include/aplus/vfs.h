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


    #define VFS_MAX_FILESYSTEMS  32

    #define FSID_TMPFS   0xDEAD1000
    #define FSID_EXT2    0xDEAD1001
    #define FSID_VFAT    0xDEAD1002
    #define FSID_ISO9660 0xDEAD1003
    #define FSID_PROCFS  0xDEAD1004


    #define INODE_FLAGS_DCACHE_DISABLED 0x00000001

    //? No directory entry and no other owner: the last struct file referencing
    //? this inode is responsible for freeing it (see fd_remove()).
    #define INODE_FLAGS_ANONYMOUS 0x00000002


    //? Which half of a pipe an endpoint inode represents. A named FIFO uses a
    //? single inode for both directions.
    #define PIPE_END_READ  0
    #define PIPE_END_WRITE 1
    #define PIPE_END_BOTH  2


    #define MODE_2_DIRENT_TYPE(mode) ((mode & S_IFMT) >> 12)


typedef struct inode inode_t;


struct inode_ops {

    inode_t* (*open)(inode_t*, int);
    int (*close)(inode_t*);
    int (*ioctl)(inode_t*, long, void*);

    //? Report which of the requested events are ready right now. Leaving this
    //? NULL means "always ready", which is the correct answer for anything
    //? that cannot block -- regular files and directories included.
    int (*poll)(inode_t*, int);

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


//? Readiness itself is not cached here: it is asked for on demand through
//? ops.poll(), so data that arrived before anyone was watching still counts.
//? All this carries is a monotonic "something about this inode changed"
//? counter, which waiters snapshot and sleep until it differs. Bumping it is
//? unconditional -- gating the bump on a pre-declared interest mask is what
//? used to lose wakeups and let a poller sleep on top of a full buffer.

struct inode_events {
    volatile uint32_t futex;
};

//? shared_ptr() expands to a fresh anonymous struct every time it is written,
//? so two separate uses are unrelated types. Naming it once here lets an inode
//? and the channel behind it hold references to the same counter.
typedef shared_ptr(struct inode_events) inode_events_t;

struct inode {

    char name[CONFIG_MAXNAMLEN];

    ino_t ino;
    int flags;

    struct superblock* sb;
    struct inode_ops ops;
    struct inode* parent;

    void* userdata;
    spinlock_t lock;

    inode_events_t ev;

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

    atomic_int status;
    atomic_int refcount;

    spinlock_t lock;
};

typedef struct {

    int id;
    const char* name;
    bool nodev;

    int (*mount)(inode_t*, inode_t*, int, const char*);
    int (*umount)(inode_t*);

} fstable_t;

__BEGIN_DECLS


void vfs_init(void);

//* kernel/fs/vfs.c
extern fstable_t* fs_table;
int vfs_mount(inode_t* dev, inode_t* dir, const char* fs, int flags, const char* args);

inode_t* vfs_open(inode_t*, int);
int vfs_close(inode_t*);
int vfs_ioctl(inode_t*, long, void*);
int vfs_poll(inode_t*, int);
void vfs_notify(inode_t*);

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
int pipefs_create_pair(inode_t**, inode_t**, size_t);
inode_t* fifofs_open(inode_t*, int);
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
