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



MODULE_NAME("virtio/virtio-gpu");
MODULE_DEPS("dev/interface,dev/pci,virtio/virtio-pci,virtio/virtio-queue");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");



#define VIRTGPU_ID "VIRTIO-GPU"
#define VIRTGPU_DISPLAY_PRIMARY 0



static void virtgpu_init(device_t*);
static void virtgpu_dnit(device_t*);
static void virtgpu_reset(device_t*);
static void virtgpu_update(device_t*);
static void virtgpu_flush(device_t*, uint32_t, uint32_t, uint32_t, uint32_t);
static void virtgpu_wait_vsync(device_t*);
static int virtgpu_cursor_set(device_t*, const struct fb_hwcursor*, const uint32_t*);
static int virtgpu_cursor_move(device_t*, int32_t, int32_t);


device_t device = {

    .type = DEVICE_TYPE_VIDEO,

    .name        = "fb0",
    .description = "VIRTIO GPU Device",

    .major = 10,
    .minor = 243,

    .status   = DEVICE_STATUS_UNKNOWN,
    .userdata = NULL,

    .init  = virtgpu_init,
    .dnit  = virtgpu_dnit,
    .reset = virtgpu_reset,

    .vid.update      = virtgpu_update,
    .vid.flush       = virtgpu_flush,
    .vid.wait_vsync  = virtgpu_wait_vsync,
    .vid.cursor_set  = virtgpu_cursor_set,
    .vid.cursor_move = virtgpu_cursor_move,

    .vid.hwc.flags      = FB_HWCINFO_HAS_CURSOR,
    .vid.hwc.max_width  = VIRTGPU_CURSOR_SIZE,
    .vid.hwc.max_height = VIRTGPU_CURSOR_SIZE,

};


/* The framebuffer is ordinary RAM that the device reads by DMA, so userspace needs its own
 * view of it. It is mapped at its physical address, as the other video adapters here do,
 * which is what lets fb_fix_screeninfo.smem_start be handed to a process as a pointer.
 *
 * Unlike those adapters this is RAM rather than a PCI aperture, and the kernel already has it
 * mapped write-back through the direct map; asking for write-combining here as they do would
 * leave two mappings of the same page disagreeing about its memory type. Write-back is also
 * the right choice for the traffic: a compositor blits whole rows into it.
 */

static void virtgpu_map_framebuffer(device_t* device) {

    DEBUG_ASSERT(device->vid.fb_base);
    DEBUG_ASSERT(device->vid.fb_size);

    PANIC_ASSERT(ARCH_VMM_MAP_FAILED != arch_vmm_map(&core->bsp.address_space, device->vid.fb_base, device->vid.fb_base, device->vid.fb_size,
                                                     ARCH_VMM_MAP_FIXED | ARCH_VMM_MAP_RDWR | ARCH_VMM_MAP_USER | ARCH_VMM_MAP_NOEXEC | ARCH_VMM_MAP_SHARED));
}


static void virtgpu_free_framebuffer(device_t* device) {

    if (!device->vid.fb_base)
        return;

    arch_vmm_unmap(&core->bsp.address_space, device->vid.fb_base, device->vid.fb_size);

    pmm_free_blocks(device->vid.fb_base, device->vid.fb_size / PML1_PAGESIZE + 1);

    device->vid.fb_base = 0;
    device->vid.fb_size = 0;
}


/* Take the scanout resource back from the device.
 *
 * This has to happen before the framebuffer it describes is handed back to the allocator,
 * never after: while the resource still holds the backing, the host is entitled to read those
 * pages, and by then they may belong to something else. */

static void virtgpu_release_scanout(struct virtgpu* gpu) {

    if (!gpu->fb_resource_id)
        return;

    virtgpu_cmd_resource_detach_backing(gpu, gpu->fb_resource_id);
    virtgpu_cmd_resource_unref(gpu, gpu->fb_resource_id);

    gpu->fb_resource_id = 0;
}


static void virtgpu_init(device_t* device) {
    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->vid.fb_base == 0);
    DEBUG_ASSERT(device->vid.fb_size == 0);
    virtgpu_reset(device);
}


static void virtgpu_dnit(device_t* device) {
    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->userdata);

    struct virtgpu* gpu = device->userdata;

    scoped_lock(&gpu->lock) {

        if (gpu->cursor.resource_id) {

            virtgpu_cmd_update_cursor(gpu, VIRTGPU_DISPLAY_PRIMARY, 0, 0, 0, 0, 0);

            virtgpu_cmd_resource_detach_backing(gpu, gpu->cursor.resource_id);
            virtgpu_cmd_resource_unref(gpu, gpu->cursor.resource_id);

            pmm_free_blocks(gpu->cursor.buffer, gpu->cursor.size / PML1_PAGESIZE + 1);

            gpu->cursor.resource_id = 0;
            gpu->cursor.buffer      = 0;
        }

        virtgpu_release_scanout(gpu);

        virtgpu_free_framebuffer(device);
    }
}



static void virtgpu_reset_framebuffer(device_t* device) {
    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->userdata);

    struct virtio_gpu_resp_display_info display_info = {0};
    if (virtgpu_cmd_get_display_info(device->userdata, &display_info) < 0) {
        device->status = DEVICE_STATUS_FAILED;
        return;
    }

    if (!display_info.pmodes[VIRTGPU_DISPLAY_PRIMARY].enabled) {
#if DEBUG_LEVEL_ERROR
        kprintf("virtio-gpu: ERROR! Primary display %d not enabled\n", VIRTGPU_DISPLAY_PRIMARY);
#endif
        device->status = DEVICE_STATUS_FAILED;
        return;
    }

    struct virtgpu* gpu = device->userdata;

    if (virtgpu_cmd_resource_create_2d(gpu, &gpu->fb_resource_id, VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM, device->vid.vs.xres, device->vid.vs.yres) < 0) {
        device->status = DEVICE_STATUS_FAILED;
        return;
    }

    if (virtgpu_cmd_resource_attach_backing(gpu, gpu->fb_resource_id, device->vid.fb_base, device->vid.fb_size) < 0) {
        device->status = DEVICE_STATUS_FAILED;
        return;
    }

    if (virtgpu_cmd_set_scanout(gpu, VIRTGPU_DISPLAY_PRIMARY, gpu->fb_resource_id, 0, 0, device->vid.vs.xres, device->vid.vs.yres) < 0) {
        device->status = DEVICE_STATUS_FAILED;
        return;
    }
}



static void virtgpu_reset(device_t* device) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->userdata);


    virtgpu_release_scanout(device->userdata);

    virtgpu_free_framebuffer(device);


    memset(&device->vid.fs, 0, sizeof(struct fb_fix_screeninfo));
    memset(&device->vid.vs, 0, sizeof(struct fb_var_screeninfo));

    if (!core->framebuffer.address) {

        device->vid.vs.xres           = 1280;
        device->vid.vs.yres           = 720;
        device->vid.vs.xres_virtual   = 1280;
        device->vid.vs.yres_virtual   = 720;
        device->vid.vs.bits_per_pixel = 32;

    } else {

        device->vid.vs.xres           = core->framebuffer.width;
        device->vid.vs.yres           = core->framebuffer.height;
        device->vid.vs.xres_virtual   = core->framebuffer.width;
        device->vid.vs.yres_virtual   = core->framebuffer.height;
        device->vid.vs.bits_per_pixel = core->framebuffer.depth;
    }

    device->vid.vs.activate = FB_ACTIVATE_NOW;


    device->vid.fb_size = device->vid.vs.xres * device->vid.vs.yres * device->vid.vs.bits_per_pixel / 8;
    device->vid.fb_base = pmm_alloc_blocks(device->vid.fb_size / PML1_PAGESIZE + 1);

    if (unlikely(!device->vid.fb_base)) {
        device->status = DEVICE_STATUS_FAILED;
        return;
    }

    virtgpu_map_framebuffer(device);


    virtgpu_reset_framebuffer(device);

    /* fb_fix_screeninfo is what a process reads to find the framebuffer and its pitch, and
       nothing else here fills it in. Leaving it to the first FBIOPUT_VSCREENINFO would mean
       whoever opens the device first sees a NULL pointer and a pitch of zero. */
    virtgpu_update(device);
}


static void virtgpu_update(device_t* device) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->userdata);
    DEBUG_ASSERT(device->vid.vs.xres);
    DEBUG_ASSERT(device->vid.vs.yres);
    DEBUG_ASSERT(device->vid.vs.bits_per_pixel);



    static int rgba[] = {
        0, 0,  0, 0, 0, 0, 0, 0,  /* NULL             */
        3, 5,  3, 2, 2, 0, 0, 0,  /* 8bpp RRRGGGBB    */
        5, 11, 6, 6, 5, 0, 0, 0,  /* 16bpp RGB565     */
        8, 16, 8, 8, 8, 0, 0, 0,  /* 24bpp RGB24      */
        8, 16, 8, 8, 8, 0, 8, 24, /* 32bpp ARGB       */
    };

    device->vid.vs.red.length    = rgba[device->vid.vs.bits_per_pixel + 0];
    device->vid.vs.red.offset    = rgba[device->vid.vs.bits_per_pixel + 1];
    device->vid.vs.green.length  = rgba[device->vid.vs.bits_per_pixel + 2];
    device->vid.vs.green.offset  = rgba[device->vid.vs.bits_per_pixel + 3];
    device->vid.vs.blue.length   = rgba[device->vid.vs.bits_per_pixel + 4];
    device->vid.vs.blue.offset   = rgba[device->vid.vs.bits_per_pixel + 5];
    device->vid.vs.transp.length = rgba[device->vid.vs.bits_per_pixel + 6];
    device->vid.vs.transp.offset = rgba[device->vid.vs.bits_per_pixel + 7];


    strncpy(device->vid.fs.id, VIRTGPU_ID, 16);

    device->vid.fs.smem_start  = device->vid.fb_base;
    device->vid.fs.smem_len    = device->vid.fb_size;
    device->vid.fs.type        = FB_TYPE_PLANES;
    device->vid.fs.visual      = FB_VISUAL_TRUECOLOR;
    device->vid.fs.accel       = FB_ACCEL_NONE;
    device->vid.fs.line_length = device->vid.vs.xres_virtual * (device->vid.vs.bits_per_pixel / 8);
}


/* Copy a rectangle of the framebuffer into the scanout resource and publish it.
 *
 * The transfer is the expensive half -- it is the host reading guest memory -- so the
 * rectangle matters: a compositor that damages one character cell moves a few hundred bytes
 * here, where a whole-screen push at this mode moves three and a half megabytes. The offset
 * is where the rectangle starts inside the backing, which the device needs because the
 * backing is laid out by the pitch rather than by the rectangle.
 *
 * Both halves have to reach the device as a pair, hence the lock: a flush that overtook the
 * transfer of another caller would publish a half-written frame.
 */

static void virtgpu_flush(device_t* device, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->userdata);

    struct virtgpu* gpu = device->userdata;

    if (unlikely(!gpu->fb_resource_id))
        return;


    uint64_t offset = (uint64_t)y * device->vid.fs.line_length + (uint64_t)x * (device->vid.vs.bits_per_pixel / 8);

    scoped_lock(&gpu->lock) {

        if (virtgpu_cmd_transfer_to_host_2d(gpu, gpu->fb_resource_id, offset, x, y, width, height) < 0)
            return;

        if (virtgpu_cmd_resource_flush_fenced(gpu, gpu->fb_resource_id, x, y, width, height) < 0)
            return;

        gpu->frame_flushed = true;
    }
}


/* There is no scanout vblank to wait for on this device, and claiming otherwise would be a
 * lie the caller then paces itself against. What this waits for is the completion fence of
 * the frame, which is the useful half of a vsync here: it is what stops the compositor from
 * drawing over a framebuffer the host has not finished reading.
 *
 * virtgpu_flush() already waits on that fence before it returns, so a caller that pushed its
 * damage has nothing left to wait for. A caller that pushed nothing is one that expects this
 * call alone to put its frame on screen -- the interface before FBIO_FLUSH existed -- and
 * gets the whole screen transferred, which is what it was getting before.
 */

static void virtgpu_wait_vsync(device_t* device) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->userdata);

    struct virtgpu* gpu = device->userdata;

    if (gpu->frame_flushed) {

        gpu->frame_flushed = false;
        return;
    }

    virtgpu_flush(device, 0, 0, device->vid.vs.xres, device->vid.vs.yres);

    gpu->frame_flushed = false;
}



/* The cursor plane is a resource like any other, except that the device will only take it at
 * exactly 64x64. A smaller image is placed at the origin of one and the rest left
 * transparent, so the hotspot the caller gave stays correct without adjustment.
 *
 * Created on first use rather than at reset: a system that never shows a pointer should not
 * be paying for the backing, and the display server is the only thing that asks for one.
 */

static int virtgpu_cursor_create(struct virtgpu* gpu) {

    if (likely(gpu->cursor.resource_id))
        return 0;


    size_t size = VIRTGPU_CURSOR_SIZE * VIRTGPU_CURSOR_SIZE * sizeof(uint32_t);

    uintptr_t buffer = pmm_alloc_blocks(size / PML1_PAGESIZE + 1);

    if (unlikely(!buffer))
        return errno = ENOMEM, -1;


    uint64_t resource_id = 0;

    if (virtgpu_cmd_resource_create_2d(gpu, &resource_id, VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM, VIRTGPU_CURSOR_SIZE, VIRTGPU_CURSOR_SIZE) < 0) {
        pmm_free_blocks(buffer, size / PML1_PAGESIZE + 1);
        return -1;
    }

    if (virtgpu_cmd_resource_attach_backing(gpu, resource_id, buffer, size) < 0) {

        virtgpu_cmd_resource_unref(gpu, resource_id);
        pmm_free_blocks(buffer, size / PML1_PAGESIZE + 1);

        return -1;
    }


    gpu->cursor.resource_id = resource_id;
    gpu->cursor.buffer      = buffer;
    gpu->cursor.size        = size;

    return 0;
}


static int virtgpu_cursor_set(device_t* device, const struct fb_hwcursor* cursor, const uint32_t* image) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->userdata);
    DEBUG_ASSERT(cursor);

    struct virtgpu* gpu = device->userdata;

    scoped_lock(&gpu->lock) {

        if (!(cursor->flags & FB_HWCURSOR_ENABLE)) {

            /* Resource 0 is how the device is told there is no cursor. The backing is kept:
               hiding a pointer is usually a prelude to showing it again. */
            if (virtgpu_cmd_update_cursor(gpu, VIRTGPU_DISPLAY_PRIMARY, 0, 0, 0, 0, 0) < 0)
                return -1;

            gpu->cursor.visible = false;

            return 0;
        }


        DEBUG_ASSERT(image);

        if (virtgpu_cursor_create(gpu) < 0)
            return -1;


        uint32_t* pixels = (uint32_t*)arch_vmm_p2v(gpu->cursor.buffer, ARCH_VMM_AREA_HEAP);

        memset(pixels, 0, gpu->cursor.size);

        for (uint32_t row = 0; row < cursor->height; row++)
            memcpy(&pixels[row * VIRTGPU_CURSOR_SIZE], &image[row * cursor->width], cursor->width * sizeof(uint32_t));


        if (virtgpu_cmd_transfer_to_host_2d(gpu, gpu->cursor.resource_id, 0, 0, 0, VIRTGPU_CURSOR_SIZE, VIRTGPU_CURSOR_SIZE) < 0)
            return -1;

        if (virtgpu_cmd_update_cursor(gpu, VIRTGPU_DISPLAY_PRIMARY, gpu->cursor.resource_id, cursor->x < 0 ? 0 : (uint32_t)cursor->x, cursor->y < 0 ? 0 : (uint32_t)cursor->y, cursor->hot_x, cursor->hot_y) < 0)
            return -1;


        gpu->cursor.width   = cursor->width;
        gpu->cursor.height  = cursor->height;
        gpu->cursor.hot_x   = cursor->hot_x;
        gpu->cursor.hot_y   = cursor->hot_y;
        gpu->cursor.visible = true;
    }

    return 0;
}


/* The whole reason the hardware cursor is worth having: one short command on a queue of its
 * own, no framebuffer traffic, and nothing to repaint where the pointer used to be. */

static int virtgpu_cursor_move(device_t* device, int32_t x, int32_t y) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->userdata);

    struct virtgpu* gpu = device->userdata;

    if (unlikely(!gpu->cursor.visible))
        return 0;

    scoped_lock(&gpu->lock) {

        if (virtgpu_cmd_move_cursor(gpu, VIRTGPU_DISPLAY_PRIMARY, x < 0 ? 0 : (uint32_t)x, y < 0 ? 0 : (uint32_t)y) < 0)
            return -1;
    }

    return 0;
}



static int setup_features(struct virtio_driver* driver, uint32_t* features, size_t index) {

    if (index == 0) {

        *features |= VIRTIO_GPU_F_VIRGL;
        *features |= VIRTIO_GPU_F_EDID;
    }

    if (index == 1) {

        *features |= VIRTIO_F_VERSION_1;
        *features |= VIRTIO_F_IN_ORDER;
    }

    return 0;
}



static int setup_config(struct virtio_driver* driver, uintptr_t device_config) {

    struct virtio_gpu_config volatile* cfg = (struct virtio_gpu_config volatile*)device_config;

    mmio_w32(&cfg->events_clear, cfg->events_read);
    atomic_thread_fence(memory_order_release);


#if DEBUG_LEVEL_TRACE
    kprintf("virtio-gpu: setup device configuration [scanouts(%d)]\n", cfg->num_scanouts);
#endif

    return 0;
}



static int interrupt_handler(pcidev_t device, irq_t vector, struct virtio_driver* driver) {

    struct virtio_gpu_config* cfg = (struct virtio_gpu_config*)driver->internals.device_config;

    cfg->events_clear = cfg->events_read;
    return 0;
}


static void pci_find(pcidev_t device, uint16_t vid, uint16_t did, void* arg) {
    DEBUG_ASSERT(arg);

    device_t* driver = (device_t*)arg;

    if (driver->userdata != NULL)
        return;

    if (vid != VIRTIO_PCI_VENDOR)
        return;

    if (did != VIRTIO_PCI_DEVICE(VIRTIO_DEVICE_TYPE_GPU))
        return;


    struct virtio_driver* virtio = kcalloc(1, sizeof(struct virtio_driver), GFP_KERNEL);

    virtio->type             = VIRTIO_DEVICE_TYPE_GPU;
    virtio->device           = device;
    virtio->send_window_size = 4096;
    virtio->recv_window_size = 4096;

    virtio->negotiate = &setup_features;
    virtio->setup     = &setup_config;
    virtio->interrupt = &interrupt_handler;


    if (virtio_pci_init(virtio) < 0) {

#if DEBUG_LEVEL_FATAL
        kprintf("virtio-gpu: device %d (%X:%X) initialization failed\n", device, vid, did);
#endif

        return;
    }


    struct virtgpu* gpu = kcalloc(1, sizeof(struct virtgpu), GFP_KERNEL);

    gpu->driver = virtio;

    spinlock_init(&gpu->lock);

    driver->userdata = gpu;
}


void init(const char* args) {

    if (strstr(core->boot.cmdline, "virtio=off"))
        return;

    if (strstr(core->boot.cmdline, "graphics=off"))
        return;

    if (strstr(core->boot.cmdline, "graphics=builtin"))
        return;


    pci_scan(&pci_find, PCI_TYPE_VGA, &device);

    if (device.userdata == NULL)
        return;

    device_mkdev(&device, 0644);
}

void dnit(void) {
    device_unlink(&device);
}
