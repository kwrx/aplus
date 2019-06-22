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
#include <aplus/mm.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>

#include <dev/interface.h>
#include <dev/network.h>

#ifndef ETHERNETIF_MAXFRAMES
#define ETHERNETIF_MAXFRAMES            1
#endif



MODULE_NAME("dev/network");
MODULE_DEPS("");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


#if 0
void ethif_init(device_t* device) {
    DEBUG_ASSERT(device);
    
}


void ethif_dnit(device_t* device) {
    DEBUG_ASSERT(device);
   
}

#endif
void init(const char* args) {
    (void) args;
}

void dnit(void) {

}