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


#define PMTMR_FREQUENCY      3579545
#define PMTMR_LENGTH         4

#define FADT_TMR_VAL_EXT     (1 << 8)
#define FADT_HW_REDUCED_ACPI (1 << 20)

#define ACPI_GAS_SYSTEM_IO   1


static uint16_t pmtmr_port = 0;
static uint32_t pmtmr_mask = 0;

static _Atomic uint64_t pmtmr_ticks = 0;


int pmtmr_init(void) {

    acpi_sdt_t* facp;
    if (acpi_find(&facp, "FACP") != 0)
        return -ENOSYS;

    acpi_fadt_t* fadt;
    if (acpi_is_extended()) {
        fadt = (acpi_fadt_t*)&facp->xtables;
    } else {
        fadt = (acpi_fadt_t*)&facp->tables;
    }

    if (fadt->flags & FADT_HW_REDUCED_ACPI) {
#if DEBUG_LEVEL_WARN
        kprintf("acpi-pmtmr: WARN! acpi has FADT_HW_REDUCED_ACPI flags, pmtmr can't be used as clock source\n");
#endif
        return -ENOTSUP;
    }

    uint64_t port = 0;
    if (acpi_is_extended()) {
        if (facp->length >= sizeof(acpi_sdt_t) + offsetof(acpi_fadt_t, x_pm_timer_block) + sizeof(fadt->x_pm_timer_block)) {
            if (fadt->x_pm_timer_block.type == ACPI_GAS_SYSTEM_IO) {
                port = fadt->x_pm_timer_block.address;
            }
        }
    }

    if (port == 0 && fadt->pm_timer_length == PMTMR_LENGTH) {
        port = fadt->pm_timer_block;
    }

    if (port == 0 || port > UINT16_MAX) {
#if DEBUG_LEVEL_WARN
        kprintf("acpi-pmtmr: WARN! acpi has invalid port 0x%lX, pmtmr can't be used as clock source\n", port);
#endif
        return -EINVAL;
    }

    pmtmr_port = (uint16_t)port;
    pmtmr_mask = (fadt->flags & FADT_TMR_VAL_EXT) ? 0xFFFFFFFF : 0x00FFFFFF;

    atomic_store(&pmtmr_ticks, inl(pmtmr_port) & pmtmr_mask);

#if DEBUG_LEVEL_TRACE
    kprintf("acpi-pmtmr: started! port(0x%X) bits(%d)\n", pmtmr_port, pmtmr_mask == 0xFFFFFFFF ? 32 : 24);
#endif

    return 0;
}


uint64_t pmtmr_getticks(void) {

    uint64_t last = atomic_load(&pmtmr_ticks);
    uint64_t next;

    do {
        uint32_t now = inl(pmtmr_port) & pmtmr_mask;
        next = last + ((now - (uint32_t)last) & pmtmr_mask);
    } while (!atomic_compare_exchange_weak(&pmtmr_ticks, &last, next));

    return next;
}

uint64_t pmtmr_getres(void) {
    return PMTMR_FREQUENCY;
}
