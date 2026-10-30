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

#define PIT_FREQUENCY 1193182
#define PIT_CHANNEL2  0x42
#define PIT_COMMAND   0x43
#define PIT_GATE      0x61

#define PIT_SELECT_CHANNEL2 (2 << 6)
#define PIT_ACCESS_LATCH    (0 << 4)
#define PIT_ACCESS_LOHI     (3 << 4)
#define PIT_MODE_RATE       (2 << 1)
#define PIT_BINARY          (0 << 0)

#define PIT_GATE_ENABLE  (1 << 0)
#define PIT_GATE_SPEAKER (1 << 1)


#define PIT_PROBE_READS   100000
#define PIT_PROBE_CHANGES 16


static _Atomic uint64_t pit_ticks = 0;


static uint16_t pit_read(void) {

    outb(PIT_COMMAND, PIT_SELECT_CHANNEL2 | PIT_ACCESS_LATCH);

    uint16_t lo = inb(PIT_CHANNEL2);
    uint16_t hi = inb(PIT_CHANNEL2);

    return (hi << 8) | lo;
}

int pit_init(void) {

    outb(PIT_GATE, (inb(PIT_GATE) & ~PIT_GATE_SPEAKER) | PIT_GATE_ENABLE);

    outb(PIT_COMMAND, PIT_SELECT_CHANNEL2 | PIT_ACCESS_LOHI | PIT_MODE_RATE | PIT_BINARY);
    outb(PIT_CHANNEL2, 0x00);
    outb(PIT_CHANNEL2, 0x00);

    atomic_store(&pit_ticks, (uint16_t)-pit_read());

#if DEBUG_LEVEL_TRACE
    kprintf("pit: started! HZ(%d)\n", PIT_FREQUENCY);
#endif

    return 0;
}

uint64_t pit_getticks(void) {

    uint64_t last = atomic_load(&pit_ticks);
    uint64_t next;

    do {
        uint16_t now = (uint16_t)-pit_read();
        next = last + (uint16_t)(now - (uint16_t)last);
    } while (!atomic_compare_exchange_weak(&pit_ticks, &last, next));

    return next;
}

uint64_t pit_getres(void) {
    return PIT_FREQUENCY;
}
