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

#ifndef _DEV_VIRTIO_VIRTIO_INPUT_H
#define _DEV_VIRTIO_VIRTIO_INPUT_H


// Queues
#define VIRTIO_INPUT_QUEUE_EVENT  0
#define VIRTIO_INPUT_QUEUE_STATUS 1


/* The device configuration space is a window, not a record: a driver writes select and
   subsel, then reads size and the union back to get the one entry it asked for. */

#define VIRTIO_INPUT_CFG_UNSET     0x00
#define VIRTIO_INPUT_CFG_ID_NAME   0x01
#define VIRTIO_INPUT_CFG_ID_SERIAL 0x02
#define VIRTIO_INPUT_CFG_ID_DEVIDS 0x03
#define VIRTIO_INPUT_CFG_PROP_BITS 0x10
#define VIRTIO_INPUT_CFG_EV_BITS   0x11
#define VIRTIO_INPUT_CFG_ABS_INFO  0x12


#ifndef __ASSEMBLY__

    #include <aplus.h>
    #include <aplus/debug.h>
    #include <aplus/ipc.h>
    #include <aplus/syscall.h>
    #include <stdbool.h>
    #include <stdint.h>

__BEGIN_DECLS

struct virtio_input_absinfo {
    uint32_t min;
    uint32_t max;
    uint32_t fuzz;
    uint32_t flat;
    uint32_t res;
} __packed;

struct virtio_input_devids {
    uint16_t bustype;
    uint16_t vendor;
    uint16_t product;
    uint16_t version;
} __packed;

struct virtio_input_config {

    volatile uint8_t select;
    volatile uint8_t subsel;
    volatile uint8_t size;
    volatile uint8_t reserved[5];

    union {
        volatile char string[128];
        volatile uint8_t bitmap[128];
        struct virtio_input_absinfo volatile abs;
        struct virtio_input_devids volatile ids;
    } u;

} __packed;


/* One event, in the same type/code/value shape Linux uses on the wire. The device writes
   exactly one of these per buffer it takes off the event queue. */

struct virtio_input_event {
    uint16_t type;
    uint16_t code;
    uint32_t value;
} __packed;

__END_DECLS

#endif

#endif
