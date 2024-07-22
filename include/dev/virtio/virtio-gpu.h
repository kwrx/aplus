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

#ifndef _DEV_VIRTIO_VIRTIO_GPU_H
#define _DEV_VIRTIO_VIRTIO_GPU_H


#define VIRTIO_GPU_MAX_SCANOUTS 16

// Features
#define VIRTIO_GPU_F_VIRGL (1 << 0)
#define VIRTIO_GPU_F_EDID  (1 << 1)

// Flags
#define VIRTIO_GPU_FLAGS_FENCE (1 << 0)


// Display Commands
#define VIRTIO_GPU_CMD_GET_DISPLAY_INFO        0x100
#define VIRTIO_GPU_CMD_RESOURCE_CREATE_2D      0x101
#define VIRTIO_GPU_CMD_RESOURCE_UNREF          0x102
#define VIRTIO_GPU_CMD_SET_SCANOUT             0x103
#define VIRTIO_GPU_CMD_RESOURCE_FLUSH          0x104
#define VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D     0x105
#define VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING 0x106
#define VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING 0x107
#define VIRTIO_GPU_CMD_GET_CAPSET_INFO         0x108
#define VIRTIO_GPU_CMD_GET_CAPSET              0x109
#define VIRTIO_GPU_CMD_GET_EDID                0x10A

// Cursor Commands
#define VIRTIO_GPU_CMD_UPDATE_CURSOR 0x300
#define VIRTIO_GPU_CMD_MOVE_CURSOR   0x301

// Responses
#define VIRTIO_GPU_RESP_OK_NODATA       0x1100
#define VIRTIO_GPU_RESP_OK_DISPLAY_INFO 0x1101
#define VIRTIO_GPU_RESP_OK_CAPSET_INFO  0x1102
#define VIRTIO_GPU_RESP_OK_CAPSET       0x1103
#define VIRTIO_GPU_RESP_OK_EDID         0x1104

// Errors
#define VIRTIO_GPU_RESP_ERR_UNSPEC              0x1200
#define VIRTIO_GPU_RESP_ERR_OUT_OF_MEMORY       0x1201
#define VIRTIO_GPU_RESP_ERR_INVALID_SCANOUT_ID  0x1202
#define VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID 0x1203
#define VIRTIO_GPU_RESP_ERR_INVALID_CONTEXT_ID  0x1204
#define VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER   0x1205

// Formats
#define VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM 1
#define VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM 2
#define VIRTIO_GPU_FORMAT_A8R8G8B8_UNORM 3
#define VIRTIO_GPU_FORMAT_X8R8G8B8_UNORM 4
#define VIRTIO_GPU_FORMAT_R8G8B8A8_UNORM 67
#define VIRTIO_GPU_FORMAT_X8B8G8R8_UNORM 68
#define VIRTIO_GPU_FORMAT_A8B8G8R8_UNORM 121
#define VIRTIO_GPU_FORMAT_R8G8B8X8_UNORM 134


// Queues
#define VIRTIO_GPU_QUEUE_CONTROL 0
#define VIRTIO_GPU_QUEUE_CURSOR  1


#ifndef __ASSEMBLY__


    #include <aplus.h>
    #include <aplus/debug.h>
    #include <aplus/syscall.h>
    #include <stdint.h>

__BEGIN_DECLS

struct virtio_gpu_config {
    uint32_t events_read;
    uint32_t events_clear;
    uint32_t num_scanouts;
    uint32_t reserved;
} __packed;

struct virtio_gpu_ctrl_hdr {
    uint32_t type;
    uint32_t flags;
    uint64_t fence_id;
    uint32_t ctx_id;
    uint32_t padding;
} __packed;

struct virtio_gpu_rect {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
} __packed;

struct virtio_gpu_resp_display_info {
    struct virtio_gpu_display_one {
        struct virtio_gpu_rect r;
        uint32_t enabled;
        uint32_t flags;
    } __packed pmodes[VIRTIO_GPU_MAX_SCANOUTS];
} __packed;

struct virtio_gpu_resp_edid {
    uint32_t size;
    uint32_t padding;
    uint8_t edid[1024];
} __packed;



struct virtio_gpu_get_edid {
    struct virtio_gpu_ctrl_hdr hdr;
    uint32_t scanout;
    uint32_t padding;
} __packed;

struct virtio_gpu_resource_create_2d {
    struct virtio_gpu_ctrl_hdr hdr;
    uint32_t resource_id;
    uint32_t format;
    uint32_t width;
    uint32_t height;
} __packed;

struct virtio_gpu_resource_unref {
    struct virtio_gpu_ctrl_hdr hdr;
    uint32_t resource_id;
    uint32_t padding;
} __packed;

struct virtio_gpu_set_scanout {
    struct virtio_gpu_ctrl_hdr hdr;
    struct virtio_gpu_rect r;
    uint32_t scanout_id;
    uint32_t resource_id;
} __packed;

struct virtio_gpu_resource_flush {
    struct virtio_gpu_ctrl_hdr hdr;
    struct virtio_gpu_rect r;
    uint32_t resource_id;
    uint32_t padding;
} __packed;

struct virtio_gpu_transfer_to_host_2d {
    struct virtio_gpu_ctrl_hdr hdr;
    struct virtio_gpu_rect r;
    uint64_t offset;
    uint32_t resource_id;
    uint32_t padding;
} __packed;

struct virtio_gpu_resource_attach_backing {
    struct virtio_gpu_ctrl_hdr hdr;
    uint32_t resource_id;
    uint32_t nr_entries;
    struct virtio_gpu_mem_entry {
        uint64_t address;
        uint32_t length;
        uint32_t padding;
    } __packed entries[1];
} __packed;

struct virtio_gpu_resource_detach_backing {
    struct virtio_gpu_ctrl_hdr hdr;
    uint32_t resource_id;
    uint32_t padding;
} __packed;

struct virtio_gpu_cursor_pos {
    uint32_t scanout_id;
    uint32_t x;
    uint32_t y;
    uint32_t padding;
} __packed;

struct virtio_gpu_update_cursor {
    struct virtio_gpu_ctrl_hdr hdr;
    struct virtio_gpu_cursor_pos pos;
    uint32_t resource_id;
    uint32_t hot_x;
    uint32_t hot_y;
    uint32_t padding;
} __packed;


struct virtio_gpu_response {
    struct virtio_gpu_ctrl_hdr hdr;
    union {
        struct virtio_gpu_resp_display_info display_info;
        struct virtio_gpu_resp_edid edid;
    };
} __packed;


struct virtgpu {
    struct virtio_driver* driver;
    uint64_t resource_ids;
} __packed;


int virtgpu_scanout_get_primary_info(struct virtgpu*, uint8_t*, uint16_t*, uint16_t*);
int virtgpu_cmd_resource_detach_backing(struct virtgpu*, uint64_t);
int virtgpu_cmd_resource_unref(struct virtgpu*, uint64_t);
int virtgpu_cmd_resource_create_2d(struct virtgpu*, uint64_t*, uint32_t, uint32_t, uint32_t);
int virtgpu_cmd_resource_attach_backing(struct virtgpu*, uint64_t, uint64_t, size_t);
int virtgpu_cmd_set_scanout(struct virtgpu*, uint32_t, uint64_t, uint32_t, uint32_t, uint32_t, uint32_t);
int virtgpu_cmd_transfer_to_host_2d(struct virtgpu*, uint64_t, uint64_t, uint32_t, uint32_t, uint32_t, uint32_t);
int virtgpu_cmd_get_display_info(struct virtgpu*, struct virtio_gpu_resp_display_info*);

__END_DECLS

#endif

#endif
