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


#include <aplus.h>
#include <arch/x86/multiboot.h>
#include <stdint.h>
#include <string.h>


__section(".data")
multiboot_info_t* mbd_raw;



void multiboot_init(void) {
    extern int end;

    memset(mbd, 0, sizeof(mbd_t));
    mbd->memory.pagesize = 4096;
    mbd->memory.start = (uintptr_t) &end + 0x400000;
    mbd->cpu.family = "unknown";

    if(unlikely(!mbd_raw))
        return;


    mbd->flags = mbd_raw->flags;

    #define __HAS(i) \
        (mbd->flags & (i))


    if(__HAS(MULTIBOOT_INFO_MEMORY))
        mbd->memory.size = (mbd_raw->mem_lower + mbd_raw->mem_upper) * 1024;
    
    if(__HAS(MULTIBOOT_INFO_CMDLINE)) {
        mbd->cmdline = (const char*) (CONFIG_KERNEL_BASE + mbd_raw->cmdline);

        if(strstr(mbd->cmdline, "quiet"))
            mbd->quiet = 1;
    }

    if(__HAS(MULTIBOOT_INFO_MODS)) {
        mbd->modules.ptr = (void*) (CONFIG_KERNEL_BASE + mbd_raw->mods_addr);
        mbd->modules.count = mbd_raw->mods_count;

        int i;
        for(i = 0; i < mbd->modules.count; i++) {
            mbd->modules.ptr[i].size -= mbd->modules.ptr[i].ptr;
            mbd->modules.ptr[i].ptr += CONFIG_KERNEL_BASE;
            mbd->modules.ptr[i].cmdline += CONFIG_KERNEL_BASE;
        }

        mbd->memory.start = (
            mbd->modules.ptr[mbd->modules.count - 1].ptr +
            mbd->modules.ptr[mbd->modules.count - 1].size +
            0x1000) & ~0xFFF
        ;
    }

    if(__HAS(MULTIBOOT_INFO_MEM_MAP)) {
        mbd->mmap.ptr = (void*) (CONFIG_KERNEL_BASE + mbd_raw->mmap_addr);
        mbd->mmap.count = mbd_raw->mmap_length / sizeof(*mbd->mmap.ptr);
    }

    if(__HAS(MULTIBOOT_INFO_FRAMEBUFFER_INFO)) {
        mbd->fb.type = mbd_raw->framebuffer_type;
        mbd->fb.base = mbd_raw->framebuffer_addr;
        mbd->fb.width = mbd_raw->framebuffer_width;
        mbd->fb.height = mbd_raw->framebuffer_height;
        mbd->fb.depth = mbd_raw->framebuffer_bpp;
        mbd->fb.pitch = mbd_raw->framebuffer_pitch;
        mbd->fb.size = mbd->fb.pitch * mbd->fb.height;
    }

    if(__HAS(MULTIBOOT_INFO_ELF_SHDR)) {
        mbd->exec.num = mbd_raw->u.elf_sec.num;
        mbd->exec.addr = CONFIG_KERNEL_BASE + mbd_raw->u.elf_sec.addr;
        mbd->exec.size = mbd_raw->u.elf_sec.size;
        mbd->exec.shndx = mbd_raw->u.elf_sec.shndx;
    }
}