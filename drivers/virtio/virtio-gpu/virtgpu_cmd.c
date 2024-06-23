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
#include <aplus/errno.h>
#include <aplus/fb.h>
#include <aplus/hal.h>
#include <aplus/memory.h>
#include <aplus/module.h>
#include <aplus/smp.h>

#include <dev/interface.h>
#include <dev/pci.h>
#include <dev/video.h>

#include <dev/virtio/virtio-gpu.h>
#include <dev/virtio/virtio.h>

#include <stdint.h>



int virtgpu_cmd_resource_detach_backing(struct virtgpu* gpu, uint64_t resource) {

    DEBUG_ASSERT(gpu);
    DEBUG_ASSERT(gpu->driver);


    struct virtio_gpu_resource_detach_backing cmd;
    struct virtio_gpu_response resp;

    cmd.hdr.type    = VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING;
    cmd.resource_id = resource;


    if (virtq_sendrecv(gpu->driver, VIRTIO_GPU_QUEUE_CONTROL, &cmd, sizeof(cmd), &resp, sizeof(resp)) < 0)
        return errno = EIO, -1;

    return resp.hdr.type == VIRTIO_GPU_RESP_OK_NODATA ? 0 : -EINVAL;
}


int virtgpu_cmd_resource_unref(struct virtgpu* gpu, uint64_t resource) {

    DEBUG_ASSERT(gpu);
    DEBUG_ASSERT(gpu->driver);


    struct virtio_gpu_resource_unref cmd;
    struct virtio_gpu_response resp;

    cmd.hdr.type    = VIRTIO_GPU_CMD_RESOURCE_UNREF;
    cmd.resource_id = resource;


    if (virtq_sendrecv(gpu->driver, VIRTIO_GPU_QUEUE_CONTROL, &cmd, sizeof(cmd), &resp, sizeof(resp)) < 0)
        return errno = EIO, -1;

    return resp.hdr.type == VIRTIO_GPU_RESP_OK_NODATA ? 0 : -EINVAL;
}


int virtgpu_cmd_resource_create_2d(struct virtgpu* gpu, uint64_t* resource, uint32_t format, uint32_t width, uint32_t height) {

    DEBUG_ASSERT(gpu);
    DEBUG_ASSERT(gpu->driver);


    struct virtio_gpu_resource_create_2d cmd = {0};
    struct virtio_gpu_response resp          = {0};


    uint64_t resource_id = ++gpu->resource_ids;

    cmd.hdr.type    = VIRTIO_GPU_CMD_RESOURCE_CREATE_2D;
    cmd.resource_id = resource_id;
    cmd.format      = format;
    cmd.width       = width;
    cmd.height      = height;


    if (virtq_sendrecv(gpu->driver, VIRTIO_GPU_QUEUE_CONTROL, &cmd, sizeof(cmd), &resp, sizeof(resp)) < 0)
        return errno = EIO, -1;

    if (resp.hdr.type != VIRTIO_GPU_RESP_OK_NODATA)
        return errno = EINVAL, -1;

    // dump hdr
    kprintf("resp.hdr.type = %X\n", resp.hdr.type);
    kprintf("resp.hdr.flags = %d\n", resp.hdr.flags);
    kprintf("resp.hdr.fence_id = %d\n", resp.hdr.fence_id);
    kprintf("resp.hdr.ctx_id = %d\n", resp.hdr.ctx_id);
    kprintf("resp.hdr.padding = %d\n", resp.hdr.padding);

    return *resource = resource_id, 0;
}


int virtgpu_cmd_resource_attach_backing(struct virtgpu* gpu, uint64_t resource, uint64_t framebuffer, size_t size) {

    DEBUG_ASSERT(gpu);
    DEBUG_ASSERT(gpu->driver);


    struct virtio_gpu_resource_attach_backing cmd = {0};
    struct virtio_gpu_response resp               = {0};

    cmd.hdr.type           = VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING;
    cmd.resource_id        = resource;
    cmd.nr_entries         = 1;
    cmd.entries[0].address = framebuffer;
    cmd.entries[0].length  = size;


    if (virtq_sendrecv(gpu->driver, VIRTIO_GPU_QUEUE_CONTROL, &cmd, sizeof(cmd), &resp, sizeof(resp)) < 0)
        return errno = EIO, -1;

    // dump hdr
    kprintf("resp.hdr.type = %X\n", resp.hdr.type);
    kprintf("resp.hdr.flags = %d\n", resp.hdr.flags);
    kprintf("resp.hdr.fence_id = %d\n", resp.hdr.fence_id);
    kprintf("resp.hdr.ctx_id = %d\n", resp.hdr.ctx_id);
    kprintf("resp.hdr.padding = %d\n", resp.hdr.padding);

    return resp.hdr.type == VIRTIO_GPU_RESP_OK_NODATA ? 0 : -EINVAL;
}


int virtgpu_cmd_set_scanout(struct virtgpu* gpu, uint32_t scanout_id, uint64_t resource, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {

    DEBUG_ASSERT(gpu);
    DEBUG_ASSERT(gpu->driver);


    struct virtio_gpu_set_scanout cmd = {0};
    struct virtio_gpu_response resp   = {0};

    cmd.hdr.type    = VIRTIO_GPU_CMD_SET_SCANOUT;
    cmd.scanout_id  = scanout_id;
    cmd.resource_id = resource;
    cmd.r.x         = x;
    cmd.r.y         = y;
    cmd.r.width     = width;
    cmd.r.height    = height;


    if (virtq_sendrecv(gpu->driver, VIRTIO_GPU_QUEUE_CONTROL, &cmd, sizeof(cmd), &resp, sizeof(resp)) < 0)
        return errno = EIO, -1;

    // dump hdr
    kprintf("resp.hdr.type = %X\n", resp.hdr.type);
    kprintf("resp.hdr.flags = %d\n", resp.hdr.flags);
    kprintf("resp.hdr.fence_id = %d\n", resp.hdr.fence_id);
    kprintf("resp.hdr.ctx_id = %d\n", resp.hdr.ctx_id);
    kprintf("resp.hdr.padding = %d\n", resp.hdr.padding);

    return resp.hdr.type == VIRTIO_GPU_RESP_OK_NODATA ? 0 : -EINVAL;
}


int virtgpu_cmd_transfer_to_host_2d(struct virtgpu* gpu, uint64_t resource, uint64_t offset, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {

    DEBUG_ASSERT(gpu);
    DEBUG_ASSERT(gpu->driver);


    struct virtio_gpu_transfer_to_host_2d cmd = {0};
    struct virtio_gpu_response resp           = {0};

    cmd.hdr.type    = VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D;
    cmd.offset      = offset;
    cmd.r.x         = x;
    cmd.r.y         = y;
    cmd.r.width     = width;
    cmd.r.height    = height;
    cmd.resource_id = resource;


    if (virtq_sendrecv(gpu->driver, VIRTIO_GPU_QUEUE_CONTROL, &cmd, sizeof(cmd), &resp, sizeof(resp)) < 0)
        return errno = EIO, -1;

    return resp.hdr.type == VIRTIO_GPU_RESP_OK_NODATA ? 0 : -EINVAL;
}


int virtgpu_cmd_get_display_info(struct virtgpu* gpu, struct virtio_gpu_resp_display_info* display_info) {

    DEBUG_ASSERT(gpu);
    DEBUG_ASSERT(gpu->driver);


    struct virtio_gpu_ctrl_hdr cmd  = {0};
    struct virtio_gpu_response resp = {0};

    cmd.type = VIRTIO_GPU_CMD_GET_DISPLAY_INFO;

    if (virtq_sendrecv(gpu->driver, VIRTIO_GPU_QUEUE_CONTROL, &cmd, sizeof(cmd), &resp, sizeof(resp)) < 0)
        return errno = EIO, -1;

    if (resp.hdr.type != VIRTIO_GPU_RESP_OK_DISPLAY_INFO)
        return errno = EINVAL, -1;


    kprintf("resp.hdr.type = %X\n", resp.hdr.type);
    kprintf("resp.hdr.flags = %d\n", resp.hdr.flags);
    kprintf("resp.hdr.fence_id = %d\n", resp.hdr.fence_id);
    kprintf("resp.hdr.ctx_id = %d\n", resp.hdr.ctx_id);
    kprintf("resp.hdr.padding = %d\n", resp.hdr.padding);

    for (int i = 0; i < 16; i++) {
        kprintf("resp.display_info.pmodes[%d].enabled = %d\n", i, resp.display_info.pmodes[i].enabled);
        kprintf("resp.display_info.pmodes[%d].flags = %d\n", i, resp.display_info.pmodes[i].flags);
        kprintf("resp.display_info.pmodes[%d].r.width = %d\n", i, resp.display_info.pmodes[i].r.width);
        kprintf("resp.display_info.pmodes[%d].r.height = %d\n", i, resp.display_info.pmodes[i].r.height);
        kprintf("resp.display_info.pmodes[%d].r.x = %d\n", i, resp.display_info.pmodes[i].r.x);
        kprintf("resp.display_info.pmodes[%d].r.y = %d\n", i, resp.display_info.pmodes[i].r.y);
    }

    memcpy(display_info, &resp.display_info, sizeof(*display_info));

    return 0;
}
