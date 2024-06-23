/*
 * GPL3 License
 *
 * Author(s):
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 *
 * Copyright (c) 2013-2019 Antonino Natale
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
#include <aplus/hal.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>

#include <arch/x86/asm.h>
#include <arch/x86/cpu.h>
#include <arch/x86/intr.h>
#include <arch/x86/vmm.h>


__nonnull(1) void arch_vmm_lock(vmm_address_space_t* space, uintptr_t virtaddr, size_t size) {

    DEBUG_ASSERT(space->pm);
    DEBUG_ASSERT(size > 0);

    (void)space;
    (void)virtaddr;
    (void)size;

    // TODO: implement a better way to lock a region of memory

#if defined(CONFIG_X86_ENABLE_SMAP)
    if (cpu_has(current_cpu->id, X86_FEATURE_SMAP))
        x86_set_cr4(x86_get_cr4() & ~(X86_CR4_SMAP_MASK));
#endif
}
