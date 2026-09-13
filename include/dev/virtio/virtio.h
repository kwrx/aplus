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

#ifndef _DEV_VIRTIO_VIRTIO_H
#define _DEV_VIRTIO_VIRTIO_H


// Device PCI
#define VIRTIO_PCI_VENDOR                 0x1AF4
#define VIRTIO_PCI_DEVICE_MIN             0x1040
#define VIRTIO_PCI_DEVICE_MAX             0x107F
#define VIRTIO_PCI_DEVICE(d)              (VIRTIO_PCI_DEVICE_MIN + d)
#define VIRTIO_PCI_DEVICE_TRANSITIONAL(d) (VIRTIO_PCI_DEVICE_MIN - 0x40 + d)


// Device PCI Capabilities
#define VIRTIO_PCI_CAP_VENDOR     0x09
#define VIRTIO_PCI_CAP_COMMON_CFG 1
#define VIRTIO_PCI_CAP_NOTIFY_CFG 2
#define VIRTIO_PCI_CAP_ISR_CFG    3
#define VIRTIO_PCI_CAP_DEVICE_CFG 4
#define VIRTIO_PCI_CAP_PCI_CFG    5


// Device MSI-X
#define VIRTIO_MSI_NO_VECTOR 0xFFFF


// Device Type
#define VIRTIO_DEVICE_TYPE_INVALID        0
#define VIRTIO_DEVICE_TYPE_NETWORK        1
#define VIRTIO_DEVICE_TYPE_BLOCK          2
#define VIRTIO_DEVICE_TYPE_CONSOLE        3
#define VIRTIO_DEVICE_TYPE_ENTROPY_SOURCE 4
#define VIRTIO_DEVICE_TYPE_SCSI_HOST      8
#define VIRTIO_DEVICE_TYPE_GPU            16
#define VIRTIO_DEVICE_TYPE_CLOCK          17
#define VIRTIO_DEVICE_TYPE_INPUT          18
#define VIRTIO_DEVICE_TYPE_SOCKET         19
#define VIRTIO_DEVICE_TYPE_CRYPTO         20
#define VIRTIO_DEVICE_TYPE_MEMDEV         24


// Device Status
#define VIRTIO_DEVICE_STATUS_RESET        0
#define VIRTIO_DEVICE_STATUS_ACKNOWNLEDGE 1
#define VIRTIO_DEVICE_STATUS_DRIVER       2
#define VIRTIO_DEVICE_STATUS_DRIVER_OK    4
#define VIRTIO_DEVICE_STATUS_FEATURES_OK  8
#define VIRTIO_DEVICE_STATUS_NEED_RESET   64
#define VIRTIO_DEVICE_STATUS_FAILED       128

// ISR Status
#define VIRTIO_ISR_STATUS_QUEUE  (1 << 0)
#define VIRTIO_ISR_STATUS_CONFIG (1 << 1)

// Features, word 0 (bits 0..31). Bits 0..23 are device specific; the rest are transport.
#define VIRTIO_F_NOTIFY_ON_EMPTY (1 << 24)
#define VIRTIO_F_ANY_LAYOUT      (1 << 27)
#define VIRTIO_F_INDIRECT_DESC   (1 << 28)
#define VIRTIO_F_EVENT_IDX       (1 << 29)

// Features, word 1 (bits 32..63): the bit number here is the spec's minus 32.
#define VIRTIO_F_VERSION_1         (1 << 0)  // 32
#define VIRTIO_F_ACCESS_PLATFORM   (1 << 1)  // 33
#define VIRTIO_F_RING_PACKED       (1 << 2)  // 34
#define VIRTIO_F_IN_ORDER          (1 << 3)  // 35
#define VIRTIO_F_ORDER_PLATFORM    (1 << 4)  // 36
#define VIRTIO_F_SR_IOV            (1 << 5)  // 37
#define VIRTIO_F_NOTIFICATION_DATA (1 << 6)  // 38
#define VIRTIO_F_NOTIF_CONFIG_DATA (1 << 7)  // 39
#define VIRTIO_F_RING_RESET        (1 << 8)  // 40


/* What this driver is prepared to accept, per feature word.
 *
 * Word 0 lets every device specific bit through for the device driver's negotiate()
 * callback to accept or drop, and refuses the transport bits above them: none of
 * INDIRECT_DESC, EVENT_IDX or NOTIFY_ON_EMPTY is implemented here, and accepting a feature
 * that is not implemented is how a driver ends up reading a ring the device is writing in a
 * layout it never agreed to.
 *
 * Word 1 is VERSION_1 alone. RING_PACKED would change the ring layout outright,
 * NOTIFICATION_DATA the payload written to the notify register, ACCESS_PLATFORM the meaning
 * of every address handed to the device, and IN_ORDER would let the device write only the
 * last used entry of a batch -- which this driver, handing out arbitrary free descriptors
 * rather than consecutive ones, is in no position to promise. */

#define VIRTIO_FEATURES_MASK_0 0x00FFFFFFU
#define VIRTIO_FEATURES_MASK_1 (VIRTIO_F_VERSION_1)


// Queue Descriptors
#define VIRTQ_DESC_F_NEXT     1
#define VIRTQ_DESC_F_WRITE    2
#define VIRTQ_DESC_F_INDIRECT 4

// Queue Available
#define VIRTQ_AVAIL_F_NO_INTERRUPT 1

// Queue Used
#define VIRTQ_USED_F_NO_NOTIFY 1


// Queue driver configuration
#define VIRTQ_MAX_QUEUES      64
#define VIRTQ_MAX_DESCRIPTORS 65535

// How long virtq_wait() gives a device to answer before giving up on it.
#define VIRTQ_TIMEOUT_MS 5000

// How long virtq_wait() pauses between two looks at the used ring.
#define VIRTQ_POLL_SPINS 1024


/* Returned by virtq_alloc_descriptor() when the queue has none left. 0xFFFF is not a
   descriptor index any queue can have: the ring is capped at 32768 entries. */
#define VIRTQ_DESC_NONE 0xFFFF


/* What the driver knows about a descriptor, kept beside the ring rather than in it. The
   descriptor table itself is read by the device, so it cannot double as the driver's
   bookkeeping: a descriptor is free or not according to this array and nothing else.
   Which of these a descriptor is put into at submission time decides who cleans it up. */

#define VIRTQ_REQUEST_FREE     0 // In the pool.
#define VIRTQ_REQUEST_INFLIGHT 1 // Submitted; a caller is in virtq_wait() for it.
#define VIRTQ_REQUEST_ASYNC    2 // Submitted and forgotten; virtq_reap() frees it.
#define VIRTQ_REQUEST_POSTED   3 // A receive buffer; virtq_reap() hands it to its owner.
#define VIRTQ_REQUEST_DONE     4 // The device is finished with it; the waiter frees it.



#ifndef __ASSEMBLY__


    #include <aplus.h>
    #include <aplus/debug.h>
    #include <aplus/syscall.h>
    #include <stdatomic.h>
    #include <stdint.h>


__BEGIN_DECLS


struct virtio_pci_common_cfg;


/* One slot per descriptor: what the driver did with it and what came back. */

struct virtq_request {
    uint8_t state;
    uint32_t length;
};


struct virtio_driver {

    uint16_t type;
    pcidev_t device;

    size_t send_window_size;
    size_t recv_window_size;

    size_t max_queues;

    int (*negotiate)(struct virtio_driver*, uint32_t*, size_t);
    int (*setup)(struct virtio_driver*, uintptr_t);
    int (*interrupt)(pcidev_t, irq_t, struct virtio_driver*);

    struct {

        irq_t irq;
        uint16_t bars;
        uint16_t num_queues;

        /* How many MSI-X vectors the queues were given. A table is not required to hold one
           per queue plus one for configuration -- QEMU hands a virtio-input device exactly
           two whatever its queue count -- so queue i raises vector i % msix_vectors and the
           configuration vector follows them. */
        uint16_t msix_vectors;

        uint16_t notify_off_multiplier;
        uintptr_t notify_offset;
        uintptr_t device_config;

        /* Kept so that the device can be told to stop -- queues disabled and the status
           reset -- when bringing it up fails partway through. */
        struct virtio_pci_common_cfg volatile* common_config;

        uint32_t volatile* isr_status;


        struct {

            /* Guards the descriptor pool, the available ring and the used ring cursor --
               everything below that both a caller and the interrupt can touch. Taken with
               interrupts disabled, so it is safe from either, but it must never nest:
               re-taking it on one CPU is a deadlock panic, not a recursion. */
            spinlock_t lock;

            /* Bumped by virtq_flush() out of the interrupt. virtq_wait() watches it so that
               it rescans the used ring when the device has said something and idles on
               __cpu_pause() when it has not. */
            atomic_uint completions;

            /* How far into the used ring the reaper has consumed, as a uint16_t so that it
               wraps exactly where the device's own index does. */
            uint16_t last_used;

            struct virtq_descriptor volatile* descriptors;
            struct virtq_available volatile* available;
            struct virtq_used volatile* used;
            struct virtq_notify volatile* notify;

            /* size entries, one per descriptor. */
            struct virtq_request* requests;

            struct {

                /* The single physical allocation the five regions are carved out of, kept
                   so that a queue can be given back if bringing the device up fails. */
                uintptr_t base;
                size_t length;

                uintptr_t sendbuf;
                uintptr_t recvbuf;

            } buffers;

            size_t size;

        } queues[VIRTQ_MAX_QUEUES];

    } internals;
};


struct virtio_pci_cap {

    volatile uint8_t cap_vndr;
    volatile uint8_t cap_next;
    volatile uint8_t cap_len;
    volatile uint8_t cfg_type;
    volatile uint8_t bar;
    volatile uint8_t padding[3];
    volatile uint32_t offset;
    volatile uint32_t length;

} __packed;


struct virtio_pci_common_cfg {

    volatile uint32_t device_feature_select;
    volatile uint32_t device_feature;
    volatile uint32_t driver_feature_select;
    volatile uint32_t driver_feature;
    volatile uint16_t config_msix_vector;
    volatile uint16_t num_queues;
    volatile uint8_t device_status;
    volatile uint8_t config_generation;

    volatile uint16_t queue_select;
    volatile uint16_t queue_size;
    volatile uint16_t queue_msix_vector;
    volatile uint16_t queue_enable;
    volatile uint16_t queue_notify_off;
    volatile uint64_t queue_desc;
    volatile uint64_t queue_driver;
    volatile uint64_t queue_device;

} __packed;

struct virtio_pci_notify_cfg {
    volatile uint32_t notify_off_multiplier;
} __packed;


struct virtq_descriptor {
    volatile uint64_t q_address;
    volatile uint32_t q_length;
    volatile uint16_t q_flags;
    volatile uint16_t q_next;
} __packed;

struct virtq_available {
    volatile uint16_t q_flags;
    volatile uint16_t q_idx;
    volatile uint16_t q_ring[];
    // uint16_t q_used_event;
} __packed;

struct virtq_used {

    volatile uint16_t q_flags;
    volatile uint16_t q_idx;

    struct {
        volatile uint32_t e_id;
        volatile uint32_t e_length;
    } volatile q_elements[];

    // uint16_t q_avail_event;

} __packed;

struct virtq_notify {
    union {
        volatile struct {
            volatile uint32_t n_vqn    : 16;
            volatile uint32_t n_offset : 15;
            volatile uint32_t n_wrap   : 1;
        };
        volatile uint16_t n_idx;
    };
} __packed;



// PCI
int virtio_pci_init(struct virtio_driver*);
void virtio_pci_dnit(struct virtio_driver*);

// Queue
int virtq_init(struct virtio_driver*, struct virtio_pci_common_cfg volatile*, uint16_t);
void virtq_dnit(struct virtio_driver*, uint16_t);

uint16_t virtq_alloc_descriptor(struct virtio_driver*, uint16_t, uint8_t);
void virtq_free_descriptor(struct virtio_driver*, uint16_t, uint16_t);

uintptr_t virtq_recvbuf(struct virtio_driver*, uint16_t, uint16_t);

void virtq_provide(struct virtio_driver*, uint16_t, uint16_t, size_t);
void virtq_notify(struct virtio_driver*, uint16_t);
int virtq_reap(struct virtio_driver*, uint16_t, uint16_t*, uint32_t*);

ssize_t virtq_send(struct virtio_driver*, uint16_t, const void*, size_t);
ssize_t virtq_sendrecv(struct virtio_driver*, uint16_t, const void*, size_t, void*, size_t);
ssize_t virtq_recv(struct virtio_driver*, uint16_t, void*, size_t);

void virtq_flush(struct virtio_driver*, uint16_t);

__END_DECLS

#endif

#endif
