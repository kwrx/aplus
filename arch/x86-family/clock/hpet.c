/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2019 Antonino Natale
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

#include <stdint.h>
#include <string.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>

#include <arch/x86/acpi.h>
#include <arch/x86/asm.h>
#include <arch/x86/cpu.h>
#include <arch/x86/time.h>

#define HPET_TICK 1000000000000000

#define HPET_GENERAL_GCID    hpet_address + 0x00
#define HPET_GENERAL_CR      hpet_address + 0x10
#define HPET_GENERAL_ISR     hpet_address + 0x20
#define HPET_GENERAL_COUNTER hpet_address + 0xF0

#define HPET_CAPS_64BIT_WIDE (1ULL << 13)


static uint64_t hpet_frequency = 1;
static uint64_t hpet_period    = 1;
static uintptr_t hpet_address  = 0;
static bool hpet_ready         = false;


static int hpet_init_caps(void) {

    uint64_t capabilities = mmio_r64(HPET_GENERAL_GCID);
    if (!(capabilities & HPET_CAPS_64BIT_WIDE)) {
#if DEBUG_LEVEL_WARN
        kprintf("hpet: WARN! unsupported 64bit wide capabilities, hpet can not be used as clock source\n");
#endif
        return -ENOTSUP;
    }

    hpet_period = capabilities >> 32ULL;
    if (hpet_period == 0) {
#if DEBUG_LEVEL_WARN
        kprintf("hpet: WARN! hpet main counter ticks period is 0, hpet can not be used as source clock\n");
#endif
        return -EINVAL;
    }

    hpet_frequency = HPET_TICK / (capabilities >> 32ULL);
    if (hpet_frequency == 0) {
#if DEBUG_LEVEL_WARN
        kprintf("hpet: WARN! hpet main counter ticks period is too high, hpet can not be used as source clock\n");
#endif
        return -EINVAL;
    }

    return 0;
}

int hpet_init(void) {

    acpi_sdt_t* sdt;
    if (acpi_find(&sdt, "HPET") != 0)
        return -ENOSYS;

    acpi_hpet_t* hpet;
    if (acpi_is_extended()) {
        hpet = (acpi_hpet_t*)&sdt->xtables;
    } else {
        hpet = (acpi_hpet_t*)&sdt->tables;
    }

    DEBUG_ASSERT(hpet);
    DEBUG_ASSERT(hpet->address.type == 0);
    DEBUG_ASSERT(hpet->address.address != 0);

#if DEBUG_LEVEL_TRACE
    kprintf("hpet: rev(%d) count(%d) counter(%d) nr(%d) mintick(%d) address(0x%lX)\n", hpet->hardware_rev_id, hpet->comparator_count, hpet->counter_size, hpet->hpet_number, hpet->minimum_tick, hpet->address.address);
#endif

    hpet_address = arch_vmm_map(&core->bsp.address_space, hpet->address.address + KERNEL_MMIO_AREA, hpet->address.address, PML1_PAGESIZE, ARCH_VMM_MAP_RDWR | ARCH_VMM_MAP_UNCACHED | ARCH_VMM_MAP_NOEXEC | ARCH_VMM_MAP_FIXED);

    PANIC_ASSERT(hpet_address != ARCH_VMM_MAP_FAILED);

    int e;
    if ((e = hpet_init_caps()) < 0)
        return e;

    mmio_w64(HPET_GENERAL_ISR, mmio_r64(HPET_GENERAL_ISR));
    mmio_w64(HPET_GENERAL_CR, mmio_r64(HPET_GENERAL_CR) | 1);

    hpet_ready = true;

#if DEBUG_LEVEL_TRACE
    kprintf("hpet: started! mHZ(%ld) period(%ld)\n", hpet_frequency / 1000000, hpet_period);
#endif

    return 0;
}

uint64_t hpet_getticks(void) {
    if (likely(hpet_ready)) {
        return mmio_r64(HPET_GENERAL_COUNTER);
    } else {
        return 0;
    }
}

uint64_t hpet_getres() {
    return hpet_frequency;
}