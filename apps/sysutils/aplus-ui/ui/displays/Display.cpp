/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2022 Antonino Natale
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


#include <iostream>
#include <vector>
#include <functional>
#include <sstream>
#include <unistd.h>
#include <fcntl.h>
#include <aplus/fb.h>

#include "../UI.hpp"


using namespace aplus;
using namespace aplus::ui;
using namespace aplus::ui::displays;


std::vector<Display> Display::__displays {};


void Display::init(void) {


    for(size_t k = 0; k < UIDefaultConfig.display.limits.displays; k++) {

        std::stringstream device {};
        device << UIDefaultConfig.display.device;
        device << k;

        if(access(device.str().c_str(), R_OK | W_OK) < 0)
            continue;

        
        LOG << "initalizing display #" << k << std::endl;



        int fd;
        struct fb_var_screeninfo var;
        struct fb_fix_screeninfo fix;


        if((fd = open(device.str().c_str(), O_RDWR)) < 0) {
            perror(device.str().c_str());
            exit(1);
        }


        if(ioctl(fd, FBIOGET_VSCREENINFO, &var) < 0) {
            perror("FBIOGET_VSCREENINFO");
            exit(1);
        }

        if(ioctl(fd, FBIOGET_FSCREENINFO, &fix) < 0) {
            perror("FBIOGET_FSCREENINFO");
            exit(1);
        }

        close(fd);


        LOG << "found " << var.xres << "x" << var.yres << "x" << var.bits_per_pixel
                        << " at " << fix.smem_start << std::endl;



        __displays.emplace_back(k, 0, 0, var.xres, var.yres, (void*) fix.smem_start);

    }

}

void Display::query(std::function<void(Display&)> select) {

    for(auto& d : __displays) {
        select(d);
    }

}


Display& Display::primary() {
    return __displays.front();
}




void Display::present() const {

}

void Display::present(uint16_t off_x, uint16_t off_y, uint16_t off_w, uint16_t off_h) const {

}