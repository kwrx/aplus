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


// TODO: Rewrite all virtio-gpu code to use a new video interface driver


MODULE_NAME("virtio/virtio-gpu");
MODULE_DEPS("dev/interface,dev/pci,virtio/virtio-pci,virtio/virtio-queue");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");



#define VIRTGPU_ID "VIRTIO-GPU"

#define VIRTGPU_FB_RESID        1
#define VIRTGPU_DISPLAY_PRIMARY 0



static void virtgpu_init(device_t*);
static void virtgpu_dnit(device_t*);
static void virtgpu_reset(device_t*);
static void virtgpu_update(device_t*);
static void virtgpu_wait_vsync(device_t*);


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

    .vid.update     = virtgpu_update,
    .vid.wait_vsync = virtgpu_wait_vsync,

};



static void virtgpu_init(device_t* device) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->vid.fb_base == 0);
    DEBUG_ASSERT(device->vid.fb_size == 0);


    virtgpu_reset(device);
}


static void virtgpu_dnit(device_t* device) {

    DEBUG_ASSERT(device);

    if (unlikely(device->vid.fs.smem_start))
        pmm_free_blocks(device->vid.fs.smem_start, device->vid.fs.smem_len / PML1_PAGESIZE + 1);
}



static void virtgpu_reset_framebuffer(device_t* device) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->userdata);


#if DEBUG_LEVEL_ERROR
    #define __(cmd, args...)                                             \
        {                                                                \
            kprintf("virtio-gpu: " #cmd "\n");                           \
            int e;                                                       \
            if ((e = cmd(args)) < 0) {                                   \
                kprintf("virtio-gpu: ERROR! %s failed: %d\n", #cmd, -e); \
                device->status = DEVICE_STATUS_FAILED;                   \
                return;                                                  \
            }                                                            \
        }
#else
    #define __(cmd, args...)                           \
        {                                              \
            if (cmd(args) < 0) {                       \
                device->status = DEVICE_STATUS_FAILED; \
                return;                                \
            }                                          \
        }
#endif


    // uint64_t resource;

    // __(virtgpu_cmd_resource_create_2d,         device->userdata, &resource, VIRTIO_GPU_FORMAT_A8R8G8B8_UNORM, device->vid.vs.xres_virtual, device->vid.vs.yres_virtual);
    // __(virtgpu_cmd_resource_attach_backing,    device->userdata, resource, device->vid.fb_base, device->vid.fb_size);
    // __(virtgpu_cmd_set_scanout,                device->userdata, VIRTGPU_DISPLAY_PRIMARY, resource, 0, 0, device->vid.vs.xres, device->vid.vs.yres);


    struct virtio_gpu_resp_display_info display_info;

    __(virtgpu_cmd_get_display_info, device->userdata, &display_info);
    __(virtgpu_cmd_get_display_info, device->userdata, &display_info);
    __(virtgpu_cmd_get_display_info, device->userdata, &display_info);
    __(virtgpu_cmd_get_display_info, device->userdata, &display_info);
    __(virtgpu_cmd_get_display_info, device->userdata, &display_info);

    for (;;)
        ;

#undef __
}



static void virtgpu_reset(device_t* device) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->userdata);


    if (unlikely(device->vid.fs.smem_start))
        pmm_free_blocks(device->vid.fs.smem_start, device->vid.fs.smem_len / PML1_PAGESIZE + 1);


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


    device->vid.fb_base = pmm_alloc_blocks(device->vid.vs.xres_virtual * device->vid.vs.yres_virtual * device->vid.vs.bits_per_pixel / 8 / PML1_PAGESIZE + 1);
    device->vid.fb_size = device->vid.vs.xres_virtual * device->vid.vs.yres_virtual * device->vid.vs.bits_per_pixel / 8;


    virtgpu_reset_framebuffer(device);
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


static void virtgpu_wait_vsync(device_t* device) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->userdata);

    virtgpu_cmd_transfer_to_host_2d(device->userdata, VIRTGPU_FB_RESID, 0, 0, 0, device->vid.vs.xres, device->vid.vs.yres);
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

    kprintf("!!!!!!!!!!!!!!!RECEIVED MSI-X INTERRUPT!!!!!!!!!!!!!!!!!!: %p\n", device);


    return 0;
}


static void pci_find(pcidev_t device, uint16_t vid, uint16_t did, void* arg) {


    device_t* driver = (device_t*)arg;

    if (driver->userdata != NULL)
        return;


    if (vid != VIRTIO_PCI_VENDOR)
        return;

    if (did != VIRTIO_PCI_DEVICE(VIRTIO_DEVICE_TYPE_GPU))
        return;



    struct virtio_driver* virtio = kmalloc(sizeof(struct virtio_driver), GFP_KERNEL);

    memset(virtio, 0, sizeof(struct virtio_driver));


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

    gpu->driver      = virtio;
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
