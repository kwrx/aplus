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

#ifndef _DEV_VIRTIO_VIRTIO_NET_H
#define _DEV_VIRTIO_VIRTIO_NET_H


/**
 * @brief The first queue pair. A control queue follows it, which this driver does not ask for.
 */
#define VIRTIO_NET_QUEUE_RX 0
#define VIRTIO_NET_QUEUE_TX 1


// Features, word 0
#define VIRTIO_NET_F_CSUM                (1 << 0)
#define VIRTIO_NET_F_GUEST_CSUM          (1 << 1)
#define VIRTIO_NET_F_CTRL_GUEST_OFFLOADS (1 << 2)
#define VIRTIO_NET_F_MTU                 (1 << 3)
#define VIRTIO_NET_F_MAC                 (1 << 5)
#define VIRTIO_NET_F_GUEST_TSO4          (1 << 7)
#define VIRTIO_NET_F_GUEST_TSO6          (1 << 8)
#define VIRTIO_NET_F_GUEST_ECN           (1 << 9)
#define VIRTIO_NET_F_GUEST_UFO           (1 << 10)
#define VIRTIO_NET_F_HOST_TSO4           (1 << 11)
#define VIRTIO_NET_F_HOST_TSO6           (1 << 12)
#define VIRTIO_NET_F_HOST_ECN            (1 << 13)
#define VIRTIO_NET_F_HOST_UFO            (1 << 14)
#define VIRTIO_NET_F_MRG_RXBUF           (1 << 15)
#define VIRTIO_NET_F_STATUS              (1 << 16)
#define VIRTIO_NET_F_CTRL_VQ             (1 << 17)
#define VIRTIO_NET_F_CTRL_RX             (1 << 18)
#define VIRTIO_NET_F_CTRL_VLAN           (1 << 19)
#define VIRTIO_NET_F_GUEST_ANNOUNCE      (1 << 21)
#define VIRTIO_NET_F_MQ                  (1 << 22)
#define VIRTIO_NET_F_CTRL_MAC_ADDR       (1 << 23)


// Configuration status
#define VIRTIO_NET_S_LINK_UP  (1 << 0)
#define VIRTIO_NET_S_ANNOUNCE (1 << 1)


// Frame header flags
#define VIRTIO_NET_HDR_F_NEEDS_CSUM 1
#define VIRTIO_NET_HDR_F_DATA_VALID 2
#define VIRTIO_NET_HDR_F_RSC_INFO   4

// Frame header segmentation offload
#define VIRTIO_NET_HDR_GSO_NONE  0
#define VIRTIO_NET_HDR_GSO_TCPV4 1
#define VIRTIO_NET_HDR_GSO_UDP   3
#define VIRTIO_NET_HDR_GSO_TCPV6 4
#define VIRTIO_NET_HDR_GSO_ECN   0x80


#ifndef __ASSEMBLY__


    #include <aplus.h>
    #include <aplus/debug.h>
    #include <aplus/syscall.h>
    #include <stdint.h>


__BEGIN_DECLS


/**
 * @brief The device configuration window, of which only the first two fields are read here.
 */

struct virtio_net_config {

    volatile uint8_t mac[6];
    volatile uint16_t status;
    volatile uint16_t max_virtqueue_pairs;
    volatile uint16_t mtu;

} __packed;


/**
 * @brief What every frame carries in front of it, in the layout VIRTIO_F_VERSION_1 asks for.
 */

struct virtio_net_hdr {

    uint8_t flags;
    uint8_t gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
    uint16_t num_buffers;

} __packed;


__END_DECLS

#endif

#endif
