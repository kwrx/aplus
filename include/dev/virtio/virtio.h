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


#define VIRTIO_PCI_VENDOR                           0x1AF4
#define VIRTIO_PCI_DEVICE_MIN                       0x1040
#define VIRTIO_PCI_DEVICE_MAX                       0x107F
#define VIRTIO_PCI_DEVICE(d)                        (VIRTIO_PCI_DEVICE_MIN + d)

#define VIRTIO_PCI_CAP_VENDOR                       0x09


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


#define VIRTIO_DEVICE_STATUS_RESET                  0
#define VIRTIO_DEVICE_STATUS_ACKNOWNLEDGE           1
#define VIRTIO_DEVICE_STATUS_DRIVER                 2
#define VIRTIO_DEVICE_STATUS_DRIVER_OK              4
#define VIRTIO_DEVICE_STATUS_FEATURES_OK            8
#define VIRTIO_DEVICE_STATUS_NEED_RESET             64
#define VIRTIO_DEVICE_STATUS_FAILED                 128


#define VIRTIO_PCI_CAP_COMMON_CFG                   1
#define VIRTIO_PCI_CAP_NOTIFY_CFG                   2
#define VIRTIO_PCI_CAP_ISR_CFG                      3
#define VIRTIO_PCI_CAP_DEVICE_CFG                   4
#define VIRTIO_PCI_CAP_PCI_CFG                      5


#ifndef __ASSEMBLY__

__BEGIN_DECLS

struct virtio_driver {

    uint16_t type;
    pcidev_t device;

    uint32_t (*negotiate) (struct virtio_driver*, uint32_t features);
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

};


struct virtio_pci_common_cfg {

    volatile uint32_t device_feature_select;
    volatile uint32_t device_feature;
    volatile uint32_t driver_feature_select;
    volatile uint32_t driver_feature;
    volatile uint16_t msix_config;
    volatile uint16_t num_queues;
    volatile uint8_t device_status;
    volatile uint8_t config_generation;
    
    volatile uint16_t queue_select;
    volatile uint16_t queue_size;
    volatile uint16_t queue_msix_vector;
    volatile uint16_t queue_enable;
    volatile uint16_t queue_notify_off;
    volatile uint16_t queue_desc;
    volatile uint16_t queue_driver;
    volatile uint16_t queue_device;

};


struct virtio_pci_notify_cap {
    struct virtio_pci_cap cap;
    uint32_t notify_off_multiplier;
};





int virtio_pci_init(struct virtio_driver*);

__END_DECLS

#endif

#endif