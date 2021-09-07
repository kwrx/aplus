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

#include <stdint.h>
#include <string.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/module.h>
#include <aplus/memory.h>
#include <aplus/smp.h>
#include <aplus/hal.h>
#include <aplus/fb.h>
#include <aplus/errno.h>
#include <aplus/endian.h>

#include <dev/interface.h>
#include <dev/video.h>
#include <dev/pci.h>

#include <dev/virtio/virtio.h>


MODULE_NAME("virtio/virtio-pci");
MODULE_DEPS("dev/interface,dev/pci");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");



static int virtio_pci_init_common_cfg(struct virtio_driver* driver, uint8_t bar, uintptr_t offset, size_t size) {

    DEBUG_ASSERT(driver);
    DEBUG_ASSERT(driver->device);
    DEBUG_ASSERT(offset);
    DEBUG_ASSERT(size);
    DEBUG_ASSERT(bar >= 0 && bar <= 5);


    uintptr_t mmio = 0UL;

    switch(bar) {

        case 0:
            mmio = pci_read(driver->device, PCI_BAR0, 4);
            break;

        case 1:
            mmio = pci_read(driver->device, PCI_BAR1, 4);
            break;

        case 2:
            mmio = pci_read(driver->device, PCI_BAR2, 4);
            break;

        case 3:
            mmio = pci_read(driver->device, PCI_BAR3, 4);
            break;

        case 4:
            mmio = pci_read(driver->device, PCI_BAR4, 4);
            break;

        case 5:
            mmio = pci_read(driver->device, PCI_BAR5, 4);
            break;

        default:
#if defined(DEBUG) && DEBUG_LEVEL >= 0
            kprintf("virtio-pci: FAIL! unknown bar #%d for device %d\n", bar, driver->device);
#endif
            return -EINVAL;

    }

    DEBUG_ASSERT(mmio);


    mmio += offset;

    arch_vmm_map(&core->bsp.address_space, mmio, mmio, size,
            ARCH_VMM_MAP_FIXED  | 
            ARCH_VMM_MAP_RDWR   |
            ARCH_VMM_MAP_NOEXEC
    );


    struct virtio_pci_common_cfg volatile* cfg = (struct virtio_pci_common_cfg volatile*) (mmio);


    // Device initialization
    // @see https://docs.oasis-open.org/virtio/virtio/v1.1/virtio-v1.1.pdf (chap.3)


    cfg->device_status = VIRTIO_DEVICE_STATUS_RESET;

    while(__atomic_load_n(&cfg->device_status, __ATOMIC_CONSUME) != 0)
        ;

    cfg->device_status |= VIRTIO_DEVICE_STATUS_ACKNOWNLEDGE;
    cfg->device_status |= VIRTIO_DEVICE_STATUS_DRIVER;


#if defined(DEBUG) && DEBUG_LEVEL >= 2
    kprintf("virtio-pci: device %d initialization successful [features(%X), queues(%d)]\n", driver->device, cfg->device_feature, cfg->num_queues);
#endif


    if(driver->negotiate) {

        uint32_t features = cpu_to_le32(driver->negotiate(driver, le32_to_cpu(cfg->device_feature)));

        cfg->device_feature_select = 0;
        cfg->device_feature = features;

        if(cfg->device_feature != features) {

#if defined(DEBUG) && DEBUG_LEVEL >= 0
            kprintf("virtio-pci: FAIL! device %d failed features negotation [requested(%X), supported(%X)]\n", cfg->device_feature, features);
#endif

            return cfg->device_status = VIRTIO_DEVICE_STATUS_FAILED, -ENOSYS;
            
        }

    }

    cfg->device_status |= VIRTIO_DEVICE_STATUS_FEATURES_OK;

#if defined(DEBUG) && DEBUG_LEVEL >= 4
    kprintf("virtio-pci: device %d negotation successful [features(%X)]\n", driver->device, cfg->device_feature);
#endif


    /* TODO: Initialize queues */

    return 0;

}


int virtio_pci_init(struct virtio_driver* driver) {

    DEBUG_ASSERT(driver);
    DEBUG_ASSERT(driver->device);


    uintptr_t caps;
    if((caps = pci_find_capabilities(driver->device)) == PCI_NONE) {

#if defined(DEBUG)
        kprintf("virtio-pci: FAIL! cannot find capabilities for pci device %d\n", driver->device);
#endif
     
        return -EINVAL;

    }


    pci_enable_pio(driver->device);
    pci_enable_mmio(driver->device);
    pci_enable_bus_mastering(driver->device);


    struct virtio_pci_cap cap;

    do {

        pci_memcpy(driver->device, &cap, caps, sizeof(struct virtio_pci_cap));

        if(cap.cap_vndr != VIRTIO_PCI_CAP_VENDOR)
            continue;


        cap.offset = le32_to_cpu(cap.offset);
        cap.length = le32_to_cpu(cap.length);


        int e;

        switch(cap.cfg_type) {

            case VIRTIO_PCI_CAP_COMMON_CFG:

                if((e = virtio_pci_init_common_cfg(driver, cap.bar, cap.offset, cap.length)) < 0)
                    return e;

                break;


            default:

#if defined(DEBUG) && DEBUG_LEVEL >= 2
                kprintf("virtio-pci: WARN! found unknown configuration type %d [offset(%p)]\n", cap.cfg_type, caps);
#endif
                break;

        }

    } while((caps = cap.cap_next) != 0);

    return 0;

}



void init(const char* args) {

    if(args && strstr(args, "virtio=disable"))
        return;

}

void dnit(void) {

}