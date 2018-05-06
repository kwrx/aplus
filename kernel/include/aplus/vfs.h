#ifndef _VFS_H
#define _VFS_H

#include <aplus.h>
#include <aplus/base.h>
#include <aplus/utils/list.h>
#include <aplus/ipc.h>
#include <libc.h>

#ifndef __ASSEMBLY__



typedef int64_t off64_t;
typedef int64_t ino64_t;

typedef struct inode inode_t;



struct mountinfo {
    char fstype[64];
    int flags;
    inode_t* dev;
};

typedef struct fsys {
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
    
    int (*rename) (struct inode* inode, char* newname);
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
int vfs_fsys_register(const char* name, int (*mount) (struct inode*, struct inode*, struct mountinfo*));

inode_t* vfs_mkdev(const char* name, dev_t rdev, mode_t mode);

#if CONFIG_IOSCHED
int iosched_read(struct inode*, void*, off_t, size_t);
int iosched_write(struct inode*, void*, off_t, size_t);
int iosched_init(void);
#endif

#endif

#endif
