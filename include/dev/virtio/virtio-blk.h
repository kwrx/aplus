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

#ifndef _DEV_VIRTIO_VIRTIO_BLK_H
#define _DEV_VIRTIO_VIRTIO_BLK_H

// Feature Bits
#define VIRTIO_BLK_F_SIZE_MAX     (1 << 1)
#define VIRTIO_BLK_F_SEG_MAX      (1 << 2)
#define VIRTIO_BLK_F_GEOMETRY     (1 << 4)
#define VIRTIO_BLK_F_RO           (1 << 5)
#define VIRTIO_BLK_F_BLK_SIZE     (1 << 6)
#define VIRTIO_BLK_F_FLUSH        (1 << 9)
#define VIRTIO_BLK_F_TOPOLOGY     (1 << 10)
#define VIRTIO_BLK_F_CONFIG_WCE   (1 << 11)
#define VIRTIO_BLK_F_MQ           (1 << 12)
#define VIRTIO_BLK_F_DISCARD      (1 << 13)
#define VIRTIO_BLK_F_WRITE_ZEROES (1 << 14)
#define VIRTIO_BLK_F_LIFETIME     (1 << 15)
#define VIRTIO_BLK_F_SECURE_ERASE (1 << 16)
#define VIRTIO_BLK_F_ZONED        (1 << 17)

// Request Types
#define VIRTIO_BLK_T_IN           0
#define VIRTIO_BLK_T_OUT          1
#define VIRTIO_BLK_T_FLUSH        4
#define VIRTIO_BLK_T_GET_ID       8
#define VIRTIO_BLK_T_GET_LIFETIME 10
#define VIRTIO_BLK_T_DISCARD      11
#define VIRTIO_BLK_T_WRITE_ZEROES 13
#define VIRTIO_BLK_T_SECURE_ERASE 14

// Status
#define VIRTIO_BLK_S_OK     0
#define VIRTIO_BLK_S_IOERR  1
#define VIRTIO_BLK_S_UNSUPP 2

// Maximum Block Size
#define VIRTIO_BLK_MAX_BLK_SIZE 4096


#ifndef __ASSEMBLY__

    #include <aplus.h>
    #include <aplus/debug.h>
    #include <aplus/syscall.h>
    #include <stdint.h>


typedef struct virtio_blk_config {

    uint64_t capacity;
    uint32_t size_max;
    uint32_t seg_max;

    struct {

        uint16_t cylinders;
        uint8_t heads;
        uint8_t sectors;
        uint16_t blk_size;

    } geometry;

    uint32_t blk_size;

    struct {

        uint8_t physical_block_exp;
        uint8_t alignment_offset;
        uint16_t min_io_size;
        uint32_t opt_io_size;

    } topology;

    uint8_t writeback;
    uint8_t unused0;

    uint16_t num_queues;
    uint32_t max_discard_sectors;
    uint32_t max_discard_seg;
    uint32_t discard_sector_alignment;
    uint32_t max_write_zeroes_sectors;
    uint32_t max_write_zeroes_seg;
    uint8_t write_zeroes_may_unmap;
    uint8_t unused1[3];
    uint32_t max_secure_erase_sectors;
    uint32_t max_secure_erase_seg;
    uint32_t secure_erase_sector_alignment;

    struct {

        uint8_t num_zones;
        uint8_t zone_size_shift;
        uint16_t max_persistent_active_zones;
        uint8_t zone_write_granularity_shift;
        uint8_t zone_write_granularity_alignment;
        uint8_t max_zone_append_size;
        uint8_t max_append_sectors;
        uint8_t unused[2];

    } zoned;

} __packed virtio_blk_config_t;


typedef struct virtio_blk_request {

    struct {
        uint32_t type;
        uint32_t reserved;
        uint64_t sector;
    } __packed hdr;

    uint8_t data[VIRTIO_BLK_MAX_BLK_SIZE];
    // uint8_t status;

} __packed virtio_blk_request_t;


__BEGIN_DECLS


__END_DECLS

#endif

#endif
