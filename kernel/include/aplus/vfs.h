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


#ifndef _VFS_H
#define _VFS_H

#include <aplus.h>
#include <aplus/base.h>
#include <aplus/utils/list.h>
#include <aplus/ipc.h>
#include <libc.h>


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

#ifndef __ASSEMBLY__



typedef int64_t off64_t;
typedef int64_t ino64_t;

typedef struct inode inode_t;



struct mountinfo {
    char fstype[64];
    int flags;
    inode_t* dev;
    struct statvfs stat;
};

typedef struct fsys {
    uint32_t id;
    const char* name;
    int (*mount) (struct inode*, struct inode*, struct mountinfo* info);
} fsys_t;

typedef struct mountpoint {
    ino64_t id;
    inode_t* root;
    struct mountinfo* info;
    struct mountpoint* parent;
} mountpoint_t;


struct iorequest {
    inode_t* inode;
    off_t offset;
    void* buffer;
    size_t size;

    int (*fn) (inode_t*, void*, size_t);
    spinlock_t lock;
    int ioerr;
};

struct inode_childs {
    inode_t* inode;
    struct inode_childs* next;
};


struct inode {
    char* name;
    dev_t dev;
    ino64_t ino;
    mode_t mode;
    nlink_t nlink;
    uid_t uid;
    gid_t gid;
    dev_t rdev;
    off64_t size;

    time_t atime;
    time_t mtime;
    time_t ctime;
    
    struct mountinfo* mtinfo;
    void* userdata;
    int dirty;

    int (*open) (struct inode* inode);
    int (*close) (struct inode* inode);

    int (*read) (struct inode* inode, void* ptr, off_t pos, size_t len);
    int (*write) (struct inode* inode, void* ptr, off_t pos, size_t len);
    
    
    struct inode* (*finddir) (struct inode* inode, char* name);
    struct inode* (*mknod) (struct inode* inode, char* name, mode_t mode);
    int (*unlink) (struct inode* inode, char* name);
    
    int (*chown) (struct inode* inode, uid_t owner, gid_t group);
    int (*chmod) (struct inode* inode, mode_t mode);
    
    int (*ioctl) (struct inode* inode, int req, void* buf);
    int (*fsync) (struct inode* inode);

#if CONFIG_IOSCHED
    list(struct iorequest*, ioqueue);
    spinlock_t iolock;
#endif

    struct inode_childs* childs;

    struct inode* parent;
    struct inode* link;
};


extern list(fsys_t*, fsys_queue);
extern list(mountpoint_t*, mnt_queue); 
extern inode_t* devfs;
extern inode_t* sysfs;
extern inode_t* vfs_root;

#ifdef _WITH_MNTFLAGS
static struct {
    char* option;
    int value;
} mnt_flags[] = {
    { "defaults", 0 },
    { "rw", 0 },
    { "ro", MS_RDONLY },
    { "readonly", MS_RDONLY },
    { "nosuid", MS_NOSUID },
    { "nodev", MS_NODEV },
    { "noexec", MS_NOEXEC },
    { "synchronous", MS_SYNCHRONOUS },
    { "remount", MS_REMOUNT },
    { "mandlock", MS_MANDLOCK },
    { "dirsync", MS_DIRSYNC },
    { "noatime", MS_NOATIME },
    { "nodiratime", MS_NODIRATIME },
    { "bind", MS_BIND },
    { "move", MS_MOVE },
    { "recursive", MS_REC },
    { "silent", MS_SILENT },
    { "posixacl", MS_POSIXACL },
    { "unbindable", MS_UNBINDABLE },
    { "private", MS_PRIVATE },
    { "slave", MS_SLAVE },
    { "shared", MS_SHARED },
    { "relatime", MS_RELATIME },
    { "system", MS_KERNMOUNT },
    { "strictatime", MS_STRICTATIME },
    { "lazytime", MS_LAZYTIME },
    { "active", MS_ACTIVE },
    { "nouser", MS_NOUSER },
    { NULL, 0 }
};
#endif

int vfs_init(void);
ino64_t vfs_inode();

int vfs_open(struct inode* inode);
int vfs_close(struct inode* inode);
int vfs_read(struct inode* inode, void* ptr, off_t pos, size_t len);
int vfs_write(struct inode* inode, void* ptr, off_t pos, size_t len);
struct inode* vfs_finddir(struct inode* inode, char* name);
struct inode* vfs_mknod(struct inode* inode, char* name, mode_t mode);
int vfs_unlink(struct inode* inode, char* name);
int vfs_rename(struct inode* inode, char* newname);
int vfs_chown(struct inode* inode, uid_t owner, gid_t group);
int vfs_chmod(struct inode* inode, mode_t mode);
int vfs_ioctl(struct inode* inode, int req, void* buf);
int vfs_fsync(struct inode* inode);

int vfs_mount(struct inode* dev, struct inode* dir, const char* fstype, int flags);
int vfs_umount(struct inode* dir);

fsys_t* vfs_fsys_find(const char* name);
int vfs_fsys_register(uint32_t id, const char* name, int (*mount) (struct inode*, struct inode*, struct mountinfo*));

inode_t* vfs_mkdev(const char* name, dev_t rdev, mode_t mode);

#if CONFIG_IOSCHED
int iosched_read(struct inode*, void*, off_t, size_t);
int iosched_write(struct inode*, void*, off_t, size_t);
int iosched_init(void);
#endif

#endif

#endif
