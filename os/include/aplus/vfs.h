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

    inode_t* (*finddir) (inode_t*, const char*);
    inode_t* (*mknod) (inode_t*, const char*, mode_t);
    int (*getdents) (inode_t*, struct dirent*, off_t, size_t);
    int (*unlink) (inode_t*, const char*);

    int (*ioctl) (inode_t*, int, void*);
    int (*fsync) (inode_t*);
};

struct inode {
    char name[MAXNAMLEN];

    struct stat st;
    struct inode_ops ops;
    
    spinlock_t lock;

    union {
        struct {
            const char* type;
            inode_t* dev;
            int flags;
            void* userdata;

            struct statvfs st;
            struct inode_ops ops;
        } mount;

        struct inode* link;
    };


    void* userdata;
    struct inode* parent;
    struct inode* root;
};


void vfs_init(void);

int vfs_mount(inode_t* dev, inode_t* dir, const char __user * fs, int flags, const char __user * args);
int vfs_open(inode_t* inode);
int vfs_close(inode_t* inode);
int vfs_read(inode_t* inode, void* buf, off_t off, size_t size);
int vfs_write(inode_t* inode, const void* buf, off_t off, size_t size);
inode_t* vfs_finddir(inode_t* inode, const char* name);
inode_t* vfs_mknod(inode_t* inode, const char* name, mode_t mode);
int vfs_getdents(inode_t* inode, struct dirent* ent, off_t off, size_t size);
int vfs_unlink(inode_t* inode, const char* name);
int vfs_ioctl(inode_t* inode, int req, void* arg);
int vfs_fsync(inode_t* inode);


extern inode_t* vfs_root;
extern inode_t* vfs_dev;


/* From Linux sources */
#define ADFS_SUPER_MAGIC                0xadf5
#define AFFS_SUPER_MAGIC                0xadff
#define AFS_SUPER_MAGIC                 0x5346414F
#define AUTOFS_SUPER_MAGIC              0x0187
#define CODA_SUPER_MAGIC                0x73757245
#define CRAMFS_MAGIC                    0x28cd3d45
#define CRAMFS_MAGIC_WEND               0x453dcd28              
#define DEBUGFS_MAGIC                   0x64626720
#define SECURITYFS_MAGIC                0x73636673
#define SELINUX_MAGIC                   0xf97cff8c
#define SMACK_MAGIC                     0x43415d53
#define RAMFS_MAGIC                     0x858458f6
#define TMPFS_MAGIC                     0x01021994
#define HUGETLBFS_MAGIC                 0x958458f6
#define SQUASHFS_MAGIC                  0x73717368
#define ECRYPTFS_SUPER_MAGIC            0xf15f
#define EFS_SUPER_MAGIC                 0x414A53
#define EXT2_SUPER_MAGIC                0xEF53
#define EXT3_SUPER_MAGIC                0xEF53
#define XENFS_SUPER_MAGIC               0xabba1974
#define EXT4_SUPER_MAGIC                0xEF53
#define BTRFS_SUPER_MAGIC               0x9123683E
#define NILFS_SUPER_MAGIC               0x3434
#define F2FS_SUPER_MAGIC                0xF2F52010
#define HPFS_SUPER_MAGIC                0xf995e849
#define ISOFS_SUPER_MAGIC               0x9660
#define JFFS2_SUPER_MAGIC               0x72b6
#define PSTOREFS_MAGIC                  0x6165676C
#define EFIVARFS_MAGIC                  0xde5e81e4
#define HOSTFS_SUPER_MAGIC              0x00c0ffee
#define OVERLAYFS_SUPER_MAGIC           0x794c7630

#define MINIX_SUPER_MAGIC               0x137F
#define MINIX_SUPER_MAGIC2              0x138F
#define MINIX2_SUPER_MAGIC              0x2468
#define MINIX2_SUPER_MAGIC2             0x2478
#define MINIX3_SUPER_MAGIC              0x4d5a

#define MSDOS_SUPER_MAGIC               0x4d44
#define NCP_SUPER_MAGIC                 0x564c
#define NFS_SUPER_MAGIC                 0x6969
#define OCFS2_SUPER_MAGIC               0x7461636f
#define OPENPROM_SUPER_MAGIC            0x9fa1
#define QNX4_SUPER_MAGIC                0x002f
#define QNX6_SUPER_MAGIC                0x68191122
#define AFS_FS_MAGIC                    0x6B414653

#define REISERFS_SUPER_MAGIC                0x52654973
#define REISERFS_SUPER_MAGIC_STRING         "ReIsErFs"
#define REISER2FS_SUPER_MAGIC_STRING        "ReIsEr2Fs"
#define REISER2FS_JR_SUPER_MAGIC_STRING     "ReIsEr3Fs"

#define SMB_SUPER_MAGIC                 0x517B
#define TRACEFS_MAGIC                   0x74726163
#define V9FS_MAGIC                      0x01021997

#define BDEVFS_MAGIC                    0x62646576
#define DAXFS_MAGIC                     0x64646178
#define BINFMTFS_MAGIC                  0x42494e4d
#define DEVPTS_SUPER_MAGIC              0x1cd1
#define FUTEXFS_SUPER_MAGIC             0xBAD1DEA
#define PIPEFS_MAGIC                    0x50495045
#define PROC_SUPER_MAGIC                0x9fa0
#define SOCKFS_MAGIC                    0x534F434B
#define SYSFS_MAGIC                     0x62656572
#define USBDEVICE_SUPER_MAGIC           0x9fa2
#define MTD_INODE_FS_MAGIC              0x11307854
#define ANON_INODE_FS_MAGIC             0x09041934
#define BTRFS_TEST_MAGIC                0x73727279
#define NSFS_MAGIC                      0x6e736673
#define BPF_FS_MAGIC                    0xcafe4a11
#define AAFS_MAGIC                      0x5a3c69f0
#define UDF_SUPER_MAGIC                 0x15013346
#define BALLOON_KVM_MAGIC               0x13661366
#define ZSMALLOC_MAGIC                  0x58295829

#endif