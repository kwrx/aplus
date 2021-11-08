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
#include <aplus/module.h>
#include <aplus/memory.h>
#include <aplus/smp.h>
#include <aplus/hal.h>
#include <aplus/fb.h>
#include <aplus/errno.h>

#include <dev/interface.h>
#include <dev/video.h>
#include <dev/pci.h>

#include <arch/x86/cpu.h>




MODULE_NAME("video/vmware");
MODULE_DEPS("dev/interface,dev/video,dev/pci");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");




#define SVGA_INDEX_PORT                         0x00
#define SVGA_VALUE_PORT                         0x01

#define SVGA_REG_ID                             0x00
#define SVGA_REG_ENABLE                         0x01
#define SVGA_REG_WIDTH                          0x02
#define SVGA_REG_HEIGHT                         0x03
#define SVGA_REG_MAX_WIDTH                      0x04
#define SVGA_REG_MAX_HEIGHT                     0x05
#define SVGA_REG_DEPTH                          0x06
#define SVGA_REG_BITS_PER_PIXEL                 0x07
#define SVGA_REG_PSEUDOCOLOR                    0x08
#define SVGA_REG_RED_MASK                       0x09
#define SVGA_REG_GREEN_MASK                     0x0A
#define SVGA_REG_BLUE_MASK                      0x0B
#define SVGA_REG_BYTES_PER_LINE                 0x0C
#define SVGA_REG_FB_START                       0x0D
#define SVGA_REG_FB_OFFSET                      0x0E
#define SVGA_REG_FB_PAGE                        0x10
#define SVGA_REG_VRAM_SIZE                      0x11
#define SVGA_REG_FB_SIZE                        0x12
#define SVGA_REG_FB_MAX_SIZE                    0x13
#define SVGA_REG_LOGICAL_WIDTH                  0x14
#define SVGA_REG_LOGICAL_HEIGHT                 0x15
#define SVGA_REG_COMMAND_TYPE                   0x16
#define SVGA_REG_COMMAND                        0x17
#define SVGA_REG_COMMAND_RESULT                 0x18
#define SVGA_REG_SYNC                           0x6F
#define SVGA_REG_BUSY                           0x3E

#define SVGA_FALLBACK_XRES                      1280
#define SVGA_FALLBACK_YRES                      720
#define SVGA_FALLBACK_BPP                       32




#define VMWARE_WR(d, a, b) {                                    \
    outl(SVGA_INDEX_PORT + d->iobase, a);                       \
    outl(SVGA_VALUE_PORT + d->iobase, b);                       \
}

#define VMWARE_RD(d, a) ({                                      \
    outl(SVGA_INDEX_PORT + d->iobase, a);                       \
    inl(SVGA_VALUE_PORT + d->iobase);                           \
})


static void vmware_init(device_t*);
static void vmware_dnit(device_t*);
static void vmware_reset(device_t*);
static void vmware_update(device_t*);


device_t device = {

    .type = DEVICE_TYPE_VIDEO,

    .name = "fb0",
    .description = "Vmware VGA Extensions",

    .major = 10,
    .minor = 243,

    .address = 0,
    .size = 0,

    .status = DEVICE_STATUS_UNKNOWN,

    .init = vmware_init,
    .dnit = vmware_dnit,
    .reset = vmware_reset,

    .vid.update = vmware_update,

};




static void vmware_init(device_t* device) {

    DEBUG_ASSERT(device);    
    DEBUG_ASSERT(device->iobase);    


    device->address = VMWARE_RD(device, SVGA_REG_FB_START);
    device->size    = VMWARE_RD(device, SVGA_REG_FB_SIZE) - device->address;

    arch_vmm_map(&core->bsp.address_space, device->address, device->address, device->size,
            ARCH_VMM_MAP_FIXED  | 
            ARCH_VMM_MAP_RDWR   |
            ARCH_VMM_MAP_USER   |
            ARCH_VMM_MAP_NOEXEC |
            ARCH_VMM_MAP_SHARED |
            ARCH_VMM_MAP_VIDEO_MEMORY
    );

    vmware_reset(device);

}


static void vmware_dnit(device_t* device) {

    DEBUG_ASSERT(device);    
    DEBUG_ASSERT(device->iobase);    

    arch_vmm_unmap(&core->bsp.address_space, device->address, device->size);

}


static void vmware_reset(device_t* device) {

    DEBUG_ASSERT(device);
    
    memset(&device->vid.fs, 0, sizeof(struct fb_fix_screeninfo));
    memset(&device->vid.vs, 0, sizeof(struct fb_var_screeninfo));


    if(!core->framebuffer.address) {

        device->vid.vs.xres = SVGA_FALLBACK_XRES;
        device->vid.vs.yres = SVGA_FALLBACK_XRES;
        device->vid.vs.xres_virtual = SVGA_FALLBACK_XRES;
        device->vid.vs.yres_virtual = SVGA_FALLBACK_YRES;
        device->vid.vs.bits_per_pixel = SVGA_FALLBACK_BPP;

    } else {

        device->vid.vs.xres = core->framebuffer.width;
        device->vid.vs.yres = core->framebuffer.height;
        device->vid.vs.xres_virtual = core->framebuffer.width;
        device->vid.vs.yres_virtual = core->framebuffer.height;
        device->vid.vs.bits_per_pixel = core->framebuffer.depth;

    }


    device->vid.vs.activate = FB_ACTIVATE_NOW;

}


static void vmware_update(device_t* device) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->iobase);    
    DEBUG_ASSERT(device->vid.vs.xres);
    DEBUG_ASSERT(device->vid.vs.yres);
    DEBUG_ASSERT(device->vid.vs.bits_per_pixel);




    VMWARE_WR(device, SVGA_REG_ENABLE, 0);
    VMWARE_WR(device, SVGA_REG_ID, 0);
    VMWARE_WR(device, SVGA_REG_WIDTH, device->vid.vs.xres);
    VMWARE_WR(device, SVGA_REG_HEIGHT, device->vid.vs.yres);
    VMWARE_WR(device, SVGA_REG_BITS_PER_PIXEL, device->vid.vs.bits_per_pixel);

    if((device->vid.vs.activate & FB_ACTIVATE_MASK) == 0)
        VMWARE_WR(device, SVGA_REG_ENABLE, 1);

    

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

    
    strncpy(device->vid.fs.id, "VMWARE", 16);

    device->vid.fs.smem_start = device->address;
    device->vid.fs.smem_len = device->size;
    device->vid.fs.type = FB_TYPE_PLANES;
    device->vid.fs.visual = FB_VISUAL_TRUECOLOR;
    device->vid.fs.line_length = device->vid.vs.xres_virtual * (device->vid.vs.bits_per_pixel / 8);


}


static void pci_find(pcidev_t device, uint16_t vid, uint16_t did, void* arg) {
    
    if(likely(!(vid == 0x15AD && did == 0x0405)))
        return;


    ((device_t*) arg)->iobase  = pci_read(device, PCI_BAR0, 4) & PCI_BAR_IO_MASK;

    pci_enable_bus_mastering(device);
    pci_enable_mmio(device);
    pci_enable_pio(device);

}


void init(const char* args) {

    if(args && strstr(args, "graphics=no"))
        return;

    if(args && strstr(args, "graphics=builtin"))
        return;


    pci_scan(&pci_find, PCI_TYPE_VGA, &device);

    if(!device.iobase)
        return;

    device_mkdev(&device, 0644);

}


void dnit(void) {
    device_unlink(&device);
}