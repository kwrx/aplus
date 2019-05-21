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

#ifndef _APLUS_VFS_H
#define _APLUS_VFS_H

#include <aplus.h>
#include <aplus/ipc.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/dirent.h>

typedef struct inode inode_t;

struct inode_ops {
    int (*open)(inode_t*);
    int (*close)(inode_t*);
    int (*read) (inode_t*, void*, off_t, size_t);
    int (*write) (inode_t*, const void*, off_t, size_t);

    inode_t* (*finddir) (inode_t*, char* name);
    inode_t* (*mknod) (inode_t*, char* name, mode_t mode);
    int (*getdents) (inode_t*, struct dirent*, off_t, size_t);
    int (*unlink) (inode_t*, char* name);

    int (*ioctl) (inode_t*, int req, void* arg);
    int (*fsync) (inode_t*);
};

struct inode {
    char name[BUFSIZ];

    struct stat st;
    struct inode_ops ops;
    
    spinlock_t lock;

    union {
        struct {
            const char* type;
            inode_t* dev;
            int flags;

            struct statvfs st;
            struct inode_ops ops;
        } mount;

        struct inode* link;
    };


    void* userdata;
    struct inode* parent;
};


void vfs_init(void);

int vfs_open(inode_t* inode);
int vfs_close(inode_t* inode);
int vfs_read(inode_t* inode, void* buf, off_t off, size_t size);
int vfs_write(inode_t* inode, const void* buf, off_t off, size_t size);
inode_t* vfs_finddir(inode_t* inode, char* name);
inode_t* vfs_mknod(inode_t* inode, char* name, mode_t mode);
int vfs_getdents(inode_t* inode, struct dirent* ent, off_t off, size_t size);
int vfs_unlink(inode_t* inode, char* name);
int vfs_ioctl(inode_t* inode, int req, void* arg);
int vfs_fsync(inode_t* inode);


extern inode_t* vfs_root;
#endif