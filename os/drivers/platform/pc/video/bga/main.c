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


#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <dev/interface.h>
#include <dev/video.h>
#include <dev/pci.h>

#include <arch/x86/cpu.h>
#include <aplus/fb.h>



MODULE_NAME("video/bga");
MODULE_DEPS("dev/interface,dev/video,dev/pci");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");




#define VBE_DISPI_IOPORT_INDEX              0x01CE
#define VBE_DISPI_IOPORT_DATA               0x01CF
#define VBE_DISPI_INDEX_ID                  0x0000
#define VBE_DISPI_INDEX_XRES                0x0001
#define VBE_DISPI_INDEX_YRES                0x0002
#define VBE_DISPI_INDEX_BPP                 0x0003
#define VBE_DISPI_INDEX_ENABLE              0x0004
#define VBE_DISPI_INDEX_BANK                0x0005
#define VBE_DISPI_INDEX_VIRT_WIDTH          0x0006
#define VBE_DISPI_INDEX_VIRT_HEIGHT         0x0007
#define VBE_DISPI_INDEX_X_OFFSET            0x0008
#define VBE_DISPI_INDEX_Y_OFFSET            0x0009
#define VBE_DISPI_INDEX_VBOX_VIDEO          0x000A
#define VBE_DISPI_INDEX_FB_BASE_HI          0x000B

#define VBE_DISPI_DISABLED                  0x00
#define VBE_DISPI_ENABLED                   0x01
#define VBE_DISPI_GETCAPS                   0x02
#define VBE_DISPI_8BIT_DAC                  0x20
#define VBE_DISPI_LFB_ENABLED               0x40
#define VBE_DISPI_NOCLEARMEM                0x80

#define CHECK_BGA(n)                        (n >= 0xB0C0 || n <= 0xB0C5)

#define BGA_VIDEORAM_SIZE                   0x1000000
#define BGA_DEVICE_TYPE                     (0x0300)    /* Display : VGA Compatible */
#define BGA_ID                              "Bochs VBE Extensions"



static void bga_init(device_t*);
static void bga_dnit(device_t*);
static void bga_reset(device_t*);
static void bga_update(device_t*);


device_t device = {
    .type = DEVICE_TYPE_VIDEO,

    .name = "BGA",
    .description = BGA_ID,

    .vendorid = 0,
    .deviceid = 0,
    .intno = 0,
    .address = 0,
    .size = 0,

    .status = DEVICE_STATUS_UNKNOWN,

    .init = bga_init,
    .dnit = bga_dnit,
    .reset = bga_reset,

    .vid.update = bga_update,
};




static void bga_init(device_t* device) {
    DEBUG_ASSERT(device);    
    DEBUG_ASSERT(device->address);    
    DEBUG_ASSERT(device->size);    

    arch_mmap (
        (void*) device->address, device->size - 1,

        ARCH_MAP_FIXED  | 
        ARCH_MAP_RDWR   |
        ARCH_MAP_USER   |
        ARCH_MAP_NOEXEC |
        ARCH_MAP_VIDEO_MEMORY
    );
}


static void bga_dnit(device_t* device) {
    DEBUG_ASSERT(device);    
    DEBUG_ASSERT(device->address);    
    DEBUG_ASSERT(device->size);    

    arch_munmap((void*) device->address, device->size - 1);
}


static void bga_reset(device_t* device) {
    DEBUG_ASSERT(device);
    
    memset(&device->vid.fs, 0, sizeof(struct fb_fix_screeninfo));
    memset(&device->vid.vs, 0, sizeof(struct fb_var_screeninfo));

    device->vid.vs.xres = 800;
    device->vid.vs.yres = 600;
    device->vid.vs.xres_virtual = 800;
    device->vid.vs.yres_virtual = 600;
    device->vid.vs.bits_per_pixel = 32;
    device->vid.vs.activate = FB_ACTIVATE_NOW;
    
    outw(VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_ENABLE);
    outw(VBE_DISPI_IOPORT_DATA, VBE_DISPI_DISABLED);

}


static void bga_update(device_t* device) {
    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->address);
    DEBUG_ASSERT(device->vid.vs.xres);
    DEBUG_ASSERT(device->vid.vs.yres);
    DEBUG_ASSERT(device->vid.vs.bits_per_pixel);



    #define wr(i, v)                                        \
        outw(VBE_DISPI_IOPORT_INDEX, i);                    \
        outw(VBE_DISPI_IOPORT_DATA, v)



    wr(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);

    wr(VBE_DISPI_INDEX_XRES,        device->vid.vs.xres);
    wr(VBE_DISPI_INDEX_YRES,        device->vid.vs.yres);
    wr(VBE_DISPI_INDEX_BPP,         device->vid.vs.bits_per_pixel);
    wr(VBE_DISPI_INDEX_X_OFFSET,    device->vid.vs.xoffset);
    wr(VBE_DISPI_INDEX_Y_OFFSET,    device->vid.vs.yoffset);
    wr(VBE_DISPI_INDEX_VIRT_WIDTH,  device->vid.vs.xres_virtual);
    wr(VBE_DISPI_INDEX_VIRT_HEIGHT, device->vid.vs.yres_virtual);


    if((device->vid.vs.activate & FB_ACTIVATE_MASK) == 0)
        wr(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);
    



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

    
    strncpy(device->vid.fs.id, BGA_ID, 16);
    device->vid.fs.smem_start = device->address;
    device->vid.fs.smem_len = device->size;
    device->vid.fs.type = FB_TYPE_PLANES;
    device->vid.fs.visual = FB_VISUAL_TRUECOLOR;
    device->vid.fs.line_length = device->vid.vs.xres_virtual * (device->vid.vs.bits_per_pixel / 8);

}


static void pci_find(pcidev_t device, uint16_t vid, uint16_t did, void* arg) {
    
    if(likely(!(
        ((vid == 0x1234) || (vid == 0x80EE)) &&
        ((did == 0x1111) || (did == 0xBEEF))
    ))) return;

    ((device_t*) arg)->vendorid = vid;
    ((device_t*) arg)->deviceid = did;
    ((device_t*) arg)->intno = pci_read(device, PCI_INTERRUPT_LINE, 1);
    ((device_t*) arg)->address = pci_read(device, PCI_BAR0, 4) & PCI_BAR_MM_MASK;
    ((device_t*) arg)->size = pci_bar_size(device, PCI_BAR0, PCI_BAR_MM_MASK);

#if defined(__x86_64__)
    if(pci_read(device, PCI_BAR0, 4) & 4)
        ((device_t*) arg)->address |= (pci_read(device, PCI_BAR1, 4) << 32);
#endif

}


void init(const char* args) {
    DEBUG_ASSERT(args);

    if(strstr(args, "graphics=no"))
        return;

    
    outw(VBE_DISPI_IOPORT_INDEX, 0);

    int n = inw(VBE_DISPI_IOPORT_DATA);
    if(!(CHECK_BGA(n)))
        return;


    pci_scan(&pci_find, BGA_DEVICE_TYPE, &device);
    
    DEBUG_ASSERT(device.vendorid);
    DEBUG_ASSERT(device.deviceid);
    DEBUG_ASSERT(device.address);


    device_mkdev(&device, 0644);
}

void dnit(void) {
    device_unlink(&device);
}