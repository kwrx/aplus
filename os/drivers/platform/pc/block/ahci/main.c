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
#include <aplus/debug.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <aplus/intr.h>
#include <aplus/mm.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <dev/interface.h>
#include <dev/block.h>
#include <dev/pci.h>

#include <arch/x86/cpu.h>


MODULE_NAME("block/ahci");
MODULE_DEPS("dev/interface,dev/block,dev/pci");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


#define AHCI_HBA_CAPS               0x00
#define AHCI_HBA_GHC                0x04
#define AHCI_HBA_IS                 0x08
#define AHCI_HBA_PI                 0x0C
#define AHCI_HBA_VS                 0x10
#define AHCI_HBA_CCC_CTL            0x14
#define AHCI_HBA_CCC_PTS            0x18
#define AHCI_HBA_EM_LOC             0x1C
#define AHCI_HBA_EM_CTL             0x20
#define AHCI_HBA_CAPS_EXT           0x24
#define AHCI_HBA_BOHC               0x28

#define AHCI_HBA_PORT(i)            \
    (0x100 + (i * 0x80))


#define AHCI_HBA_PORT_CLB           0x00
#define AHCI_HBA_PORT_CLBU          0x04
#define AHCI_HBA_PORT_FB            0x08
#define AHCI_HBA_PORT_FBU           0x0C
#define AHCI_HBA_PORT_IS            0x10
#define AHCI_HBA_PORT_IE            0x14
#define AHCI_HBA_PORT_CMD           0x18
#define AHCI_HBA_PORT_RSV           0x1C
#define AHCI_HBA_PORT_TFD           0x20
#define AHCI_HBA_PORT_SIG           0x24
#define AHCI_HBA_PORT_SSTS          0x28
#define AHCI_HBA_PORT_SCTL          0x2C
#define AHCI_HBA_PORT_SERR          0x30
#define AHCI_HBA_PORT_SACT          0x34
#define AHCI_HBA_PORT_CI            0x38
#define AHCI_HBA_PORT_SNTF          0x3C
#define AHCI_HBA_PORT_FBS           0x40

#define ACHI_MMIO_SIZE              0x1100



struct {
    uint32_t vendorid;
    uint32_t deviceid;

    uintptr_t mmio;
} sata = { };


static void pci_find(pcidev_t device, uint16_t vid, uint16_t did, void* arg) {

    if((vid != 0x8086) || (did != 0x2922))
        return;

    sata.deviceid = did;
    sata.vendorid = vid;

    sata.mmio = pci_read(device, PCI_BAR5, 4) & PCI_BAR_MM_MASK;

}



void init(const char* args) {
    
    pci_scan(&pci_find, 0x0106, NULL);

    if(!sata.deviceid)
        return;

    
    arch_mmap (
        (void*) sata.mmio, ACHI_MMIO_SIZE,
        ARCH_MAP_NOEXEC |
        ARCH_MAP_FIXED  |
        ARCH_MAP_RDWR   |
        ARCH_MAP_UNCACHED
    );


    int p = mmio_r32(sata.mmio + AHCI_HBA_PI);

    int i;
    for(i = 0; i < 32; i++) {
        if(!(p & (1 << i)))
            continue;


        int s = mmio_r32(sata.mmio + AHCI_HBA_PORT(i) + AHCI_HBA_PORT_SSTS);

        if((s >> 8) & 0xF != 3)
            continue;

        if((s & 0xF) != 1)
            continue;


        kprintf("found %d device: %p %p\n", i,
            mmio_r32(sata.mmio + AHCI_HBA_PORT(i) + AHCI_HBA_PORT_SSTS),
            mmio_r32(sata.mmio + AHCI_HBA_PORT(i) + AHCI_HBA_PORT_SIG)
        );
    }

}

void dnit(void) {

}