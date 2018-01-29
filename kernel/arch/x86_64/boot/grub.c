
#include <aplus.h>
#include "grub.h"


BootInfo* mbd_grub;


int load_bootargs() {

    mbd->memory.size = (mbd_grub->mem_lower + mbd_grub->mem_upper) * 1024;
    mbd->memory.pagesize = 4096;

    mbd->modules.ptr = (void*) mbd_grub->mods_addr;
    mbd->modules.count = mbd_grub->mods_count;
    
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


    mbd->memory.start = (mbd->modules.ptr[mbd->modules.count - 1].size + 0x4000) & ~0xFFF;
    
    int i;
    for(i = 0; i < mbd->modules.count; i++) {
        mbd->modules.ptr[i].size -= mbd->modules.ptr[i].ptr;
        mbd->modules.ptr[i].ptr += CONFIG_KERNEL_BASE;
        mbd->modules.ptr[i].cmdline += CONFIG_KERNEL_BASE;
    }


    if(mbd->lfb.base) {
        
    }

    return 0;
}
