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
#include <aplus/debug.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <aplus/ipc.h>
#include <aplus/intr.h>
#include <aplus/timer.h>
#include <aplus/mm.h>
#include <aplus/mmio.h>
#include <libc.h>

MODULE_NAME("pc/audio/hda");
MODULE_DEPS("arch/x86");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


#if defined(__i386__) || defined(__x86_64__)
#    if defined(__i386__)
#        include <arch/i386/i386.h>
#        include <arch/i386/pci.h>
#    elif defined(__x86_64__)
#        include <arch/x86_64/x86_64.h>
#        include <arch/x86_64/pci.h>
#    endif



static uint32_t hda_devices[] = {
    0x8086, 0x2668,
    0x0000, 0x0000
};



static void find_hda_pci(uint32_t device, uint16_t vendorid, uint16_t deviceid, void* arg) {
    int i;
    for(i = 0; hda_devices[i]; i += 2)
        if(vendorid == hda_devices[i] && deviceid == hda_devices[i + 1])
            *((uintptr_t*) arg) = device;
}

int init(void) {
    uintptr_t hda_pci = 0;
    pci_scan(&find_hda_pci, -1, &hda_pci);

    if(unlikely(!hda_pci)) {
        kprintf(ERROR "hda: no pci device found!\n");
        return -1;
    }



    vfs_mkdev("snd", -1, S_IFCHR | 0444);
    vfs_mkdev("mixer", -1, S_IFCHR | 0444);

    kprintf(WARN "hda: Intel HDA not yet supported! it doesn't work\n");
    return 0;
}

#else

int init(void) {
    return 0;
}

#endif


int dnit(void) {
    return 0;
}