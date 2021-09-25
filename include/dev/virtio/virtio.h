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

#ifndef _DEV_VIRTIO_VIRTIO_H
#define _DEV_VIRTIO_VIRTIO_H


// Device PCI
#define VIRTIO_PCI_VENDOR                           0x1AF4
#define VIRTIO_PCI_DEVICE_MIN                       0x1040
#define VIRTIO_PCI_DEVICE_MAX                       0x107F
#define VIRTIO_PCI_DEVICE(d)                        (VIRTIO_PCI_DEVICE_MIN + d)
#define VIRTIO_PCI_DEVICE_TRANSITIONAL(d)           (VIRTIO_PCI_DEVICE_MIN - 0x40 + d)


// Device PCI Capabilities
#define VIRTIO_PCI_CAP_VENDOR                       0x09
#define VIRTIO_PCI_CAP_COMMON_CFG                   1
#define VIRTIO_PCI_CAP_NOTIFY_CFG                   2
#define VIRTIO_PCI_CAP_ISR_CFG                      3
#define VIRTIO_PCI_CAP_DEVICE_CFG                   4
#define VIRTIO_PCI_CAP_PCI_CFG                      5


// Device MSI-X
#define VIRTIO_MSI_NO_VECTOR                        0xFFFF


// Device Type
#define VIRTIO_DEVICE_TYPE_INVALID                  0
#define VIRTIO_DEVICE_TYPE_NETWORK                  1
#define VIRTIO_DEVICE_TYPE_BLOCK                    2
#define VIRTIO_DEVICE_TYPE_CONSOLE                  3
#define VIRTIO_DEVICE_TYPE_ENTROPY_SOURCE           4
#define VIRTIO_DEVICE_TYPE_SCSI_HOST                8
#define VIRTIO_DEVICE_TYPE_GPU                      16
#define VIRTIO_DEVICE_TYPE_CLOCK                    17
#define VIRTIO_DEVICE_TYPE_INPUT                    18
#define VIRTIO_DEVICE_TYPE_SOCKET                   19
#define VIRTIO_DEVICE_TYPE_CRYPTO                   20
#define VIRTIO_DEVICE_TYPE_MEMDEV                   24


// Device Status
#define VIRTIO_DEVICE_STATUS_RESET                  0
#define VIRTIO_DEVICE_STATUS_ACKNOWNLEDGE           1
#define VIRTIO_DEVICE_STATUS_DRIVER                 2
#define VIRTIO_DEVICE_STATUS_DRIVER_OK              4
#define VIRTIO_DEVICE_STATUS_FEATURES_OK            8
#define VIRTIO_DEVICE_STATUS_NEED_RESET             64
#define VIRTIO_DEVICE_STATUS_FAILED                 128


// Reserved feature bits
// Select (1)
#define VIRTIO_F_IN_ORDER                           (1 << 6)


// Queue Descriptors
#define VIRTQ_DESC_F_NEXT                           1
#define VIRTQ_DESC_F_WRITE                          2
#define VIRTQ_DESC_F_INDIRECT                       4

// Queue Available
#define VIRTQ_AVAIL_F_NO_INTERRUPT                  1

// Queue Used
#define VIRTQ_USED_F_NO_NOTIFY                      1


// Queue driver configuration
#define VIRTQ_MAX_QUEUES                            64
#define VIRTQ_MAX_DESCRIPTORS                       65535



#ifndef __ASSEMBLY__


#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/syscall.h>
#include <stdint.h>


__BEGIN_DECLS

struct virtio_driver {

    uint16_t type;
    pcidev_t device;

    size_t send_window_size;
    size_t recv_window_size;

    int (*negotiate) (struct virtio_driver*, uint32_t*, size_t);
    int (*setup)     (struct virtio_driver*, uintptr_t);
    int (*interrupt) (void*, uint8_t, struct virtio_driver*);

    struct {

        irq_t irq;
        uint16_t bars;
        uint16_t num_queues;
    
        uint16_t notify_off_multiplier;
        uintptr_t notify_offset;
        uintptr_t device_config;

        uint32_t volatile* isr_status;

        
        struct {

            struct virtq_descriptor volatile* descriptors;
            struct virtq_available volatile* available;
            struct virtq_used volatile* used;
            struct virtq_notify volatile* notify;

            struct {
                uintptr_t sendbuf;
                uintptr_t recvbuf;
            } buffers;

            size_t size;
            
        } queues[VIRTQ_MAX_QUEUES];

    } internals;

};


struct virtio_pci_cap {

    uint8_t cap_vndr;
    uint8_t cap_next;
    uint8_t cap_len;
    uint8_t cfg_type;
    uint8_t bar;
    uint8_t padding[3];
    uint32_t offset;
    uint32_t length;

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
    uint32_t notify_off_multiplier;
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
        struct {
            uint32_t n_vqn : 16;
            uint32_t n_offset : 15;
            uint32_t n_wrap : 1;
        };
        uint16_t n_idx;
    };
} __packed;



// PCI
int virtio_pci_init(struct virtio_driver*);

// Queue
int virtq_init(struct virtio_driver*, struct virtio_pci_common_cfg volatile*, uint16_t);
ssize_t virtq_send(struct virtio_driver*, uint16_t, void*, size_t);
ssize_t virtq_sendrecv(struct virtio_driver*, uint16_t, void*, size_t, void*, size_t);
ssize_t virtq_recv(struct virtio_driver*, uint16_t, void*, size_t);

__END_DECLS

#endif

#endif