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

#include <stdint.h>
#include <string.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/module.h>
#include <aplus/memory.h>
#include <aplus/smp.h>
#include <aplus/hal.h>
#include <aplus/fb.h>
#include <aplus/errno.h>

#include <dev/interface.h>
#include <dev/video.h>
#include <dev/pci.h>

#include <dev/virtio/virtio.h>
#include <dev/virtio/virtio-gpu.h>


MODULE_NAME("virtio/virtio-gpu");
MODULE_DEPS("dev/interface,dev/pci,virtio/virtio-pci,virtio/virtio-queue");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");



#define VIRTGPU_ID              "VIRTIO-GPU"




static void virtgpu_init(device_t*);
static void virtgpu_dnit(device_t*);
static void virtgpu_reset(device_t*);
static void virtgpu_update(device_t*);


device_t device = {

    .type = DEVICE_TYPE_VIDEO,

    .name = "fb0",
    .description = "VIRTIO GPU Device",

    .major = 10,
    .minor = 243,

    .address = 0,
    .size = 0,

    .status = DEVICE_STATUS_UNKNOWN,

    .init = virtgpu_init,
    .dnit = virtgpu_dnit,
    .reset = virtgpu_reset,

    .vid.update = virtgpu_update,

};





static void virtgpu_init(device_t* device) {

    DEBUG_ASSERT(device);    
    DEBUG_ASSERT(device->address == 0);    
    DEBUG_ASSERT(device->size == 0);    

    
    virtgpu_reset(device);

}


static void virtgpu_dnit(device_t* device) {

    DEBUG_ASSERT(device);    

    // virtgpu_free_buffers();
    // virtgpu_free_resources();

}


static void virtgpu_reset(device_t* device) {

    DEBUG_ASSERT(device);
    
    memset(&device->vid.fs, 0, sizeof(struct fb_fix_screeninfo));
    memset(&device->vid.vs, 0, sizeof(struct fb_var_screeninfo));

    if(!core->framebuffer.address) {

        device->vid.vs.xres = 800;
        device->vid.vs.yres = 600;
        device->vid.vs.xres_virtual = 800;
        device->vid.vs.yres_virtual = 600;
        device->vid.vs.bits_per_pixel = 32;

    } else {

        device->vid.vs.xres = core->framebuffer.width;
        device->vid.vs.yres = core->framebuffer.height;
        device->vid.vs.xres_virtual = core->framebuffer.width;
        device->vid.vs.yres_virtual = core->framebuffer.height;
        device->vid.vs.bits_per_pixel = core->framebuffer.depth;

    }


    device->vid.vs.activate = FB_ACTIVATE_NOW;
    

    // virtgpu_free_buffers();
    // virtgpu_free_resources();

}


static void virtgpu_update(device_t* device) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->vid.vs.xres);
    DEBUG_ASSERT(device->vid.vs.yres);
    DEBUG_ASSERT(device->vid.vs.bits_per_pixel);


    // virtgpu_update_buffers();
    


    static int rgba[] = {
        0, 0, 0, 0, 0, 0, 0, 0,     /* NULL             */
        3, 5, 3, 2, 2, 0, 0, 0,     /* 8bpp RRRGGGBB    */
        5, 11, 6, 6, 5, 0, 0, 0,    /* 16bpp RGB565     */
        8, 16, 8, 8, 8, 0, 0, 0,    /* 24bpp RGB24      */
        8, 16, 8, 8, 8, 0, 8, 24,   /* 32bpp ARGB       */
    };

    device->vid.vs.red.length =     rgba[device->vid.vs.bits_per_pixel + 0];
    device->vid.vs.red.offset =     rgba[device->vid.vs.bits_per_pixel + 1];
    device->vid.vs.green.length =   rgba[device->vid.vs.bits_per_pixel + 2];
    device->vid.vs.green.offset =   rgba[device->vid.vs.bits_per_pixel + 3];
    device->vid.vs.blue.length =    rgba[device->vid.vs.bits_per_pixel + 4];
    device->vid.vs.blue.offset =    rgba[device->vid.vs.bits_per_pixel + 5];
    device->vid.vs.transp.length =  rgba[device->vid.vs.bits_per_pixel + 6];
    device->vid.vs.transp.offset =  rgba[device->vid.vs.bits_per_pixel + 7];

    
    strncpy(device->vid.fs.id, VIRTGPU_ID, 16);

    device->vid.fs.smem_start = device->address;
    device->vid.fs.smem_len = device->size;
    device->vid.fs.type = FB_TYPE_PLANES;
    device->vid.fs.visual = FB_VISUAL_TRUECOLOR;
    device->vid.fs.line_length = device->vid.vs.xres_virtual * (device->vid.vs.bits_per_pixel / 8);


}






static int setup_features(struct virtio_driver* driver, uint32_t* features, size_t index) {
    
    if(index == 0) {

        *features |= VIRTIO_GPU_F_VIRGL;
        *features |= VIRTIO_GPU_F_EDID;

    }

    if(index == 1) {

        //*features |= VIRTIO_F_IN_ORDER;

    }

    return 0;
    
}



static int setup_config(struct virtio_driver* driver, uintptr_t device_config) {
    
    struct virtio_gpu_config volatile* cfg = (struct virtio_gpu_config volatile*) device_config;

    __atomic_store_n(&cfg->events_clear, cfg->events_read, __ATOMIC_SEQ_CST);

    return 0;

}



static int interrupt_handler(void* frame, uint8_t irq, struct virtio_driver* driver) {
    return kprintf("!!!!!!!!!!!!!!!RECEIVED MSI-X INTERRUPT!!!!!!!!!!!!!!!!!!\n"), 0;
}


static void pci_find(pcidev_t device, uint16_t vid, uint16_t did, void* arg) {

    if(vid != VIRTIO_PCI_VENDOR)
        return;

    if(did != VIRTIO_PCI_DEVICE(VIRTIO_DEVICE_TYPE_GPU))
        return;


    //DEBUG_ASSERT(((device_t*) arg)->userdata);



    struct virtio_driver* driver = kmalloc(sizeof(struct virtio_driver), GFP_KERNEL);

    memset(driver, 0, sizeof(struct virtio_driver));


    driver->type = VIRTIO_DEVICE_TYPE_GPU;
    driver->device = device;
    driver->send_window_size = 512;
    driver->recv_window_size = 512;

    driver->negotiate = &setup_features;
    driver->setup = &setup_config;
    driver->interrupt = &interrupt_handler;


    if(virtio_pci_init(driver) < 0) {

#if defined(DEBUG) && DEBUG_LEVEL >= 0
        kprintf("virtio-gpu: device %d (%X:%X) initialization failed\n", device, vid, did);
#endif

        return;

    }


    struct virtio_gpu_resp_display_info info = { 0 };
    struct virtio_gpu_ctrl_hdr cmd = { 0 };


    cmd.type = VIRTIO_GPU_CMD_GET_DISPLAY_INFO;
    
    virtq_sendrecv(driver, 0, &cmd, sizeof(cmd), &info, sizeof(info) + 100);
    

    kprintf("GPU: type: %p, enabled: %p, flags: %p, x: %d, y: %d, w: %d, h: %d\n", info.hdr.type, info.pmodes[0].enabled, info.pmodes[0].flags,
        info.pmodes[0].r.x, 
        info.pmodes[0].r.y, 
        info.pmodes[0].r.width, 
        info.pmodes[0].r.height); 

    for(;;);
    //((device_t*) arg)->userdata = driver;

}


void init(const char* args) {

    if(args && strstr(args, "virtio=disable"))
        return;

    pci_scan(&pci_find, PCI_TYPE_VGA, &device);

}

void dnit(void) {

}