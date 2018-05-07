#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <aplus/sysconfig.h>
#include <libc.h>

#include "tmpfs.h"

int tmpfs_mount(struct inode* dev, struct inode* dir, struct mountinfo* info) {
    (void) dev;


    dir->open = NULL;
    dir->close = NULL;
    dir->unlink = tmpfs_unlink;
    dir->mknod = tmpfs_mknod;
    dir->finddir = NULL;
    dir->mtinfo = info;


    uint32_t size = (uint32_t) sysconfig("tmpfs.size", 0x1000000);
    uint32_t files = (uint32_t) sysconfig("tmpfs.files", 0x100000);
 
    size = size ? size : 0x1000000;
    files = files ? files : 0x100000;



    info->stat.f_bsize = 1;
    info->stat.f_frsize = 1;
    info->stat.f_blocks =
    info->stat.f_bfree =
    info->stat.f_bavail = size;
    info->stat.f_files =
    info->stat.f_ffree =
    info->stat.f_favail = files;
    info->stat.f_namemax = UINT32_MAX;


    return 0;
}
