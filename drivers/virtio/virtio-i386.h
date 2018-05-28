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


#if defined(__i386__)
#include <arch/i386/i386.h>

uint8_t virtio_r8(uint16_t p) {
    return inb(p);
}

uint16_t virtio_r16(uint16_t p) {
    return inw(p);
}

uint32_t virtio_r32(uint16_t p) {
    return inl(p);
}

void virtio_w8(uint16_t p, uint8_t v) {
    outb(p, v);
}

void virtio_w16(uint16_t p, uint16_t v) {
    outw(p, v);
}

void virtio_w32(uint16_t p, uint32_t v) {
    outl(p, v);
}

#endif