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
#include <aplus/errno.h>
#include <stdint.h>
#include <string.h>

#include <dev/interface.h>
#include <dev/network.h>


MODULE_NAME("pc/network/pcnet");
MODULE_DEPS("dev/interface,dev/network");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");



device_t device = {

    .type = DEVICE_TYPE_NETWORK,

    .name = "pcnet",
    .description = "PCNET Network Adapter",

    .major = 144,
    .minor = 0,

    .status = DEVICE_STATUS_UNKNOWN,

    .init  = NULL,
    .dnit  = NULL,
    .reset = NULL,

    .netif.low_level_init           = NULL,
    .netif.low_level_startoutput    = NULL,
    .netif.low_level_output         = NULL,
    .netif.low_level_endoutput      = NULL,
    .netif.low_level_startinput     = NULL,
    .netif.low_level_input          = NULL,
    .netif.low_level_endinput       = NULL,
    .netif.low_level_input_nomem    = NULL,

};


void init(const char* args) {
    device_mkdev(&device, 0666);
}


void dnit(void) {
    device_unlink(&device);
}