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


#include <aplus.h>
#include "grub.h"


BootInfo* mbd_grub;


int load_bootargs() {

    mbd->flags = mbd_grub->flags;
    
    mbd->memory.size = (mbd_grub->mem_lower + mbd_grub->mem_upper) * 1024;
    mbd->memory.pagesize = 4096;

    mbd->modules.ptr = (void*) mbd_grub->mods_addr;
    mbd->modules.count = mbd_grub->mods_count;

    mbd->mmap.ptr = (void*) mbd_grub->mmap_addr;
    mbd->mmap.count = mbd_grub->mmap_length / sizeof(*mbd->mmap.ptr);
    
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

    if(mbd->exec.addr)
        mbd->exec.addr += CONFIG_KERNEL_BASE;

    if(mbd->lfb.base) {
        
    }

    return 0;
}
