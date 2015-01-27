#ifdef __i386__

#include <aplus.h>

#include "grub.h"
#include "i386.h"


BootInfo* mbd_grub;


int load_bootargs() {

	mbd->memory.size = (mbd_grub->mem_lower + mbd_grub->mem_upper) * 1024;
	mbd->memory.pagesize = 4096;
	mbd->memory.start = ((uint32_t*) mbd_grub->mods_addr) [1];

	mbd->ramdisk.ptr = ((uint32_t*) mbd_grub->mods_addr) [0];
	mbd->ramdisk.size = ((uint32_t*) mbd_grub->mods_addr) [1] - ((uint32_t*) mbd_grub->mods_addr) [0];
	
	mbd->lfb.width = mbd_grub->vbe_mode_info->Xres;
	mbd->lfb.height = mbd_grub->vbe_mode_info->Yres;
	mbd->lfb.depth = mbd_grub->vbe_mode_info->bpp;
	mbd->lfb.pitch = mbd_grub->vbe_mode_info->pitch;
	mbd->lfb.base = mbd_grub->vbe_mode_info->physbase;
	mbd->lfb.size = mbd->lfb.pitch * mbd->lfb.height;

	mbd->cmdline.args = (char*) mbd_grub->cmdline;
	mbd->cmdline.length = 0;

	mbd->exec.num = mbd_grub->num;
	mbd->exec.addr = mbd_grub->addr;
	mbd->exec.size = mbd_grub->size;
	mbd->exec.shndx = mbd_grub->shndx;

	return 0;
}

#endif
