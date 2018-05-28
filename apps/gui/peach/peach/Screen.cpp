/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2018 Antonino Natale
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


#include <peach/peach.h>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <aplus/base.h>
#include <aplus/fb.h>
#include <aplus/sysconfig.h>
using namespace std;


Screen::Screen(Peach* c) {
    if(!this->GetMode(&this->backupWidth, &this->backupHeight, &this->backupBitsPerPixel))
        peach_die("Screen->GetMode()");

    this->context = c;
}

Screen::~Screen() {
    this->SetMode(this->backupWidth, this->backupHeight, this->backupBitsPerPixel);
    delete this->Framebuffer;
}

bool Screen::SetMode(uint16_t w, uint16_t h, uint16_t bpp) {
    char* dev = getenv("FRAMEBUFFER");
    if(!dev)
        dev = (char*) sysconfig("screen.device", "/dev/fb0");

    int fd = open(dev, O_RDONLY);
    if(fd < 0)
        peach_die("framebuffer");


    struct fb_var_screeninfo var;
    struct fb_fix_screeninfo fix;
    ioctl(fd, FBIOGET_VSCREENINFO, &var);
    var.xres =
    var.xres_virtual = w;
    var.yres =
    var.yres_virtual = h;
    var.bits_per_pixel = bpp;
    var.activate = FB_ACTIVATE_NOW;
    ioctl(fd, FBIOPUT_VSCREENINFO, &var);
    ioctl(fd, FBIOGET_VSCREENINFO, &var);
    ioctl(fd, FBIOGET_FSCREENINFO, &fix);
    close(fd);

    this->Width = var.xres;
    this->Height = var.yres;
    this->BitsPerPixel = var.bits_per_pixel;
    this->Framebuffer = new Renderer(this->context, var.xres, var.yres, (void*) fix.smem_start);
    return true;
}

bool Screen::GetMode(uint16_t* w, uint16_t* h, uint16_t* bpp) {
    char* dev = getenv("FRAMEBUFFER");
    if(!dev)
        dev = (char*) sysconfig("screen.device", "/dev/fb0");

    int fd = open(dev, O_RDONLY);
    if(fd < 0)
        peach_die("framebuffer");


    struct fb_var_screeninfo var;
    struct fb_fix_screeninfo fix;
    ioctl(fd, FBIOGET_VSCREENINFO, &var);
    ioctl(fd, FBIOGET_FSCREENINFO, &fix);
    close(fd);


    if(w)
        *w = (uint16_t) var.xres;
    if(h)
        *h = (uint16_t) var.yres;
    if(bpp)
        *bpp = (uint16_t) var.bits_per_pixel;


    return true;
}