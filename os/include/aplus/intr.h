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


#ifndef _APLUS_INTR_H
#define _APLUS_INTR_H

#if defined(KERNEL)
long arch_intr_disable();
void arch_intr_enable(long);

int arch_intr_map_irq(uint8_t irq, void* (*handler) (void*));
int arch_intr_unmap_irq(uint8_t irq);

void arch_intr_begin(void);
void arch_intr_end(void);
#endif

#endif