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


#ifndef _ARCH_X86_SMP_H
#define _ARCH_X86_SMP_H

#include <aplus.h>
#include <aplus/ipc.h>
#include <stdint.h>

#define AP_BOOTSTRAP_MAGIC              0xCAFE1234
#define AP_BOOTSTRAP_CODE               0x1000
#define AP_BOOTSTRAP_HEADER             0x1F00

typedef struct {
    uint32_t ap_magic;
    uintptr_t ap_start;
    uintptr_t ap_end;
    uintptr_t ap_main;
    uintptr_t ap_cr3;
    uintptr_t ap_stack;
} __packed ap_bootstrap_header_t;

int ap_check(int, int);
void ap_stack(uintptr_t);
void ap_init(void);
void smp_init(void);
void smp_setup(int);

#endif