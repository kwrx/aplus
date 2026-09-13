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
#include <aplus/endian.h>
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

    struct virtio_gpu_resource_detach_backing cmd = {0};
    struct virtio_gpu_response resp               = {0};

    cmd.hdr.type    = VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING;
    cmd.resource_id = resource;

    if (virtq_sendrecv(gpu->driver, VIRTIO_GPU_QUEUE_CONTROL, &cmd, sizeof(cmd), &resp, sizeof(resp)) < 0)
        return errno = EIO, -1;

    if (resp.hdr.type != VIRTIO_GPU_RESP_OK_NODATA)
        return errno = EINVAL, -1;

    return 0;
}


int virtgpu_cmd_resource_unref(struct virtgpu* gpu, uint64_t resource) {
    DEBUG_ASSERT(gpu);
    DEBUG_ASSERT(gpu->driver);

    struct virtio_gpu_resource_unref cmd = {0};
    struct virtio_gpu_response resp       = {0};

    cmd.hdr.type    = VIRTIO_GPU_CMD_RESOURCE_UNREF;
    cmd.resource_id = resource;

    if (virtq_sendrecv(gpu->driver, VIRTIO_GPU_QUEUE_CONTROL, &cmd, sizeof(cmd), &resp, sizeof(resp)) < 0)
        return errno = EIO, -1;

    if (resp.hdr.type != VIRTIO_GPU_RESP_OK_NODATA)
        return errno = EINVAL, -1;

    return 0;
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

    if (resp.hdr.type != VIRTIO_GPU_RESP_OK_NODATA)
        return errno = EINVAL, -1;

    return 0;
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

    if (resp.hdr.type != VIRTIO_GPU_RESP_OK_NODATA)
        return errno = EINVAL, -1;

    return 0;
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

    if (resp.hdr.type != VIRTIO_GPU_RESP_OK_NODATA)
        return errno = EINVAL, -1;

    return 0;
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

    memcpy(display_info, &resp.display_info, sizeof(*display_info));
    return 0;
}

int virtgpu_cmd_resource_flush(struct virtgpu* gpu, uint64_t resource, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    DEBUG_ASSERT(gpu);
    DEBUG_ASSERT(gpu->driver);

    struct virtio_gpu_resource_flush cmd = {0};
    struct virtio_gpu_response resp = {0};

    cmd.hdr.type    = VIRTIO_GPU_CMD_RESOURCE_FLUSH;
    cmd.resource_id = resource;
    cmd.r.x         = x;
    cmd.r.y         = y;
    cmd.r.width     = width;
    cmd.r.height    = height;

    if (virtq_sendrecv(gpu->driver, VIRTIO_GPU_QUEUE_CONTROL, &cmd, sizeof(cmd), &resp, sizeof(resp)) < 0)
        return errno = EIO, -1;

    if (resp.hdr.type != VIRTIO_GPU_RESP_OK_NODATA)
        return errno = EINVAL, -1;

    return 0;
}


/* The same flush, tagged with a fence the device must retire before it answers. On this
 * transport virtq_sendrecv() already blocks until the response comes back, so what the fence
 * buys is the guarantee attached to that response: without it the device may answer as soon
 * as it has accepted the command, and the framebuffer is then still being read while the
 * caller believes the frame is done. With it, the answer means the flush has completed.
 *
 * The device echoes the fence id back in the response header; a mismatch means the answer
 * belongs to some other command and the frame cannot be assumed finished.
 */

int virtgpu_cmd_resource_flush_fenced(struct virtgpu* gpu, uint64_t resource, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    DEBUG_ASSERT(gpu);
    DEBUG_ASSERT(gpu->driver);

    struct virtio_gpu_resource_flush cmd = {0};
    struct virtio_gpu_response resp      = {0};

    uint64_t fence = ++gpu->fence_id;

    cmd.hdr.type     = VIRTIO_GPU_CMD_RESOURCE_FLUSH;
    cmd.hdr.flags    = VIRTIO_GPU_FLAGS_FENCE;
    cmd.hdr.fence_id = fence;
    cmd.resource_id  = resource;
    cmd.r.x          = x;
    cmd.r.y          = y;
    cmd.r.width      = width;
    cmd.r.height     = height;

    if (virtq_sendrecv(gpu->driver, VIRTIO_GPU_QUEUE_CONTROL, &cmd, sizeof(cmd), &resp, sizeof(resp)) < 0)
        return errno = EIO, -1;

    if (resp.hdr.type != VIRTIO_GPU_RESP_OK_NODATA)
        return errno = EINVAL, -1;

    if (resp.hdr.fence_id != fence)
        return errno = EIO, -1;

    return 0;
}


/* Point the cursor plane at a resource, place it, and say which pixel of it sits under the
 * pointer. Resource 0 means "no cursor", which is how the plane is hidden.
 *
 * These go on the cursor queue rather than the control queue on purpose: it exists so that
 * moving the pointer does not queue behind whatever rendering work is in flight, which is
 * most of the reason a hardware cursor feels different from a drawn one.
 */

int virtgpu_cmd_update_cursor(struct virtgpu* gpu, uint32_t scanout_id, uint64_t resource, uint32_t x, uint32_t y, uint32_t hot_x, uint32_t hot_y) {
    DEBUG_ASSERT(gpu);
    DEBUG_ASSERT(gpu->driver);

    struct virtio_gpu_update_cursor cmd = {0};
    struct virtio_gpu_response resp     = {0};

    cmd.hdr.type       = VIRTIO_GPU_CMD_UPDATE_CURSOR;
    cmd.pos.scanout_id = scanout_id;
    cmd.pos.x          = x;
    cmd.pos.y          = y;
    cmd.resource_id    = resource;
    cmd.hot_x          = hot_x;
    cmd.hot_y          = hot_y;

    /* The device consumes these without writing anything back, so the response buffer comes
       back empty. It is sent anyway because it is what returns the descriptors to the pool:
       a send with no reply to wait for would leak one per pointer movement. */
    if (virtq_sendrecv(gpu->driver, VIRTIO_GPU_QUEUE_CURSOR, &cmd, sizeof(cmd), &resp, sizeof(resp)) < 0)
        return errno = EIO, -1;

    return 0;
}


int virtgpu_cmd_move_cursor(struct virtgpu* gpu, uint32_t scanout_id, uint32_t x, uint32_t y) {
    DEBUG_ASSERT(gpu);
    DEBUG_ASSERT(gpu->driver);

    struct virtio_gpu_update_cursor cmd = {0};
    struct virtio_gpu_response resp     = {0};

    cmd.hdr.type       = VIRTIO_GPU_CMD_MOVE_CURSOR;
    cmd.pos.scanout_id = scanout_id;
    cmd.pos.x          = x;
    cmd.pos.y          = y;

    if (virtq_sendrecv(gpu->driver, VIRTIO_GPU_QUEUE_CURSOR, &cmd, sizeof(cmd), &resp, sizeof(resp)) < 0)
        return errno = EIO, -1;

    return 0;
}
