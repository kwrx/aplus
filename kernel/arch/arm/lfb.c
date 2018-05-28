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


#include <aplus.h>
#include <aplus/mmio.h>
#include <aplus/debug.h>
#include <libc.h>
#include "arm.h"

uint32_t lfbio[] = {
    LFB_WIDTH,
    LFB_HEIGHT,
    LFB_WIDTH,
    LFB_HEIGHT,
    0,
    LFB_DEPTH,
    0,
    0,
    0,
    0,
};



int lfb_init() {
    int i;
    uint32_t* p = (uint32_t*) &lfbio;

    for(i = 0; i < sizeof(lfbio); i += sizeof(uint32_t))
        mmio_w32(LFBIO_BASE + i, *p++);

    mail_send(LFBIO_BASE, LFBIO_BOX);
    mail_recv(LFBIO_BOX);


    mbd->lfb.width = mmio_r32(LFBIO_BASE + 0);
    mbd->lfb.height = mmio_r32(LFBIO_BASE + 4);
    mbd->lfb.pitch = mmio_r32(LFBIO_BASE + 16);
    mbd->lfb.depth = mmio_r32(LFBIO_BASE + 20);    
    mbd->lfb.base = mmio_r32(LFBIO_BASE + 32);
    mbd->lfb.size = mmio_r32(LFBIO_BASE + 36);

#ifdef DEBUG
    memset((void*) mbd->lfb.base, 0xFF, mbd->lfb.size);
#endif

    kprintf(LOG "lfb: %dx%dx%d at 0x%x\n", mbd->lfb.width, mbd->lfb.height, mbd->lfb.depth, mbd->lfb.base);

    return 0;
}
