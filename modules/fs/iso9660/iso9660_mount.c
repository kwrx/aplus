#include <xdev.h>
#include <xdev/debug.h>
#include <xdev/vfs.h>
#include <xdev/mm.h>
#include <libc.h>

#include "iso9660.h"

int iso9660_mount(struct inode* dev, struct inode* dir) {

	if(unlikely(!dev || !dir))
		return E_ERR;


	iso9660_t* ctx = (iso9660_t*) kmalloc(sizeof(iso9660_t), GFP_USER);
	
	KASSERT(ctx);
	memset(ctx, 0, sizeof(iso9660_t));
	
	dev->position = ISO9660_PVD;
	if(unlikely(vfs_read(dev, &ctx->pvd, ISO9660_VOLDESC_SIZE) != ISO9660_VOLDESC_SIZE)) {
		kprintf(ERROR, "iso9660: (%s) cannot read from this device\n", dev->name);
		kfree(ctx);
		return E_ERR;
	}

	if(strncmp(ctx->pvd.id, ISO9660_ID, 5) != 0) {
		kprintf(ERROR, "iso9660: (%s) invalid iso9660 ID\n", dev->name);
		kfree(ctx);
		return E_ERR;
	}


	ctx->dev = dev;
	memcpy(&ctx->dir, &ctx->pvd.rootdir, sizeof(ctx->pvd.rootdir));

	dir->open = iso9660_open;
	dir->close = iso9660_close;
	dir->finddir = iso9660_finddir;

	dir->userdata = (void*) ctx;

	return E_OK;
}
