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

#include <arch/x86/cpu.h>



MODULE_NAME("video/builtin");
MODULE_DEPS("dev/interface,dev/video");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");



static void builtin_init(device_t*);
static void builtin_dnit(device_t*);
static void builtin_reset(device_t*);


device_t device = {

    .type = DEVICE_TYPE_VIDEO,

    .name        = "fb0",
    .description = "Video Builtin Adapter",

    .major = 10,
    .minor = 242,

    .status = DEVICE_STATUS_UNKNOWN,

    .init  = builtin_init,
    .dnit  = builtin_dnit,
    .reset = builtin_reset,

};



static void builtin_init(device_t* device) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->vid.fb_base);
    DEBUG_ASSERT(device->vid.fb_size);

    arch_vmm_map(&core->bsp.address_space, device->vid.fb_base, device->vid.fb_base, device->vid.fb_size, ARCH_VMM_MAP_FIXED | ARCH_VMM_MAP_RDWR | ARCH_VMM_MAP_USER | ARCH_VMM_MAP_NOEXEC | ARCH_VMM_MAP_SHARED | ARCH_VMM_MAP_VIDEO_MEMORY);

    builtin_reset(device);
}


static void builtin_dnit(device_t* device) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->vid.fb_base);
    DEBUG_ASSERT(device->vid.fb_size);

    arch_vmm_unmap(&core->bsp.address_space, device->vid.fb_base, device->vid.fb_size);
}


static void builtin_reset(device_t* device) {

    DEBUG_ASSERT(device);

    memset(&device->vid.fs, 0, sizeof(struct fb_fix_screeninfo));
    memset(&device->vid.vs, 0, sizeof(struct fb_var_screeninfo));

    device->vid.vs.xres           = core->framebuffer.width;
    device->vid.vs.yres           = core->framebuffer.height;
    device->vid.vs.xres_virtual   = core->framebuffer.width;
    device->vid.vs.yres_virtual   = core->framebuffer.height;
    device->vid.vs.bits_per_pixel = core->framebuffer.depth;
    device->vid.vs.activate       = FB_ACTIVATE_NOW;



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


    strncpy(device->vid.fs.id, "Builtin Video", 16);

    device->vid.fs.smem_start  = device->vid.fb_base;
    device->vid.fs.smem_len    = device->vid.fb_size;
    device->vid.fs.type        = FB_TYPE_PLANES;
    device->vid.fs.visual      = FB_VISUAL_TRUECOLOR;
    device->vid.fs.line_length = device->vid.vs.xres_virtual * (device->vid.vs.bits_per_pixel / 8);
}



void init(const char* args) {

    if (strstr(core->boot.cmdline, "graphics=off"))
        return;

    if (strstr(core->boot.cmdline, "graphics=builtin") == NULL)
        return;

    if (unlikely(!core->framebuffer.address))
        return;


    device.vid.fb_base = core->framebuffer.address;
    device.vid.fb_size = core->framebuffer.pitch * core->framebuffer.height;

    device_mkdev(&device, 0644);
}


void dnit(void) {
    device_unlink(&device);
}
