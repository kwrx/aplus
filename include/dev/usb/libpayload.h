/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2026 Antonino Natale
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

#ifndef _DEV_USB_LIBPAYLOAD_H
#define _DEV_USB_LIBPAYLOAD_H

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/hal.h>
#include <aplus/memory.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <dev/pci.h>
#include <dev/usb/usb.h>


#ifndef ARRAY_SIZE
    #define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

#define div_round_up(n, d) (((n) + (d) - 1) / (d))

#ifndef __printf
    #define __printf(a, b) __attribute__((format(printf, a, b)))
#endif

#define fatal(...) kpanicf(__VA_ARGS__)

#define read32(p) (*(volatile const u32*)(p))
#define wmb()     __atomic_thread_fence(__ATOMIC_SEQ_CST)


/**
 * @brief Allocates zeroed kernel memory, accepting the size of zero libpayload sometimes asks for.
 * @param size Bytes to allocate.
 * @return The memory, or NULL when the heap is exhausted.
 */
static inline void* usbh_zalloc(size_t size) {
    return kcalloc(1, size ? size : 1, GFP_KERNEL);
}

/**
 * @brief Frees kernel memory, ignoring NULL as free(3) does.
 * @param ptr The memory to free, or NULL.
 */
static inline void usbh_free(const volatile void* ptr) {
    if (ptr)
        kfree((void*)ptr);
}

#define malloc(size)    usbh_zalloc(size)
#define calloc(n, size) usbh_zalloc((n) * (size))
#define xzalloc(size)   usbh_zalloc(size)
#define free(ptr)       usbh_free(ptr)


#define dma_memalign(align, size) usb_dma_memalign((align), (size))
#define dma_free(ptr)             usb_dma_free(ptr)

#define dma_initialized() 1
#define dma_coherent(ptr) 0

#define virt_to_phys(ptr)  ((u64)arch_vmm_v2p((uintptr_t)(ptr), ARCH_VMM_AREA_HEAP))
#define phys_to_virt(addr) ((void*)arch_vmm_p2v((uintptr_t)(addr), ARCH_VMM_AREA_HEAP))


/**
 * @brief Busy-waits for a number of microseconds.
 * @param us Microseconds to wait.
 */
static inline void udelay(unsigned int us) {
    if (us)
        arch_timer_delay(us);
}

/**
 * @brief Busy-waits for a number of milliseconds.
 * @param ms Milliseconds to wait.
 */
static inline void mdelay(unsigned int ms) {
    if (ms)
        arch_timer_delay((uint64_t)ms * 1000ULL);
}


#define pci_read_config8(dev, off)       ((u8)pci_read((dev), (off), 1))
#define pci_read_config16(dev, off)      ((u16)pci_read((dev), (off), 2))
#define pci_read_config32(dev, off)      ((u32)pci_read((dev), (off), 4))
#define pci_write_config16(dev, off, v)  pci_write((dev), (off), 2, (v))
#define pci_write_config32(dev, off, v)  pci_write((dev), (off), 4, (v))

#endif
