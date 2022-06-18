/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2019 Antonino Natale
 * 
 * 
 * This file is part of aplus.
 * 
 * aplus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * aplus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with aplus.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <string.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <aplus/hal.h>
#include <aplus/errno.h>
#include <aplus/endian.h>

#include <dev/interface.h>
#include <dev/pci.h>



#define PCI_MSIX_DEVICES_MAX                128
#define PCI_MSIX_INTR_BASE                  65


static struct {
    pcidev_t device;
    pci_irq_handler_t handler;
    pci_irq_data_t data;
    uint16_t index;
} pci_msix_devices[PCI_MSIX_DEVICES_MAX] = { 0 };

static spinlock_t pci_msix_lock = SPINLOCK_INIT_WITH_FLAGS(SPINLOCK_FLAGS_CPU_OWNER);



static void pci_msix_interrupt_handler(void* frame, irq_t irq) {

    DEBUG_ASSERT(irq > PCI_MSIX_INTR_BASE - 1);
    DEBUG_ASSERT(irq < PCI_MSIX_INTR_BASE + PCI_MSIX_DEVICES_MAX);

    
    irq -= PCI_MSIX_INTR_BASE;

    if(!pci_msix_devices[irq].device)
        return;

    if(!pci_msix_devices[irq].handler)
        return;


    pci_msix_devices[irq].handler(pci_msix_devices[irq].device, pci_msix_devices[irq].index, pci_msix_devices[irq].data);

}




int pci_find_msix(pcidev_t device, pci_msix_t* mptr) {

    DEBUG_ASSERT(device);


    uintptr_t caps;
    if((caps = pci_find_capabilities(device)) == PCI_NONE)
        return PCI_NONE;


    pci_msix_t msix;
    
    do {
        
        pci_memcpy(device, &msix.msix_pci, caps, sizeof(pci_msix_t));

        if(msix.msix_pci.pci_capid != PCI_MSIX_CAPID)
            continue;



        if(msix.msix_pci.pci_bir > 5) {
#if defined(DEBUG) && DEBUG_LEVEL >= 0
                kprintf("pci-msix: FAIL! unknown pci bar #%d for device %d\n", msix.msix_pci.pci_bir, device);
#endif
                return PCI_NONE;
        }


        
        uintptr_t mmio = 0UL;


#if defined(__x86_64__) || defined(__aarch64__)
        if(pci_is_64bit(device, PCI_BAR(msix.msix_pci.pci_bir)))
            mmio = pci_read(device, PCI_BAR(msix.msix_pci.pci_bir), 8) & PCI_BAR_MM_MASK;
        else
#endif
            mmio = pci_read(device, PCI_BAR(msix.msix_pci.pci_bir), 4) & PCI_BAR_MM_MASK;


        DEBUG_ASSERT(mmio);


        mmio += msix.msix_pci.pci_offset;

        arch_vmm_map(&core->bsp.address_space, mmio, mmio, msix.msix_pci.pci_msgctl_table_size * sizeof(pci_msix_row_t),
                ARCH_VMM_MAP_FIXED      | 
                ARCH_VMM_MAP_RDWR       |
                ARCH_VMM_MAP_UNCACHED   |
                ARCH_VMM_MAP_NOEXEC
        );


        msix.msix_cap  = caps;
        msix.msix_rows = (pci_msix_row_t volatile*) mmio;



        // Mask all interrupts

        uint32_t i;
        for(i = 0; i < msix.msix_pci.pci_msgctl_table_size + 1; i++)
            pci_msix_mask(device, &msix, i);



        if(mptr)
            memcpy(mptr, &msix, sizeof(pci_msix_t));

        return 0;


    } while((caps = msix.msix_pci.pci_capnext) != 0);


    return PCI_NONE;

}



void pci_msix_enable(pcidev_t device, pci_msix_t* msix) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(msix);
    DEBUG_ASSERT(msix->msix_cap);

#if defined(DEBUG)
    DEBUG_ASSERT(pci_read(device, msix->msix_cap, 1) == PCI_MSIX_CAPID);
#endif

    return pci_write(device, msix->msix_cap + 2, 2, pci_read(device, msix->msix_cap + 2, 2) | PCI_MSIX_ENABLE);

}

void pci_msix_disable(pcidev_t device, pci_msix_t* msix) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(msix);
    DEBUG_ASSERT(msix->msix_cap);

#if defined(DEBUG)
    DEBUG_ASSERT(pci_read(device, msix->msix_cap, 1) == PCI_MSIX_CAPID);
#endif

    return pci_write(device, msix->msix_cap + 2, 2, pci_read(device, msix->msix_cap + 2, 2) & ~PCI_MSIX_ENABLE);

}


void pci_msix_mask(pcidev_t device, pci_msix_t* msix, uint32_t index) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(msix);
    DEBUG_ASSERT(msix->msix_rows);

    msix->msix_rows[index].pr_ctl |= PCI_MSIX_INTR_MASK;

    __atomic_membarrier();

}

void pci_msix_unmask(pcidev_t device, pci_msix_t* msix, uint32_t index) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(msix);
    DEBUG_ASSERT(msix->msix_rows);

    msix->msix_rows[index].pr_ctl &= ~PCI_MSIX_INTR_MASK;

    __atomic_membarrier();

}



int pci_msix_map_irq(pcidev_t device, pci_msix_t* msix, pci_irq_handler_t handler, pci_irq_data_t data, uint16_t index) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(handler);
    DEBUG_ASSERT(msix);
    DEBUG_ASSERT(msix->msix_rows);
    DEBUG_ASSERT(msix->msix_pci.pci_msgctl_table_size >= index);


    spinlock_lock(&pci_msix_lock);

    size_t i;
    for(i = 0; i < PCI_MSIX_DEVICES_MAX; i++) {

        if(pci_msix_devices[i].device)
            continue;

        if(pci_msix_devices[i].handler)
            continue;
        

        pci_msix_devices[i].index   = index;
        pci_msix_devices[i].device  = device;
        pci_msix_devices[i].handler = handler;
        pci_msix_devices[i].data    = data;

        arch_intr_map_irq_without_ioapic(i + PCI_MSIX_INTR_BASE, pci_msix_interrupt_handler);


        msix->msix_rows[index].pr_address   = cpu_to_le64(0xFEE00000 | (current_cpu->id << 12) | (1 << 14));
        msix->msix_rows[index].pr_data      = cpu_to_le32(i + PCI_MSIX_INTR_BASE);
        msix->msix_rows[index].pr_ctl       = cpu_to_le32(le32_to_cpu(msix->msix_rows[index].pr_ctl) | PCI_MSIX_INTR_MASK);

        __atomic_membarrier();


#if defined(DEBUG) && DEBUG_LEVEL >= 4
        kprintf("pci-msix: slot %d mapped for device %d [index(%p), handler(%p), data(%p)]\n", i, device, index, handler, data);
#endif

        spinlock_unlock(&pci_msix_lock);
        return 0;
    }


    spinlock_unlock(&pci_msix_lock);


#if defined(DEBUG) && DEBUG_LEVEL >= 0
    kprintf("pci-msix: ERROR! No more device slots available for device %d [index(%p), handler(%p), data(%p)]\n", device, index, handler, data);
#endif

    return -ENOSPC;

}


int pci_msix_unmap_irq(pcidev_t device, pci_msix_t* msix) {

    spinlock_lock(&pci_msix_lock);


    size_t i;
    for(i = 0; i < PCI_MSIX_DEVICES_MAX; i++) {

        if(pci_msix_devices[i].device != device)
            continue;


        msix->msix_rows[pci_msix_devices[i].index].pr_address   = 0;
        msix->msix_rows[pci_msix_devices[i].index].pr_data      = 0;
        msix->msix_rows[pci_msix_devices[i].index].pr_ctl       = cpu_to_le32(le32_to_cpu(msix->msix_rows[pci_msix_devices[i].index].pr_ctl) | PCI_MSIX_INTR_MASK);

        __atomic_membarrier();


        pci_msix_devices[i].index   = 0;
        pci_msix_devices[i].device  = 0;
        pci_msix_devices[i].handler = NULL;
        pci_msix_devices[i].data    = NULL;


        spinlock_unlock(&pci_msix_lock);
        return 0;

    }


    spinlock_unlock(&pci_msix_lock);


#if defined(DEBUG) && DEBUG_LEVEL >= 0
    kprintf("pci-msix: ERROR! No device slot found for device %d\n", device);
#endif

    return -ESRCH;

}


