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

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/hal.h>
#include <aplus/memory.h>
#include <aplus/module.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <dev/usb/usb.h>


MODULE_NAME("usb/core");
MODULE_DEPS("dev/pci");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


#define USB_CORE_MAX_CONTROLLERS 8
#define USB_CORE_MAX_CLASSES     16
#define USB_CORE_DMA_BLOCKS      4096
#define USB_CORE_POLL_MS         8
#define USB_CORE_STACK_SIZE      0x10000


/** @brief A controller waiting to be, or already, brought up on the USB thread. */
struct usb_core_controller {
    usb_controller_start_t start;
    void* arg;
};

/** @brief A class driver: who claims the devices of a class. */
struct usb_core_class {
    int class;
    void (*_Atomic init)(usbdev_t* dev);
};

/** @brief A page run handed out by usb_dma_memalign(), remembered so usb_dma_free() knows its size. */
struct usb_core_dma_block {
    uintptr_t virt;
    size_t pages;
};


extern hci_t* usb_hcs;

static struct usb_core_controller controllers[USB_CORE_MAX_CONTROLLERS];
static atomic_size_t num_controllers = 0;

static struct usb_core_class classes[USB_CORE_MAX_CLASSES];
static atomic_uint classes_generation = 0;

static struct usb_core_dma_block dma_blocks[USB_CORE_DMA_BLOCKS];
static spinlock_t dma_lock;

static atomic_bool usb_thread_running = false;
static atomic_bool usb_thread_stop    = false;
static atomic_bool usb_thread_stopped = false;
static uint32_t usb_thread_idle       = 0;



void* usb_dma_memalign(size_t align, size_t size) {

    if (!size)
        size = 1;

    if (align <= 16 && size <= PML1_PAGESIZE - 16)
        return kcalloc(1, size, GFP_KERNEL);


    size_t pages     = (size + PML1_PAGESIZE - 1) / PML1_PAGESIZE;
    uintptr_t palign = PML1_PAGESIZE;

    while (palign < align)
        palign <<= 1;

    uintptr_t phys = pmm_alloc_blocks_aligned(pages, palign);

    if (unlikely(phys == PMM_INVALID_ADDRESS))
        return NULL;

    uintptr_t virt = arch_vmm_p2v(phys, ARCH_VMM_AREA_HEAP);

    scoped_lock(&dma_lock) {

        for (size_t i = 0; i < USB_CORE_DMA_BLOCKS; i++) {

            if (dma_blocks[i].virt)
                continue;

            dma_blocks[i].virt  = virt;
            dma_blocks[i].pages = pages;

            memset((void*)virt, 0, pages * PML1_PAGESIZE);
            return (void*)virt;
        }
    }

    pmm_free_blocks(phys, pages);

    kprintf("usb-core: ERROR! out of DMA block slots\n");
    return NULL;
}


void usb_dma_free(const volatile void* ptr) {

    if (!ptr)
        return;

    uintptr_t virt = (uintptr_t)ptr;

    if (virt & (PML1_PAGESIZE - 1)) {
        kfree((void*)ptr);
        return;
    }

    size_t pages = 0;

    scoped_lock(&dma_lock) {

        for (size_t i = 0; i < USB_CORE_DMA_BLOCKS; i++) {

            if (dma_blocks[i].virt != virt)
                continue;

            pages = dma_blocks[i].pages;

            dma_blocks[i].virt  = 0;
            dma_blocks[i].pages = 0;
            break;
        }
    }

    if (likely(pages)) {
        pmm_free_blocks(arch_vmm_v2p(virt, ARCH_VMM_AREA_HEAP), pages);
    } else {
        kprintf("usb-core: WARN! freeing unknown DMA block %p\n", ptr);
    }
}



void (*usb_class_driver(int class))(usbdev_t* dev) {

    for (size_t i = 0; i < USB_CORE_MAX_CLASSES; i++) {

        void (*init)(usbdev_t*) = atomic_load(&classes[i].init);

        if (init && classes[i].class == class)
            return init;
    }

    return usb_generic_init;
}


int usb_register_class(int class, void (*init)(usbdev_t* dev)) {

    DEBUG_ASSERT(init);

    if (usb_class_driver(class) != usb_generic_init)
        return -1;

    for (size_t i = 0; i < USB_CORE_MAX_CLASSES; i++) {

        if (atomic_load(&classes[i].init))
            continue;

        classes[i].class = class;
        atomic_store(&classes[i].init, init);
        atomic_fetch_add(&classes_generation, 1);

        return 0;
    }

    return -1;
}


void usb_unregister_class(int class) {

    for (size_t i = 0; i < USB_CORE_MAX_CLASSES; i++) {
        if (classes[i].class == class) {
            atomic_store(&classes[i].init, NULL);
        }
    }
}


/**
 * @brief Hands devices nobody claimed to a class driver registered after they were enumerated.
 */
static void usb_core_rebind(void) {

    for (hci_t* hc = usb_hcs; hc; hc = hc->next) {

        for (int i = 1; i < 128; i++) {

            usbdev_t* dev = hc->devices[i];

            if (!dev || dev->init != usb_generic_init)
                continue;

            void (*init)(usbdev_t*) = usb_class_driver(dev->class);

            if (init == usb_generic_init)
                continue;

            dev->destroy(dev);
            dev->init = init;
            dev->init(dev);
        }
    }
}


/**
 * @brief Sleeps the calling kernel thread.
 * @param ms Milliseconds to sleep.
 */
static void usb_core_sleep(uint32_t ms) {
    arch_syscall3(SYSCALL_NR_KTHREAD_WAIT, &usb_thread_idle, 0, ms);
}


/**
 * @brief Owns the whole USB stack: brings controllers up, binds late drivers and polls every device.
 * @param arg Unused.
 */
static void usb_core_thread(void* arg) {

    size_t started      = 0;
    unsigned generation = 0;

    while (!atomic_load(&usb_thread_stop)) {

        for (; started < atomic_load(&num_controllers); started++)
            controllers[started].start(controllers[started].arg);

        if (generation != atomic_load(&classes_generation)) {
            generation = atomic_load(&classes_generation);
            usb_core_rebind();
        }

        usb_poll();
        usb_core_sleep(USB_CORE_POLL_MS);
    }

    usb_exit();

    atomic_store(&usb_thread_stopped, true);

    for (;;)
        usb_core_sleep(1000);
}


int usb_add_controller(usb_controller_start_t start, void* arg) {

    DEBUG_ASSERT(start);

    size_t n = atomic_load(&num_controllers);

    if (n >= USB_CORE_MAX_CONTROLLERS)
        return -1;

    controllers[n].start = start;
    controllers[n].arg   = arg;

    atomic_store(&num_controllers, n + 1);

    if (!atomic_exchange(&usb_thread_running, true))
        arch_task_spawn_kthread("usb", usb_core_thread, USB_CORE_STACK_SIZE, NULL);

    return 0;
}



void init(const char* args) {
    spinlock_init(&dma_lock);
}


void dnit(void) {

    if (!atomic_load(&usb_thread_running))
        return;

    atomic_store(&usb_thread_stop, true);

    for (size_t i = 0; i < 2000 && !atomic_load(&usb_thread_stopped); i++)
        arch_timer_delay(1000);
}
