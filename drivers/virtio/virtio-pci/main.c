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


static void virtio_pci_interrupt(pcidev_t device, irq_t vector, struct virtio_driver* driver) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(driver);
    DEBUG_ASSERT(driver->internals.isr_status);    


    uint32_t isr = *driver->internals.isr_status;


    if(isr & VIRTIO_ISR_STATUS_QUEUE) {

#if defined(CONFIG_HAVE_PCI_MSIX)

        virtq_flush(driver, vector);

#else
        size_t i;
        for(i = 0; i < driver->internals.num_queues; i++)
            virtq_flush(driver, i);

#endif

    }


    if(isr & VIRTIO_ISR_STATUS_CONFIG) {

        // TODO...
        kprintf("virtio-pci: WARN! received isr status config!\n");

    }


    if(likely(driver->interrupt)) {
        driver->interrupt(device, vector, driver);
    }

}

static uintptr_t virtio_pci_find_bar(struct virtio_driver* driver, uint8_t bar, uintptr_t offset) {

    DEBUG_ASSERT(driver);
    DEBUG_ASSERT(driver->device);
    DEBUG_ASSERT(bar >= 0 && bar <= 5);


    uintptr_t mmio = 0UL;
    uintptr_t size = 0UL;

#if defined(__x86_64__) || defined(__aarch64__)
    if(pci_is_64bit(driver->device, PCI_BAR(bar)))
        mmio = pci_read(driver->device, PCI_BAR(bar), 8) & PCI_BAR_MM_MASK;
    else
#endif
        mmio = pci_read(driver->device, PCI_BAR(bar), 4) & PCI_BAR_MM_MASK;


    size = pci_bar_size(driver->device, PCI_BAR(bar), 4);


    DEBUG_ASSERT(mmio);
    DEBUG_ASSERT(size);


    if((driver->internals.bars & (1 << bar)) == 0) {

#if DEBUG_LEVEL_TRACE
        kprintf("virtio-pci: device %d is mapping bar %d [address(%p), size(%p)]\n", driver->device, bar, mmio, size);
#endif

        arch_vmm_map(&core->bsp.address_space, mmio, mmio, size,
                ARCH_VMM_MAP_FIXED      | 
                ARCH_VMM_MAP_RDWR       |
                ARCH_VMM_MAP_UNCACHED   |
                ARCH_VMM_MAP_NOEXEC
        );

        driver->internals.bars |= (1 << bar);

    }


    return mmio + offset;

}




static int virtio_pci_init_common_cfg(struct virtio_driver* driver, uint8_t bar, uintptr_t offset) {

    DEBUG_ASSERT(driver);
    DEBUG_ASSERT(driver->device);
    DEBUG_ASSERT(bar >= 0 && bar <= 5);




#if DEBUG_LEVEL_ERROR
    #define cfg_set_status_and_check(status) {                                      \
        __atomic_or_fetch(&cfg->device_status, status, __ATOMIC_SEQ_CST);           \
        if((cfg->device_status & status) == 0) {                                    \
            kprintf("virtio-pci: FAIL! device %d setting status %s [status(%X)]\n", \
                driver->device, #status, cfg->device_status);                       \
            return cfg->device_status = VIRTIO_DEVICE_STATUS_FAILED, -ENOSYS;       \
        }                                                                           \
    }
#else
    #define cfg_set_status_and_check(status) {                                      \
        __atomic_or_fetch(&cfg->device_status, status, __ATOMIC_SEQ_CST);           \
        if((cfg->device_status & status) == 0)                                      \
            return cfg->device_status = VIRTIO_DEVICE_STATUS_FAILED, -ENOSYS;       \
    }
#endif



    struct virtio_pci_common_cfg volatile* cfg = (struct virtio_pci_common_cfg volatile*) virtio_pci_find_bar(driver, bar, offset);


    //
    // Device initialization
    // @see https://docs.oasis-open.org/virtio/virtio/v1.1/virtio-v1.1.pdf (chap.3)
    //

    cfg->device_status = VIRTIO_DEVICE_STATUS_RESET;

    while(__atomic_load_n(&cfg->device_status, __ATOMIC_CONSUME) != VIRTIO_DEVICE_STATUS_RESET)
        ;

    cfg_set_status_and_check(VIRTIO_DEVICE_STATUS_ACKNOWNLEDGE);
    cfg_set_status_and_check(VIRTIO_DEVICE_STATUS_DRIVER);


#if DEBUG_LEVEL_TRACE
    kprintf("virtio-pci: device %d reset successful [bar(%d), cfg(%p), queues(%d)]\n", driver->device, bar, cfg, cfg->num_queues);
#endif


    if(driver->negotiate) {

        for(size_t i = 0; i < 2; i++) {

            cfg->device_feature_select = i;
            cfg->driver_feature_select = i;

            __atomic_membarrier();


            uint32_t features = le32_to_cpu(cfg->device_feature);

            int e;
            if((e = driver->negotiate(driver, &features, i)) < 0)
                return e;

            cfg->driver_feature = cpu_to_le32(features);

            __atomic_membarrier();


            if(cfg->driver_feature != features) {

    #if DEBUG_LEVEL_FATAL
                kprintf("virtio-pci: FAIL! device %d failed features %d negotiation [user(%X), driver(%X), device(%X)]\n", driver->device, features, i, cfg->driver_feature, cfg->device_feature);
    #endif

                return cfg->device_status = VIRTIO_DEVICE_STATUS_FAILED, -ENOSYS;
                
            }

#if DEBUG_LEVEL_TRACE
            kprintf("virtio-pci: device %d negotiation %d successful [driver(%X), device(%X)]\n", driver->device, i, cfg->driver_feature, cfg->device_feature);
#endif


        }

    }

    cfg_set_status_and_check(VIRTIO_DEVICE_STATUS_FEATURES_OK);



    //
    // Interrupts Handler
    //


#if defined(CONFIG_HAVE_PCI_MSIX)

    pci_msix_t msix;

    if(pci_find_msix(driver->device, &msix) == PCI_NONE) {

#if DEBUG_LEVEL_FATAL
        kprintf("virtio-pci: ERROR! device %d MSI-X capabilities not found!\n", driver->device);
#endif

        return cfg->device_status = VIRTIO_DEVICE_STATUS_FAILED, -ENOSYS;

    }


    int i;
    for(i = 0; i < cfg->num_queues; i++) {

        pci_msix_map_irq(driver->device, &msix, (pci_irq_handler_t) virtio_pci_interrupt, (pci_irq_data_t) driver, i);
        pci_msix_unmask(driver->device, &msix, i);

    }

    cfg->config_msix_vector = i;

    pci_msix_map_irq(driver->device, &msix, (pci_irq_handler_t) virtio_pci_interrupt, (pci_irq_data_t) driver, i);
    pci_msix_unmask(driver->device, &msix, i);
    pci_msix_enable(driver->device, &msix);


#if DEBUG_LEVEL_TRACE
    kprintf("virtio-pci: device %d MSI-X capabilities found [caps(%p), rows(%p), count(%d), config_msix_vector(%p)]\n", driver->device, msix.msix_cap, msix.msix_rows, msix.msix_pci.pci_msgctl_table_size, cfg->config_msix_vector);
#endif

#else

    driver->internals.irq = pci_read(driver->device, PCI_INTERRUPT_LINE, 1);

    if(driver->internals.irq != PCI_INTERRUPT_LINE_NONE) {

        pci_intx_map_irq(driver->device, driver->internals.irq, (pci_irq_handler_t) virtio_pci_interrupt, (pci_irq_data_t) driver);
        pci_intx_unmask(driver->device);
    
    }

#endif






    //
    // Queue
    //

    for(size_t i = 0; i < cfg->num_queues; i++) {

        if(virtq_init(driver, cfg, i) < 0) {

#if DEBUG_LEVEL_FATAL
            kprintf("virtio-pci: FAIL! device %d queue %d initialization failed\n", driver->device, i);
#endif 

            return cfg->device_status = VIRTIO_DEVICE_STATUS_FAILED, -ENOSYS;

        }

    }

    driver->internals.num_queues = cfg->num_queues;



    cfg_set_status_and_check(VIRTIO_DEVICE_STATUS_DRIVER_OK);


#if DEBUG_LEVEL_TRACE
    kprintf("virtio-pci: device %d common initialization successful\n", driver->device);
#endif

    return 0;

}



static int virtio_pci_init_device_cfg(struct virtio_driver* driver, uintptr_t caps, uint8_t bar, uintptr_t offset) {

    DEBUG_ASSERT(driver);
    DEBUG_ASSERT(driver->device);
    DEBUG_ASSERT(caps);
    DEBUG_ASSERT(bar >= 0 && bar <= 5);


    driver->internals.device_config = virtio_pci_find_bar(driver, bar, offset);


    int e = 0;

    if(unlikely(!driver->setup))
        return e;

    if((e = driver->setup(driver, driver->internals.device_config)) < 0)
        return e;



#if DEBUG_LEVEL_TRACE
    kprintf("virtio-pci: device %d obtaining device config successful [caps(%d), bar(%d), offset(%p)]\n", driver->device, caps, bar, offset);
#endif


    return 0;

}



static int virtio_pci_init_isr_status(struct virtio_driver* driver, uint8_t bar, uintptr_t offset) {

    DEBUG_ASSERT(driver);
    DEBUG_ASSERT(driver->device);
    DEBUG_ASSERT(offset);
    DEBUG_ASSERT(bar >= 0 && bar <= 5);


    driver->internals.isr_status = (uint32_t volatile*) virtio_pci_find_bar(driver, bar, offset);

    
#if DEBUG_LEVEL_TRACE
    kprintf("virtio-pci: device %d obtaining isr status successful [bar(%d), offset(%d), isr(%p)]\n", driver->device, bar, offset, driver->internals.isr_status);
#endif

    return 0;

}





static int virtio_pci_init_notify_cfg(struct virtio_driver* driver, uintptr_t caps, uint8_t bar, uintptr_t offset) {

    DEBUG_ASSERT(driver);
    DEBUG_ASSERT(driver->device);
    DEBUG_ASSERT(caps);


    struct virtio_pci_notify_cfg cfg;
    pci_memcpy(driver->device, &cfg, caps + sizeof(struct virtio_pci_cap), sizeof(struct virtio_pci_notify_cfg));


    if(unlikely(cfg.notify_off_multiplier == 0)) {
#if DEBUG_LEVEL_FATAL
        kprintf("virtio-pci: FAIL! device %d has null notify_off_multiplier [caps(%d)]", driver->device, caps);
#endif
        return errno = EINVAL, -1;
    }


    driver->internals.notify_off_multiplier = le32_to_cpu(cfg.notify_off_multiplier);
    driver->internals.notify_offset = virtio_pci_find_bar(driver, bar, offset);


#if DEBUG_LEVEL_TRACE
    kprintf("virtio-pci: device %d obtaining notify_off_multiplier successful [caps(%d), multiplier(%d)]\n", driver->device, caps, driver->internals.notify_off_multiplier);
#endif

    return 0;

}





int virtio_pci_init(struct virtio_driver* driver) {

    DEBUG_ASSERT(driver);
    DEBUG_ASSERT(driver->device);


    uintptr_t caps;
    if((caps = pci_find_capabilities(driver->device)) == PCI_NONE) {

#if DEBUG_LEVEL_FATAL
        kprintf("virtio-pci: FAIL! cannot find capabilities for pci device %d\n", driver->device);
#endif
     
        return errno = EINVAL, -1;

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

                if((e = virtio_pci_init_common_cfg(driver, cap.bar, cap.offset)) < 0)
                    return e;

                break;


            case VIRTIO_PCI_CAP_DEVICE_CFG:

                if((e = virtio_pci_init_device_cfg(driver, caps, cap.bar, cap.offset)) < 0)
                    return e;

                break;


            case VIRTIO_PCI_CAP_ISR_CFG:

                if((e = virtio_pci_init_isr_status(driver, cap.bar, cap.offset)) < 0)
                    return e;

                break;

            
            case VIRTIO_PCI_CAP_NOTIFY_CFG:

                if((e = virtio_pci_init_notify_cfg(driver, caps, cap.bar, cap.offset)) < 0)
                    return e;

                break;


            case VIRTIO_PCI_CAP_PCI_CFG:

                break;


            default:

#if DEBUG_LEVEL_WARN
                kprintf("virtio-pci: WARN! found unknown configuration type %d [offset(%p)]\n", cap.cfg_type, caps);
#endif
                break;

        }

    } while((caps = cap.cap_next) != 0);


#if DEBUG_LEVEL_TRACE
    kprintf("virtio-pci: device %d pci initialization successful [irq(%d)]\n", driver->device, driver->internals.irq);
#endif


    return 0;

}



void init(const char* args) {

    if(args && strstr(args, "virtio=disable"))
        return;

}

void dnit(void) {

}