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



/*!
 * @brief arch_vmm_getpagesize().
 *        Get page size.
 */
uintptr_t arch_vmm_getpagesize() {
    return X86_MMU_PAGESIZE;
}


/*!
 * @brief arch_vmm_gethugepagesize().
 *        Get huge page size.
 */
uintptr_t arch_vmm_gethugepagesize(uint64_t huge_type) {

    switch (huge_type) {

        case ARCH_VMM_MAP_HUGE_2MB:
            return X86_MMU_HUGE_2MB_PAGESIZE;
        case ARCH_VMM_MAP_HUGE_1GB:
            return X86_MMU_HUGE_1GB_PAGESIZE;
    }

    kpanicf("arch_vmm_gethugepagesize(): PANIC! unsupported huge_type: %ld\n", huge_type);
    return 0UL;
}



/*!
 * @brief arch_vmm_p2v().
 *        Convert a physical address to virtual one.
 *
 * @param physaddr: physical address.
 * @param type: type of memory area.
 */
uintptr_t arch_vmm_p2v(uintptr_t physaddr, int type) {

    switch (type) {

        case ARCH_VMM_AREA_HEAP:
            return physaddr + KERNEL_HEAP_AREA;

        case ARCH_VMM_AREA_KERNEL:
            return physaddr + KERNEL_HIGH_AREA;
    }

    PANIC_ASSERT(0);
    return -1;
}


/*!
 * @brief arch_vmm_v2p().
 *        Convert a virtual address to physical one.
 *
 * @param virtaddr: virtual address.
 * @param type: type of memory area.
 */
uintptr_t arch_vmm_v2p(uintptr_t virtaddr, int type) {

    switch (type) {

        case ARCH_VMM_AREA_HEAP:
            return virtaddr - KERNEL_HEAP_AREA;

        case ARCH_VMM_AREA_KERNEL:
            return virtaddr - KERNEL_HIGH_AREA;

        case ARCH_VMM_AREA_USER:
            return arch_vmm_getphysaddr(current_task->address_space, virtaddr);
    }

    PANIC_ASSERT(0);
    return -1;
}



TEST(x86_vmm_test, {
    uintptr_t addr = 0x12345678;
    uintptr_t phys = arch_vmm_v2p(addr, ARCH_VMM_AREA_KERNEL);
    uintptr_t virt = arch_vmm_p2v(phys, ARCH_VMM_AREA_KERNEL);

    DEBUG_ASSERT(addr == virt);
});
