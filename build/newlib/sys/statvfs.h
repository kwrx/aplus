#ifndef _SYS_STATFS_H
#define _SYS_STATFS_H 1

#include <sys/features.h>
#include <sys/types.h>

#define ST_RDONLY           1
#define ST_NOSUID           2
#define ST_NODEV            4
#define ST_NOEXEC           8
#define ST_SYNCHRONOUS      16
#define ST_MANDLOCK         64
#define ST_WRITE            128
#define ST_APPEND           256
#define ST_IMMUTABLE        512
#define ST_NOATIME          1024
#define ST_NODIRATIME       2048
#define ST_RELATIME         4096

struct statvfs {
    unsigned long int f_bsize;
    unsigned long int f_frsize;

    fsblkcnt_t f_blocks;
    fsblkcnt_t f_bfree;
    fsblkcnt_t f_bavail;

    fsfilcnt_t f_files;
    fsfilcnt_t f_ffree;
    fsfilcnt_t f_favail;

    unsigned long int f_fsid;
    unsigned long int f_flag;
    unsigned long int f_namemax;
    int __f_spare[6];
};

__BEGIN_DECLS

extern int statvfs(const char* file, struct statvfs* buf)
    __THROW __nonnull((1, 2));

extern int fstatvfs(int fd, struct statvfs* buf)
    __THROW __nonnull((2));


__END_DECLS

#endif