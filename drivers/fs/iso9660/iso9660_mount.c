#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <libc.h>

#include "iso9660.h"

int iso9660_mount(struct inode* dev, struct inode* dir, struct mountinfo* info) {

    if(unlikely(!dev || !dir))
        return -1;


    iso9660_t* ctx = (iso9660_t*) kmalloc(sizeof(iso9660_t), GFP_USER);
    
    KASSERT(ctx);
    memset(ctx, 0, sizeof(iso9660_t));
    
    if(unlikely(vfs_read(dev, &ctx->pvd, ISO9660_PVD, ISO9660_VOLDESC_SIZE) != ISO9660_VOLDESC_SIZE)) {
        kprintf(ERROR "iso9660: (%s) cannot read from this device\n", dev->name);
        kfree(ctx);
        return -1;
    }

    if(strncmp(ctx->pvd.id, ISO9660_ID, 5) != 0) {
        kprintf(ERROR "iso9660: (%s) invalid iso9660 ID\n", dev->name);

        kfree(ctx);
        return -1;
    }


    ctx->dev = dev;
    memcpy(&ctx->dir, &ctx->pvd.rootdir, sizeof(ctx->pvd.rootdir));

    dir->open = iso9660_open;
    dir->close = iso9660_close;
    dir->finddir = iso9660_finddir;
    dir->unlink = iso9660_unlink;

    dir->userdata = (void*) ctx;
    dir->mtinfo = info;


    info->stat.f_bsize =
    info->stat.f_frsize = ctx->pvd.logical_blksize.lsb;
    info->stat.f_blocks = ctx->pvd.volsize.lsb;
    info->stat.f_bfree = 0;
    info->stat.f_bavail = 0;
    info->stat.f_files = 0;
    info->stat.f_ffree = 0;
    info->stat.f_favail = 0;
    info->stat.f_namemax = ISO9660_NAME_LENGTH;

    return 0;
}
