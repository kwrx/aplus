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

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/endian.h>
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/memory.h>
#include <aplus/module.h>
#include <aplus/smp.h>

#include <dev/interface.h>
#include <dev/pci.h>

#include <dev/virtio/virtio.h>


MODULE_NAME("virtio/virtio-pci");
MODULE_DEPS("dev/interface,dev/pci");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


/* A capability list long enough to reach this is a malformed or cyclic one. */
#define VIRTIO_PCI_MAX_CAPABILITIES 64

/* How long a device is given to come back from a reset. */
#define VIRTIO_PCI_RESET_TIMEOUT_MS 1000


/* Where one virtio capability lives, gathered on a first pass over the list. */

struct virtio_pci_location {

    bool found;

    uint8_t bar;
    uintptr_t offset;
    uintptr_t length;

    /* Offset of the capability structure itself in configuration space, for the few
       capabilities that carry more than the header. */
    uintptr_t address;
};


static void virtio_pci_interrupt(pcidev_t device, irq_t irq, struct virtio_driver* driver, uint16_t vector) {

#if defined(CONFIG_HAVE_PCI_MSIX)

    DEBUG_ASSERT(vector <= driver->internals.msix_vectors);

    if (vector == driver->internals.msix_vectors) {
        // TODO: handle config interrupt
        kprintf("virtio-pci: WARN! received config interrupt!\n");
    } else if (likely(driver->internals.msix_vectors)) {
        /* A vector serves every queue that was given it, which is more than one whenever the
           device's table was too small to go round. */
        for (size_t i = vector; i < driver->internals.num_queues; i += driver->internals.msix_vectors)
            virtq_flush(driver, i);
    }

#else
    uint32_t isr = driver->internals.isr_status ? mmio_r32(driver->internals.isr_status) : 0;

    if (isr & VIRTIO_ISR_STATUS_QUEUE) {
        for (size_t i = 0; i < driver->internals.num_queues; i++)
            virtq_flush(driver, i);
    }

    if (isr & VIRTIO_ISR_STATUS_CONFIG) {
        // TODO: handle config interrupt
        kprintf("virtio-pci: WARN! received isr status config!\n");
    }
#endif

    if (likely(driver->interrupt)) {
        driver->interrupt(device, vector, driver);
    }
}


/* The size of a BAR, in the one form this driver needs it: whole pages.
 *
 * pci_bar_size() probes by writing all ones over the BAR and complementing what reads back,
 * and leaves the BAR's own read-only type bits in the answer -- a 16KiB aperture comes back
 * as 0x3FFC. It also cannot usefully be asked for the 64-bit form: passing 8 has it write
 * only the low dword and read both back, so the untouched high half is complemented into
 * nonsense and the result is a size of several exabytes.
 *
 * Neither matters for a virtio BAR, which is comfortably under 4GiB. Probe the low dword,
 * mask the type bits off the answer and round it out to the page the mapping works in. */

static uintptr_t virtio_pci_bar_size(struct virtio_driver* driver, uint8_t bar) {

    uintptr_t size = pci_bar_size(driver->device, PCI_BAR(bar), 4) & PCI_BAR_MM_MASK;

    if (unlikely(!size))
        return 0;

    return (size + (PML1_PAGESIZE - 1)) & ~((uintptr_t)PML1_PAGESIZE - 1);
}


static uintptr_t virtio_pci_find_bar(struct virtio_driver* driver, uint8_t bar, uintptr_t offset) {

    DEBUG_ASSERT(driver);
    DEBUG_ASSERT(driver->device);
    DEBUG_ASSERT(bar <= 5);


    uintptr_t mmio = 0UL;

#if defined(__x86_64__) || defined(__aarch64__)
    if (pci_is_64bit(driver->device, PCI_BAR(bar)))
        mmio = pci_read(driver->device, PCI_BAR(bar), 8) & PCI_BAR_64_MM_MASK;
    else
#endif
        mmio = pci_read(driver->device, PCI_BAR(bar), 4) & PCI_BAR_MM_MASK;

    uintptr_t size = virtio_pci_bar_size(driver, bar);


    if (unlikely(!mmio || !size)) {
#if DEBUG_LEVEL_ERROR
        kprintf("virtio-pci: ERROR! device %d bar %d is not usable [address(%p), size(%p)]\n", driver->device, bar, mmio, size);
#endif
        return 0;
    }


    if ((driver->internals.bars & (1 << bar)) == 0) {

#if DEBUG_LEVEL_TRACE
        kprintf("virtio-pci: device %d is mapping bar %d [address(%p), size(%p)]\n", driver->device, bar, mmio, size);
#endif

        if (arch_vmm_map(&core->bsp.address_space, mmio, mmio, size, ARCH_VMM_MAP_FIXED | ARCH_VMM_MAP_RDWR | ARCH_VMM_MAP_UNCACHED | ARCH_VMM_MAP_NOEXEC) == ARCH_VMM_MAP_FAILED) {

#if DEBUG_LEVEL_ERROR
            kprintf("virtio-pci: ERROR! failed to map bar %d [address(%p), size(%p)]\n", bar, mmio, size);
#endif
            return 0;
        }

        driver->internals.bars |= (1 << bar);
    }


    return mmio + offset;
}


//
// Device status.
//
// Driven with plain reads and writes rather than atomic_fetch_or(): that emitted a locked
// read-modify-write against device memory, which is not something a device is obliged to
// implement, and the status byte has exactly one writer anyway.
//

static uint8_t virtio_pci_get_status(struct virtio_driver* driver) {

    DEBUG_ASSERT(driver->internals.common_config);

    return mmio_r8(&driver->internals.common_config->device_status);
}


static void virtio_pci_set_status(struct virtio_driver* driver, uint8_t status) {

    DEBUG_ASSERT(driver->internals.common_config);

    mmio_w8(&driver->internals.common_config->device_status, status);

    atomic_thread_fence(memory_order_seq_cst);
}


/* Say that the driver has given up on the device.
 *
 * FAILED is added to the status rather than substituted for it: the specification asks a
 * driver to report that it gave up, not to pretend it never acknowledged the device, and
 * clearing ACKNOWLEDGE|DRIVER on the way out is indistinguishable from a reset. */

static void virtio_pci_set_failed(struct virtio_driver* driver) {

    if (likely(driver->internals.common_config))
        virtio_pci_set_status(driver, virtio_pci_get_status(driver) | VIRTIO_DEVICE_STATUS_FAILED);
}


static int virtio_pci_add_status(struct virtio_driver* driver, uint8_t status) {

    virtio_pci_set_status(driver, virtio_pci_get_status(driver) | status);

    if (likely((virtio_pci_get_status(driver) & status) == status))
        return 0;

#if DEBUG_LEVEL_FATAL
    kprintf("virtio-pci: FAIL! device %d refused status %X [status(%X)]\n", driver->device, status, virtio_pci_get_status(driver));
#endif

    virtio_pci_set_failed(driver);

    return errno = ENOSYS, -1;
}


static int virtio_pci_reset(struct virtio_driver* driver) {

    virtio_pci_set_status(driver, VIRTIO_DEVICE_STATUS_RESET);

    uint64_t deadline = arch_timer_generic_getms() + VIRTIO_PCI_RESET_TIMEOUT_MS;

    while (virtio_pci_get_status(driver) != VIRTIO_DEVICE_STATUS_RESET) {

        if (unlikely(arch_timer_generic_getms() >= deadline)) {
#if DEBUG_LEVEL_FATAL
            kprintf("virtio-pci: FAIL! device %d did not come back from a reset\n", driver->device);
#endif
            return errno = ETIMEDOUT, -1;
        }

        __cpu_pause();
    }

    return 0;
}


/* Agree on a feature set.
 *
 * The starting point is what both sides can do, not everything the device offers. Echoing
 * the device's word straight back accepts whatever it happens to advertise, and a feature
 * that is accepted but not implemented is how a driver ends up reading a ring the device is
 * writing in a layout it never agreed to -- RING_PACKED changes the ring outright,
 * EVENT_IDX moves where the notification threshold lives, IN_ORDER lets the device write
 * only the last used entry of a batch, and NOTIFICATION_DATA changes what a kick even is.
 */

static int virtio_pci_negotiate(struct virtio_driver* driver) {

    struct virtio_pci_common_cfg volatile* cfg = driver->internals.common_config;

    static const uint32_t supported[2] = {VIRTIO_FEATURES_MASK_0, VIRTIO_FEATURES_MASK_1};

    for (size_t i = 0; i < 2; i++) {

        mmio_w32(&cfg->device_feature_select, cpu_to_le32(i));
        mmio_w32(&cfg->driver_feature_select, cpu_to_le32(i));

        atomic_thread_fence(memory_order_seq_cst);


        uint32_t offered  = le32_to_cpu(mmio_r32(&cfg->device_feature));
        uint32_t features = offered & supported[i];

        int e;
        if (driver->negotiate && (e = driver->negotiate(driver, &features, i)) < 0) {
            virtio_pci_set_failed(driver);
            return e;
        }


        /* Nothing downstream reports this: QEMU masks the surplus away silently and hands
           the raw word back on a read, so the device comes up looking as though the feature
           had been agreed and behaves as though it had been refused. */

        if (unlikely(features & ~offered)) {
#if DEBUG_LEVEL_FATAL
            kprintf("virtio-pci: FAIL! device %d was asked for features %X of word %d which it does not offer [offered(%X)]\n", driver->device, features & ~offered, i, offered);
#endif
            virtio_pci_set_failed(driver);
            return errno = ENOTSUP, -1;
        }


        if (i == 1 && !(features & VIRTIO_F_VERSION_1)) {
#if DEBUG_LEVEL_FATAL
            kprintf("virtio-pci: FAIL! device %d is legacy only, this driver speaks the modern transport [offered(%X)]\n", driver->device, offered);
#endif
            virtio_pci_set_failed(driver);
            return errno = ENOTSUP, -1;
        }


        mmio_w32(&cfg->driver_feature, cpu_to_le32(features));

        atomic_thread_fence(memory_order_release);

#if DEBUG_LEVEL_TRACE
        kprintf("virtio-pci: device %d negotiation %d successful [offered(%X), accepted(%X)]\n", driver->device, i, offered, features);
#endif
    }

    return 0;
}


static int virtio_pci_init_interrupts(struct virtio_driver* driver) {

#if defined(CONFIG_HAVE_PCI_MSIX)

    pci_msix_t msix;

    if (pci_find_msix(driver->device, &msix) == PCI_NONE) {
    #if DEBUG_LEVEL_FATAL
        kprintf("virtio-pci: FAIL! device %d MSI-X capabilities not found!\n", driver->device);
    #endif
        return errno = ENOSYS, -1;
    }

    #if DEBUG_LEVEL_TRACE
    kprintf("virtio-pci: device %d MSI-X capabilities found [caps(%p), rows(%p), vectors(%d)]\n", driver->device, msix.msix_cap, msix.msix_rows, msix.msix_pci.pci_msgctl_table_size + 1);
    #endif


    // NOTE:
    // Mapping MSI-X vectors:
    //  vectors[0..(msix_vectors - 1)]  -> queues, shared round-robin
    //  vectors[msix_vectors]           -> config interrupt
    //
    // The table is not guaranteed to hold one vector per queue plus one for configuration:
    // QEMU gives a virtio-input device exactly two, however many queues it has, which is the
    // shared arrangement the device expects a driver to fall back to. Each distinct vector
    // is mapped exactly once -- pci_msix_map_irq() takes the next free table row rather than
    // the row named by its argument, so mapping one vector twice would burn two rows and
    // leave none for the configuration interrupt.

    uint16_t vector_limit = msix.msix_pci.pci_msgctl_table_size + 1;

    if (vector_limit < 2) {
    #if DEBUG_LEVEL_FATAL
        kprintf("virtio-pci: FAIL! device %d has an MSI-X table of %d, too small to serve a queue and the configuration\n", driver->device, vector_limit);
    #endif
        return errno = ENOSYS, -1;
    }

    driver->internals.msix_vectors = vector_limit - 1;

    if (driver->internals.msix_vectors > driver->internals.num_queues)
        driver->internals.msix_vectors = driver->internals.num_queues;

    if (unlikely(!driver->internals.msix_vectors)) {
    #if DEBUG_LEVEL_FATAL
        kprintf("virtio-pci: FAIL! device %d offers no queue to give a vector to\n", driver->device);
    #endif
        return errno = ENOSYS, -1;
    }


    for (uint16_t i = 0; i < driver->internals.msix_vectors; i++) {

        if (pci_msix_map_irq(driver->device, &msix, (pci_irq_handler_t)virtio_pci_interrupt, (pci_irq_data_t)driver, i) < 0) {
    #if DEBUG_LEVEL_FATAL
            kprintf("virtio-pci: FAIL! device %d mapping MSI-X vector %d for its queues failed\n", driver->device, i);
    #endif
            return errno = ENOSPC, -1;
        }

        pci_msix_unmask(driver->device, &msix, i);
    }

    if (pci_msix_map_irq(driver->device, &msix, (pci_irq_handler_t)virtio_pci_interrupt, (pci_irq_data_t)driver, driver->internals.msix_vectors) < 0) {
    #if DEBUG_LEVEL_FATAL
        kprintf("virtio-pci: FAIL! device %d mapping MSI-X vector %d for config failed\n", driver->device, driver->internals.msix_vectors);
    #endif
        return errno = ENOSPC, -1;
    }

    pci_msix_unmask(driver->device, &msix, driver->internals.msix_vectors);

    mmio_w16(&driver->internals.common_config->config_msix_vector, cpu_to_le16(driver->internals.msix_vectors));

    pci_msix_enable(driver->device, &msix);

#else

    #if DEBUG_LEVEL_WARN
    pci_msix_t msix;
    if (pci_find_msix(driver->device, &msix) != PCI_NONE) {
        kprintf("virtio-pci: WARN! device %d has MSI-X capabilities but the kernel was built without MSI-X support\n", driver->device);
        DEBUG_ASSERT(pci_msix_is_enabled(driver->device, &msix) == false);
    }
    #endif

    driver->internals.irq = pci_read(driver->device, PCI_INTERRUPT_LINE, 1);

    if (driver->internals.irq != PCI_INTERRUPT_LINE_NONE) {
        pci_intx_map_irq(driver->device, driver->internals.irq, (pci_irq_handler_t)virtio_pci_interrupt, (pci_irq_data_t)driver);
        pci_intx_unmask(driver->device);
    }

    mmio_w16(&driver->internals.common_config->config_msix_vector, cpu_to_le16(VIRTIO_MSI_NO_VECTOR));

#endif

    return 0;
}


/* Bring the device up as far as its queues.
 *
 * Everything up to and including populating the queues, but deliberately not DRIVER_OK: the
 * device specific setup that runs off the device configuration capability belongs between
 * the two, and it used to run before features had even been agreed. */

static int virtio_pci_init_common_cfg(struct virtio_driver* driver, struct virtio_pci_location* at) {

    DEBUG_ASSERT(driver);
    DEBUG_ASSERT(driver->device);
    DEBUG_ASSERT(at);


    uintptr_t address = virtio_pci_find_bar(driver, at->bar, at->offset);

    if (unlikely(!address))
        return errno = ENXIO, -1;

    driver->internals.common_config = (struct virtio_pci_common_cfg volatile*)address;


    //
    // Device initialization
    // @see https://docs.oasis-open.org/virtio/virtio/v1.1/virtio-v1.1.pdf (chap.3)
    //

    int e;

    if ((e = virtio_pci_reset(driver)) < 0)
        return e;

    if ((e = virtio_pci_add_status(driver, VIRTIO_DEVICE_STATUS_ACKNOWNLEDGE)) < 0)
        return e;

    if ((e = virtio_pci_add_status(driver, VIRTIO_DEVICE_STATUS_DRIVER)) < 0)
        return e;


#if DEBUG_LEVEL_TRACE
    kprintf("virtio-pci: device %d reset successful [bar(%d), cfg(%p), queues(%d)]\n", driver->device, at->bar, driver->internals.common_config, le16_to_cpu(mmio_r16(&driver->internals.common_config->num_queues)));
#endif


    if ((e = virtio_pci_negotiate(driver)) < 0)
        return e;

    if ((e = virtio_pci_add_status(driver, VIRTIO_DEVICE_STATUS_FEATURES_OK)) < 0)
        return e;


    driver->internals.num_queues = le16_to_cpu(mmio_r16(&driver->internals.common_config->num_queues));

    if (driver->internals.num_queues > VIRTQ_MAX_QUEUES)
        driver->internals.num_queues = VIRTQ_MAX_QUEUES;

    if (driver->max_queues && driver->internals.num_queues > driver->max_queues)
        driver->internals.num_queues = driver->max_queues;

    if (unlikely(!driver->internals.num_queues)) {
#if DEBUG_LEVEL_FATAL
        kprintf("virtio-pci: FAIL! device %d offers no queues\n", driver->device);
#endif
        virtio_pci_set_failed(driver);
        return errno = ENODEV, -1;
    }


    if ((e = virtio_pci_init_interrupts(driver)) < 0) {
        virtio_pci_set_failed(driver);
        return e;
    }


    for (size_t i = 0; i < driver->internals.num_queues; i++) {

        if (virtq_init(driver, driver->internals.common_config, i) < 0) {
#if DEBUG_LEVEL_FATAL
            kprintf("virtio-pci: FAIL! device %d queue %d initialization failed\n", driver->device, i);
#endif
            virtio_pci_set_failed(driver);
            return errno = ENOSYS, -1;
        }
    }


#if DEBUG_LEVEL_TRACE
    kprintf("virtio-pci: device %d common initialization successful [queues(%d)]\n", driver->device, driver->internals.num_queues);
#endif

    return 0;
}


static int virtio_pci_init_device_cfg(struct virtio_driver* driver, struct virtio_pci_location* at) {

    DEBUG_ASSERT(driver);
    DEBUG_ASSERT(driver->device);
    DEBUG_ASSERT(at);

    driver->internals.device_config = virtio_pci_find_bar(driver, at->bar, at->offset);

    if (unlikely(!driver->internals.device_config))
        return errno = ENXIO, -1;

    int e = 0;
    if (unlikely(!driver->setup))
        return e;

    if ((e = driver->setup(driver, driver->internals.device_config)) < 0)
        return e;

#if DEBUG_LEVEL_TRACE
    kprintf("virtio-pci: device %d obtaining device config successful [bar(%d), offset(%p)]\n", driver->device, at->bar, at->offset);
#endif
    return 0;
}


static int virtio_pci_init_isr_status(struct virtio_driver* driver, struct virtio_pci_location* at) {

    DEBUG_ASSERT(driver);
    DEBUG_ASSERT(driver->device);
    DEBUG_ASSERT(at);

    /* An offset of zero is a legal place for the ISR byte to sit; only the mapping failing
       is a problem. */
    driver->internals.isr_status = (uint32_t volatile*)virtio_pci_find_bar(driver, at->bar, at->offset);

    if (unlikely(!driver->internals.isr_status))
        return errno = ENXIO, -1;

#if DEBUG_LEVEL_TRACE
    kprintf("virtio-pci: device %d obtaining isr status successful [bar(%d), offset(%p), isr(%p)]\n", driver->device, at->bar, at->offset, driver->internals.isr_status);
#endif
    return 0;
}


static int virtio_pci_init_notify_cfg(struct virtio_driver* driver, struct virtio_pci_location* at) {

    DEBUG_ASSERT(driver);
    DEBUG_ASSERT(driver->device);
    DEBUG_ASSERT(at);

    struct virtio_pci_notify_cfg cfg;
    pci_memcpy(driver->device, &cfg, at->address + sizeof(struct virtio_pci_cap), sizeof(struct virtio_pci_notify_cfg));

    uint32_t multiplier = le32_to_cpu(cfg.notify_off_multiplier);

    if (unlikely(multiplier == 0)) {
#if DEBUG_LEVEL_FATAL
        kprintf("virtio-pci: FAIL! device %d has null notify_off_multiplier\n", driver->device);
#endif
        return errno = EINVAL, -1;
    }

    driver->internals.notify_offset = virtio_pci_find_bar(driver, at->bar, at->offset);

    if (unlikely(!driver->internals.notify_offset))
        return errno = ENXIO, -1;

    driver->internals.notify_off_multiplier = multiplier;

#if DEBUG_LEVEL_TRACE
    kprintf("virtio-pci: device %d obtaining notify_off_multiplier successful [base(%p), multiplier(%d)]\n", driver->device, driver->internals.notify_offset, multiplier);
#endif
    return 0;
}


/* Put back what bringing the device up took.
 *
 * The queues are disabled and the device reset before their memory is released, in that
 * order: while a queue is enabled the host is entitled to read the pages behind it, and by
 * the time the allocator has them back they may belong to something else.
 *
 * The BAR mappings are left in place. They are identity mappings of this device's own
 * apertures, which costs page tables and nothing else, and there is no record here of what
 * was mapped where to undo them with. */

void virtio_pci_dnit(struct virtio_driver* driver) {

    DEBUG_ASSERT(driver);

    if (driver->internals.common_config) {

        for (size_t i = 0; i < driver->internals.num_queues; i++) {

            mmio_w16(&driver->internals.common_config->queue_select, cpu_to_le16(i));
            atomic_thread_fence(memory_order_seq_cst);

            mmio_w16(&driver->internals.common_config->queue_enable, cpu_to_le16(0));
        }

        virtio_pci_set_status(driver, VIRTIO_DEVICE_STATUS_RESET);
    }


#if defined(CONFIG_HAVE_PCI_MSIX)

    pci_msix_t msix;

    if (pci_find_msix(driver->device, &msix) != PCI_NONE) {

        pci_msix_disable(driver->device, &msix);

        /* One table row and one device slot per call, and this device took one per queue
           vector plus one for the configuration. */
        for (uint16_t i = 0; i <= driver->internals.msix_vectors; i++) {

            if (pci_msix_unmap_irq(driver->device, &msix) < 0)
                break;
        }
    }

#else

    pci_intx_mask(driver->device);
    pci_intx_unmap_irq(driver->device);

#endif


    for (size_t i = 0; i < VIRTQ_MAX_QUEUES; i++)
        virtq_dnit(driver, i);


    driver->internals.common_config         = NULL;
    driver->internals.isr_status            = NULL;
    driver->internals.device_config         = 0;
    driver->internals.notify_offset         = 0;
    driver->internals.notify_off_multiplier = 0;
    driver->internals.num_queues            = 0;
    driver->internals.msix_vectors          = 0;
}


int virtio_pci_init(struct virtio_driver* driver) {

    DEBUG_ASSERT(driver);
    DEBUG_ASSERT(driver->device);

    uintptr_t caps;
    if ((caps = pci_find_capabilities(driver->device)) == PCI_NONE) {
#if DEBUG_LEVEL_FATAL
        kprintf("virtio-pci: FAIL! cannot find capabilities for pci device %d\n", driver->device);
#endif
        return errno = EINVAL, -1;
    }

    pci_enable_pio(driver->device);
    pci_enable_mmio(driver->device);
    pci_enable_bus_mastering(driver->device);


    /* Read the whole list before acting on any of it: the order these have to be brought up
       in is not the order they are listed in, and a device listing them in the order the
       specification does gave every queue a notify address of zero. */

    struct virtio_pci_location found[VIRTIO_PCI_CAP_PCI_CFG + 1] = {0};

    for (size_t n = 0; caps && n < VIRTIO_PCI_MAX_CAPABILITIES; n++) {

        struct virtio_pci_cap cap;
        pci_memcpy(driver->device, &cap, caps, sizeof(struct virtio_pci_cap));

        if (cap.cap_vndr == VIRTIO_PCI_CAP_VENDOR) {

            if (cap.cfg_type <= VIRTIO_PCI_CAP_PCI_CFG) {

                /* The first of each kind wins: a device is free to offer more than one and
                   the extras are alternative views of the same thing. */
                if (!found[cap.cfg_type].found) {

                    found[cap.cfg_type].found   = true;
                    found[cap.cfg_type].bar     = cap.bar;
                    found[cap.cfg_type].offset  = le32_to_cpu(cap.offset);
                    found[cap.cfg_type].length  = le32_to_cpu(cap.length);
                    found[cap.cfg_type].address = caps;
                }

            } else {
#if DEBUG_LEVEL_WARN
                kprintf("virtio-pci: WARN! found unknown configuration type %d [offset(%p)]\n", cap.cfg_type, caps);
#endif
            }
        }

        caps = cap.cap_next;
    }


    if (!found[VIRTIO_PCI_CAP_COMMON_CFG].found || !found[VIRTIO_PCI_CAP_NOTIFY_CFG].found) {
#if DEBUG_LEVEL_FATAL
        kprintf("virtio-pci: FAIL! device %d is missing the common or notify configuration capability\n", driver->device);
#endif
        return errno = ENODEV, -1;
    }


    int e;

    //? Before the queues: each one is kicked at an address derived from this.
    if ((e = virtio_pci_init_notify_cfg(driver, &found[VIRTIO_PCI_CAP_NOTIFY_CFG])) < 0)
        goto fail;

    //? Before the interrupts, which read it when MSI-X is not in use.
    if (found[VIRTIO_PCI_CAP_ISR_CFG].found && (e = virtio_pci_init_isr_status(driver, &found[VIRTIO_PCI_CAP_ISR_CFG])) < 0)
        goto fail;

    //? Reset, features, interrupts and queues, but not DRIVER_OK.
    if ((e = virtio_pci_init_common_cfg(driver, &found[VIRTIO_PCI_CAP_COMMON_CFG])) < 0)
        goto fail;

    //? Device specific setup, which needs the features agreed and the queues live.
    if (found[VIRTIO_PCI_CAP_DEVICE_CFG].found && (e = virtio_pci_init_device_cfg(driver, &found[VIRTIO_PCI_CAP_DEVICE_CFG])) < 0) {
        virtio_pci_set_failed(driver);
        goto fail;
    }

    //? Only now is the device allowed to assume the driver is driving it.
    if ((e = virtio_pci_add_status(driver, VIRTIO_DEVICE_STATUS_DRIVER_OK)) < 0)
        goto fail;


#if DEBUG_LEVEL_TRACE
    kprintf("virtio-pci: device %d pci initialization successful [irq(%d), queues(%d)]\n", driver->device, driver->internals.irq, driver->internals.num_queues);
#endif

    return 0;


fail:
    virtio_pci_dnit(driver);
    return e;
}



void init(const char* args) {
}

void dnit(void) {
}
