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


#ifndef _MMIO_H
#define _MMIO_H

#ifndef __ASSEMBLY__

#define mmio_r8(x)              (*(volatile uint8_t*) (x))
#define mmio_r16(x)             (*(volatile uint16_t*) (x))
#define mmio_r32(x)             (*(volatile uint32_t*) (x))
#define mmio_r64(x)             (*(volatile uint64_t*) (x))

#define mmio_w8(x, y)           { mmio_r8(x) = (uint8_t) (y); }
#define mmio_w16(x, y)          { mmio_r16(x) = (uint16_t) (y); }
#define mmio_w32(x, y)          { mmio_r32(x) = (uint32_t) (y); }
#define mmio_w64(x, y)          { mmio_r64(x) = (uint64_t) (y); }

#endif

#endif
