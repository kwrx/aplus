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

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/endian.h>
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/memory.h>
#include <aplus/module.h>
#include <aplus/smp.h>

#include <dev/interface.h>
#include <dev/pci.h>

#include <dev/virtio/virtio.h>


MODULE_NAME("virtio/virtio-queue");
MODULE_DEPS("dev/interface,dev/pci");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


#define virtq(d, q) (&(d)->internals.queues[(q)])

#define VIRTQ_PAGE_ALIGN(x) (((x) + (PML1_PAGESIZE - 1)) & ~((uintptr_t)PML1_PAGESIZE - 1))


/* A queue is five regions of one physical allocation, each starting on a page of its own:
 * three the device is handed the address of, and two it DMAs in and out of. Sized and
 * placed by a single routine so that the two cannot drift apart -- they were separate
 * before, and an allocation that disagrees with the offsets read out of it is the kind of
 * bug that only shows up on a queue size nobody tested. */

enum {
    VIRTQ_REGION_DESC = 0,
    VIRTQ_REGION_AVAIL,
    VIRTQ_REGION_USED,
    VIRTQ_REGION_SEND,
    VIRTQ_REGION_RECV,
    VIRTQ_REGION_MAX,
};


static size_t virtq_layout(struct virtio_driver* driver, size_t q_size, uintptr_t* offsets) {

    DEBUG_ASSERT(driver);
    DEBUG_ASSERT(offsets);

    const size_t sizes[VIRTQ_REGION_MAX] = {
        [VIRTQ_REGION_DESC]  = q_size * sizeof(struct virtq_descriptor),
        [VIRTQ_REGION_AVAIL] = sizeof(struct virtq_available) + ((q_size + 1) * sizeof(uint16_t)), //? + used_event
        [VIRTQ_REGION_USED]  = sizeof(struct virtq_used) + (q_size * 8) + sizeof(uint16_t),        //? + avail_event
        [VIRTQ_REGION_SEND]  = q_size * driver->send_window_size,
        [VIRTQ_REGION_RECV]  = q_size * driver->recv_window_size,
    };

    size_t total = 0;

    for (size_t i = 0; i < VIRTQ_REGION_MAX; i++) {

        offsets[i] = total;
        total += VIRTQ_PAGE_ALIGN(sizes[i]);
    }

    return total;
}


static uintptr_t virtq_sendbuf(struct virtio_driver* driver, uint16_t queue, uint16_t desc) {

    DEBUG_ASSERT(driver);
    DEBUG_ASSERT(desc < virtq(driver, queue)->size);

    return arch_vmm_p2v(virtq(driver, queue)->buffers.sendbuf + ((uintptr_t)desc * driver->send_window_size), ARCH_VMM_AREA_HEAP);
}


uintptr_t virtq_recvbuf(struct virtio_driver* driver, uint16_t queue, uint16_t desc) {

    DEBUG_ASSERT(driver);
    DEBUG_ASSERT(queue < driver->internals.num_queues);
    DEBUG_ASSERT(desc < virtq(driver, queue)->size);

    return arch_vmm_p2v(virtq(driver, queue)->buffers.recvbuf + ((uintptr_t)desc * driver->recv_window_size), ARCH_VMM_AREA_HEAP);
}


int virtq_init(struct virtio_driver* driver, struct virtio_pci_common_cfg volatile* cfg, uint16_t index) {

    DEBUG_ASSERT(driver);
    DEBUG_ASSERT(cfg);

    if (unlikely(index >= VIRTQ_MAX_QUEUES))
        return errno = EINVAL, -1;


    /* The address a queue is kicked through is computed from the notify capability here, so
       a queue set up before that capability was read would kick something else entirely. */

    if (unlikely(!driver->internals.notify_offset || !driver->internals.notify_off_multiplier)) {
#if DEBUG_LEVEL_FATAL
        kprintf("virtio-queue: FAIL! device %d has no notify configuration yet, cannot set up queue %d\n", driver->device, index);
#endif
        return errno = ENXIO, -1;
    }


    mmio_w16(&cfg->queue_select, cpu_to_le16(index));
    atomic_thread_fence(memory_order_seq_cst);

    size_t q_size = le16_to_cpu(mmio_r16(&cfg->queue_size));

    /* A device offers a queue it does not have as one of size zero, and the ring indices
       below are taken modulo the size, which only wraps where the device's own 16-bit
       counters do if that size is a power of two. */

    if (unlikely(!q_size || (q_size & (q_size - 1)) || q_size > VIRTQ_MAX_DESCRIPTORS)) {
#if DEBUG_LEVEL_FATAL
        kprintf("virtio-queue: FAIL! device %d offers queue %d an unusable size of %d\n", driver->device, index, q_size);
#endif
        return errno = EINVAL, -1;
    }


    uintptr_t offsets[VIRTQ_REGION_MAX];

    size_t phys_size = virtq_layout(driver, q_size, offsets);

    uintptr_t phys_buffer = pmm_alloc_blocks(phys_size / PML1_PAGESIZE);

    if (unlikely(!phys_buffer))
        return errno = ENOMEM, -1;


    struct virtq_request* requests = (struct virtq_request*)kcalloc(q_size, sizeof(struct virtq_request), GFP_KERNEL);

    if (unlikely(!requests)) {

        pmm_free_blocks(phys_buffer, phys_size / PML1_PAGESIZE);
        return errno = ENOMEM, -1;
    }


    uintptr_t virt_buffer = arch_vmm_p2v(phys_buffer, ARCH_VMM_AREA_HEAP);

    memset((void*)virt_buffer, 0, phys_size);


    virtq(driver, index)->descriptors = (struct virtq_descriptor volatile*)(virt_buffer + offsets[VIRTQ_REGION_DESC]);
    virtq(driver, index)->available   = (struct virtq_available volatile*)(virt_buffer + offsets[VIRTQ_REGION_AVAIL]);
    virtq(driver, index)->used        = (struct virtq_used volatile*)(virt_buffer + offsets[VIRTQ_REGION_USED]);
    virtq(driver, index)->notify      = (struct virtq_notify volatile*)(driver->internals.notify_offset + (le16_to_cpu(mmio_r16(&cfg->queue_notify_off)) * driver->internals.notify_off_multiplier));

    virtq(driver, index)->requests = requests;
    virtq(driver, index)->size     = q_size;

    virtq(driver, index)->buffers.base    = phys_buffer;
    virtq(driver, index)->buffers.length  = phys_size;
    virtq(driver, index)->buffers.sendbuf = phys_buffer + offsets[VIRTQ_REGION_SEND];
    virtq(driver, index)->buffers.recvbuf = phys_buffer + offsets[VIRTQ_REGION_RECV];

    virtq(driver, index)->last_used = 0;

    atomic_store(&virtq(driver, index)->completions, 0);

    spinlock_init_with_flags(&virtq(driver, index)->lock, SPINLOCK_FLAGS_CPU_OWNER);


    /* Queues share vectors when the device's table is too small to give each its own, so the
       vector a queue raises is not simply its own index. */

    uint16_t vector = VIRTIO_MSI_NO_VECTOR;

#if defined(CONFIG_HAVE_PCI_MSIX)
    if (likely(driver->internals.msix_vectors))
        vector = index % driver->internals.msix_vectors;
#endif

    mmio_w16(&cfg->queue_msix_vector, cpu_to_le16(vector));

    /* A device that could not take the vector says so by reading back NO_VECTOR: the queue
       goes uninterrupted and virtq_wait() falls back to rescanning on its own. */

#if DEBUG_LEVEL_WARN
    if (vector != VIRTIO_MSI_NO_VECTOR && le16_to_cpu(mmio_r16(&cfg->queue_msix_vector)) != vector)
        kprintf("virtio-queue: WARN! device %d refused MSI-X vector %d for queue %d\n", driver->device, vector, index);
#endif


    mmio_w64(&cfg->queue_desc, cpu_to_le64(phys_buffer + offsets[VIRTQ_REGION_DESC]));
    mmio_w64(&cfg->queue_driver, cpu_to_le64(phys_buffer + offsets[VIRTQ_REGION_AVAIL]));
    mmio_w64(&cfg->queue_device, cpu_to_le64(phys_buffer + offsets[VIRTQ_REGION_USED]));

    atomic_thread_fence(memory_order_release);

    mmio_w16(&cfg->queue_enable, cpu_to_le16(1));


#if DEBUG_LEVEL_TRACE
    kprintf("virtio-queue: device %d initialized queue %d successful [desc(%p), driver(%p), device(%p), msix(%d), notify(%p), size(%d)]\n", driver->device, index, phys_buffer + offsets[VIRTQ_REGION_DESC],
            phys_buffer + offsets[VIRTQ_REGION_AVAIL], phys_buffer + offsets[VIRTQ_REGION_USED], vector, virtq(driver, index)->notify, q_size);
#endif

    return 0;
}


void virtq_dnit(struct virtio_driver* driver, uint16_t queue) {

    DEBUG_ASSERT(driver);

    if (unlikely(queue >= VIRTQ_MAX_QUEUES))
        return;

    if (virtq(driver, queue)->requests)
        kfree(virtq(driver, queue)->requests);

    if (virtq(driver, queue)->buffers.base)
        pmm_free_blocks(virtq(driver, queue)->buffers.base, virtq(driver, queue)->buffers.length / PML1_PAGESIZE);

    memset(virtq(driver, queue), 0, sizeof(*virtq(driver, queue)));
}


/* Put a descriptor and everything chained behind it back in the pool. Bounded by the queue
   size so that a device that corrupts q_next into a cycle costs a walk, not a hang. */

static void virtq_release_locked(struct virtio_driver* driver, uint16_t queue, uint16_t head) {

    uint16_t d = head;

    for (size_t n = 0; n < virtq(driver, queue)->size; n++) {

        uint16_t flags = le16_to_cpu(virtq(driver, queue)->descriptors[d].q_flags);
        uint16_t next  = le16_to_cpu(virtq(driver, queue)->descriptors[d].q_next);

        virtq(driver, queue)->requests[d].state  = VIRTQ_REQUEST_FREE;
        virtq(driver, queue)->requests[d].length = 0;

        if (!(flags & VIRTQ_DESC_F_NEXT))
            break;

        if (unlikely(next >= virtq(driver, queue)->size))
            break;

        d = next;
    }
}


static uint16_t virtq_alloc_locked(struct virtio_driver* driver, uint16_t queue, uint8_t state) {

    for (uint16_t d = 0; d < virtq(driver, queue)->size; d++) {

        if (virtq(driver, queue)->requests[d].state != VIRTQ_REQUEST_FREE)
            continue;

        virtq(driver, queue)->requests[d].state  = state;
        virtq(driver, queue)->requests[d].length = 0;

        return d;
    }

    return VIRTQ_DESC_NONE;
}


/* Publish one descriptor chain on the available ring. The caller holds the lock: this reads
   and writes the ring index, and two callers doing that at once lose a buffer. */

static void virtq_publish_locked(struct virtio_driver* driver, uint16_t queue, uint16_t head) {

    uint16_t idx = le16_to_cpu(virtq(driver, queue)->available->q_idx);

    virtq(driver, queue)->available->q_ring[idx % virtq(driver, queue)->size] = cpu_to_le16(head);

    /* The descriptors and the ring entry have to be in place before the index that makes
       them visible to the device is. */
    atomic_thread_fence(memory_order_release);

    virtq(driver, queue)->available->q_idx = cpu_to_le16(idx + 1);
}


/* Take a descriptor out of the pool.
 *
 * VIRTQ_DESC_NONE means the queue has none left. Descriptor 0 is a descriptor like any
 * other: the pool is tracked in requests[], beside the ring rather than in it, so there is
 * no need to reserve an index to mean failure -- which is what the old sentinel of 0 cost,
 * and it could not be checked for anyway since the return type was unsigned. */

uint16_t virtq_alloc_descriptor(struct virtio_driver* driver, uint16_t queue, uint8_t state) {

    DEBUG_ASSERT(driver);
    DEBUG_ASSERT(driver->device);
    DEBUG_ASSERT(queue < driver->internals.num_queues);
    DEBUG_ASSERT(state != VIRTQ_REQUEST_FREE);

    uint16_t desc = VIRTQ_DESC_NONE;

    scoped_lock(&virtq(driver, queue)->lock) {
        desc = virtq_alloc_locked(driver, queue, state);
    }

    if (likely(desc != VIRTQ_DESC_NONE))
        return desc;


    /* An empty pool is most often one that has not been collected from: every fire and
       forget send leaves its descriptor for whoever next needs one. */

    while (virtq_reap(driver, queue, NULL, NULL))
        ;

    scoped_lock(&virtq(driver, queue)->lock) {
        desc = virtq_alloc_locked(driver, queue, state);
    }

    return desc;
}


void virtq_free_descriptor(struct virtio_driver* driver, uint16_t queue, uint16_t desc) {

    DEBUG_ASSERT(driver);
    DEBUG_ASSERT(driver->device);
    DEBUG_ASSERT(queue < driver->internals.num_queues);
    DEBUG_ASSERT(desc < virtq(driver, queue)->size);

    scoped_lock(&virtq(driver, queue)->lock) {
        virtq_release_locked(driver, queue, desc);
    }
}


/* Hand a receive buffer to the device. The descriptor keeps its window for as long as it is
   posted, and the owning driver gets it back from virtq_reap() to read and re-post.

   Notifying is left to the caller so that stocking a whole queue costs one notification
   rather than one per buffer. */

void virtq_provide(struct virtio_driver* driver, uint16_t queue, uint16_t desc, size_t length) {

    DEBUG_ASSERT(driver);
    DEBUG_ASSERT(queue < driver->internals.num_queues);
    DEBUG_ASSERT(desc < virtq(driver, queue)->size);
    DEBUG_ASSERT(length && length <= driver->recv_window_size);

    scoped_lock(&virtq(driver, queue)->lock) {

        virtq(driver, queue)->descriptors[desc].q_address = cpu_to_le64(virtq(driver, queue)->buffers.recvbuf + ((uintptr_t)desc * driver->recv_window_size));
        virtq(driver, queue)->descriptors[desc].q_length  = cpu_to_le32(length);
        virtq(driver, queue)->descriptors[desc].q_flags   = cpu_to_le16(VIRTQ_DESC_F_WRITE);
        virtq(driver, queue)->descriptors[desc].q_next    = cpu_to_le16(0);

        virtq(driver, queue)->requests[desc].state  = VIRTQ_REQUEST_POSTED;
        virtq(driver, queue)->requests[desc].length = 0;

        virtq_publish_locked(driver, queue, desc);
    }
}


void virtq_notify(struct virtio_driver* driver, uint16_t queue) {

    DEBUG_ASSERT(driver);
    DEBUG_ASSERT(queue < driver->internals.num_queues);

    /* Everything published has to be visible to the device before it is told to look. */
    atomic_thread_fence(memory_order_seq_cst);

    mmio_w16(&virtq(driver, queue)->notify->n_idx, cpu_to_le16(queue));
}


/* Consume the next entry of the used ring.
 *
 * Returns 1 when one was consumed and 0 when the ring is drained. An entry belonging to a
 * buffer the caller posted is reported through desc and length; one belonging to a fire and
 * forget send is freed and one a caller is waiting on is marked done for it, both of which
 * report desc as VIRTQ_DESC_NONE.
 *
 * Each entry is consumed exactly once, which is the point: the old code had every waiter
 * rescan the whole ring from its own snapshot, so an entry could be matched by more than
 * one of them and a descriptor recycled underneath a caller still reading its window. */

int virtq_reap(struct virtio_driver* driver, uint16_t queue, uint16_t* desc, uint32_t* length) {

    DEBUG_ASSERT(driver);
    DEBUG_ASSERT(queue < driver->internals.num_queues);

    if (desc)
        *desc = VIRTQ_DESC_NONE;

    if (length)
        *length = 0;


    scoped_lock(&virtq(driver, queue)->lock) {

        /* An unsigned difference, never a comparison: both of these are 16-bit counters
           that wrap, and "has anything arrived" is a distance rather than an ordering. A
           plain < stops finding anything the moment the device's index wraps past ours. */

        if ((uint16_t)(le16_to_cpu(virtq(driver, queue)->used->q_idx) - virtq(driver, queue)->last_used) == 0)
            return 0;

        atomic_thread_fence(memory_order_acquire);


        uint16_t i = virtq(driver, queue)->last_used % virtq(driver, queue)->size;

        uint16_t head = (uint16_t)le32_to_cpu(virtq(driver, queue)->used->q_elements[i].e_id);
        uint32_t size = le32_to_cpu(virtq(driver, queue)->used->q_elements[i].e_length);


        /* A posted buffer belongs to whoever posted it, and a caller that asked for no
           descriptor back has nowhere to put it. Leave it where it is rather than consume
           it into nothing: this is the path virtq_alloc_descriptor() takes when it is
           looking for space, and swallowing an event there would lose it outright. */

        if (head < virtq(driver, queue)->size && virtq(driver, queue)->requests[head].state == VIRTQ_REQUEST_POSTED && !desc)
            return 0;


        virtq(driver, queue)->last_used++;


        if (unlikely(head >= virtq(driver, queue)->size)) {
#if DEBUG_LEVEL_ERROR
            kprintf("virtio-queue: ERROR! device %d completed descriptor %d which queue %d does not have\n", driver->device, head, queue);
#endif
            return 1;
        }


        switch (virtq(driver, queue)->requests[head].state) {

            case VIRTQ_REQUEST_ASYNC:
                virtq_release_locked(driver, queue, head);
                break;

            case VIRTQ_REQUEST_INFLIGHT:
                virtq(driver, queue)->requests[head].length = size;
                virtq(driver, queue)->requests[head].state  = VIRTQ_REQUEST_DONE;
                break;

            case VIRTQ_REQUEST_POSTED:

                virtq(driver, queue)->requests[head].length = size;

                if (desc)
                    *desc = head;

                if (length)
                    *length = size;

                break;

            default:
#if DEBUG_LEVEL_WARN
                kprintf("virtio-queue: WARN! device %d completed descriptor %d of queue %d which was not submitted\n", driver->device, head, queue);
#endif
                break;
        }

        return 1;
    }

    return 0;
}


/* Wait for the device to finish with a descriptor chain.
 *
 * There is no sleeping wait to use here. sem_wait() in this kernel spins on a counter with
 * __cpu_pause() rather than descheduling, so dressing this up as a blocking call would buy
 * nothing and cost the semaphore's counter drift -- one poster in the interrupt, any number
 * of waiters, and a post for every interrupt whether anyone is waiting or not.
 *
 * The interrupt is a hint that shortens the wait, never the thing being waited for: every
 * spinlock in this kernel disables interrupts, so a device driver that holds one across a
 * command -- virtgpu_flush() pairs a transfer and a flush under one -- runs this loop on a
 * CPU that cannot take the completion. The used ring is therefore polled regardless, and
 * the deadline is what stops a wedged device from owning a CPU forever. */

static int virtq_wait(struct virtio_driver* driver, uint16_t queue, uint16_t head, size_t* length) {

    DEBUG_ASSERT(driver);
    DEBUG_ASSERT(queue < driver->internals.num_queues);

    uint64_t deadline = arch_timer_generic_getms() + VIRTQ_TIMEOUT_MS;

    unsigned seen = atomic_load(&virtq(driver, queue)->completions);

    for (;;) {

        while (virtq_reap(driver, queue, NULL, NULL))
            ;


        bool done      = false;
        size_t written = 0;

        scoped_lock(&virtq(driver, queue)->lock) {

            if (virtq(driver, queue)->requests[head].state == VIRTQ_REQUEST_DONE) {

                done    = true;
                written = virtq(driver, queue)->requests[head].length;
            }
        }

        if (done) {

            if (length && *length > written)
                *length = written;

            return 0;
        }


        if (unlikely(arch_timer_generic_getms() >= deadline)) {
#if DEBUG_LEVEL_ERROR
            kprintf("virtio-queue: ERROR! device %d did not answer descriptor %d of queue %d within %dms\n", driver->device, head, queue, VIRTQ_TIMEOUT_MS);
#endif
            return errno = ETIMEDOUT, -1;
        }


        /* Idle before looking again, cut short by the interrupt when one can be taken. */

        for (size_t spin = 0; spin < VIRTQ_POLL_SPINS; spin++) {

            if (atomic_load(&virtq(driver, queue)->completions) != seen)
                break;

            __cpu_pause();
        }

        seen = atomic_load(&virtq(driver, queue)->completions);
    }
}


/* Take a descriptor from the pool, waiting for the device when there are none left.
 *
 * A fire and forget request has no completion anybody is sitting on, so the only thing that
 * ever puts its descriptor back is the next caller reaping the used ring on the way in. That
 * keeps up with a device that keeps up, and nothing else: a writer faster than the device --
 * a process in a write() loop on /dev/hvc0 is faster than the host's main loop after a few
 * hundred sends -- empties the pool with every descriptor still legitimately in flight.
 *
 * Failing there reports a queue that is merely full as an I/O error, and because a descriptor
 * only ever comes back on the way in, giving up is also what makes it permanent: the caller
 * that would have collected them is the one being turned away. That is how a port that had
 * been writing happily stopped for good partway through a large transfer.
 *
 * So wait instead, on the deadline virtq_wait() gives a device to answer a request: room in a
 * queue is something the device produces, and one that produces none in five seconds has
 * stopped rather than fallen behind. The pool is re-examined -- which re-reads the used ring
 * -- on every pass rather than only when the completion counter moves, because that counter
 * only says an interrupt got through, and a send from a syscall runs with interrupts disabled
 * often enough that usually none has. */

static uint16_t virtq_alloc_descriptor_wait(struct virtio_driver* driver, uint16_t queue, uint8_t state) {

    uint16_t desc = virtq_alloc_descriptor(driver, queue, state);

    if (likely(desc != VIRTQ_DESC_NONE))
        return desc;


    uint64_t deadline = arch_timer_generic_getms() + VIRTQ_TIMEOUT_MS;

    unsigned seen = atomic_load(&virtq(driver, queue)->completions);

    do {

        for (size_t spin = 0; spin < VIRTQ_POLL_SPINS; spin++) {

            if (atomic_load(&virtq(driver, queue)->completions) != seen)
                break;

            __cpu_pause();
        }

        seen = atomic_load(&virtq(driver, queue)->completions);

        if ((desc = virtq_alloc_descriptor(driver, queue, state)) != VIRTQ_DESC_NONE)
            return desc;

    } while (arch_timer_generic_getms() < deadline);


    return VIRTQ_DESC_NONE;
}


ssize_t virtq_sendrecv(struct virtio_driver* driver, uint16_t queue, const void* message, size_t size, void* output, size_t outsize) {

    DEBUG_ASSERT(driver);
    DEBUG_ASSERT(driver->device);
    DEBUG_ASSERT(message);
    DEBUG_ASSERT(output);
    DEBUG_ASSERT(queue < driver->internals.num_queues);

    if (unlikely(!size || size > driver->send_window_size))
        return errno = EINVAL, -1;

    if (unlikely(!outsize || outsize > driver->recv_window_size))
        return errno = EINVAL, -1;


    uint16_t inp = virtq_alloc_descriptor(driver, queue, VIRTQ_REQUEST_INFLIGHT);

    if (unlikely(inp == VIRTQ_DESC_NONE)) {
#if DEBUG_LEVEL_ERROR
        kprintf("virtio-queue: ERROR! device %d has no free descriptor in queue %d\n", driver->device, queue);
#endif
        return errno = ENOSPC, -1;
    }

    uint16_t out = virtq_alloc_descriptor(driver, queue, VIRTQ_REQUEST_INFLIGHT);

    if (unlikely(out == VIRTQ_DESC_NONE)) {

        virtq_free_descriptor(driver, queue, inp);

#if DEBUG_LEVEL_ERROR
        kprintf("virtio-queue: ERROR! device %d has no free descriptor in queue %d\n", driver->device, queue);
#endif
        return errno = ENOSPC, -1;
    }


    memcpy((void*)virtq_sendbuf(driver, queue, inp), message, size);

    scoped_lock(&virtq(driver, queue)->lock) {

        virtq(driver, queue)->descriptors[inp].q_address = cpu_to_le64(virtq(driver, queue)->buffers.sendbuf + ((uintptr_t)inp * driver->send_window_size));
        virtq(driver, queue)->descriptors[inp].q_length  = cpu_to_le32(size);
        virtq(driver, queue)->descriptors[inp].q_flags   = cpu_to_le16(VIRTQ_DESC_F_NEXT);
        virtq(driver, queue)->descriptors[inp].q_next    = cpu_to_le16(out);

        virtq(driver, queue)->descriptors[out].q_address = cpu_to_le64(virtq(driver, queue)->buffers.recvbuf + ((uintptr_t)out * driver->recv_window_size));
        virtq(driver, queue)->descriptors[out].q_length  = cpu_to_le32(outsize);
        virtq(driver, queue)->descriptors[out].q_flags   = cpu_to_le16(VIRTQ_DESC_F_WRITE);
        virtq(driver, queue)->descriptors[out].q_next    = cpu_to_le16(0);

        virtq_publish_locked(driver, queue, inp);
    }

    virtq_notify(driver, queue);


    size_t received = outsize;

    int e = virtq_wait(driver, queue, inp, &received);

    /* The copy comes out of the window before the descriptor goes back in the pool, not
       after: a descriptor in the pool is one somebody else may already be filling. */

    if (likely(e == 0))
        memcpy(output, (void*)virtq_recvbuf(driver, queue, out), received);

    virtq_free_descriptor(driver, queue, inp);

    if (unlikely(e < 0))
        return errno = EIO, -1;

#if DEBUG_LEVEL_TRACE
    kprintf("virtio-queue: device %d has sent %ld and received %ld bytes on queue %d\n", driver->device, size, received, queue);
#endif

    return received;
}


ssize_t virtq_recv(struct virtio_driver* driver, uint16_t queue, void* output, size_t outsize) {

    DEBUG_ASSERT(driver);
    DEBUG_ASSERT(driver->device);
    DEBUG_ASSERT(output);
    DEBUG_ASSERT(queue < driver->internals.num_queues);

    if (unlikely(!outsize || outsize > driver->recv_window_size))
        return errno = EINVAL, -1;


    uint16_t out = virtq_alloc_descriptor(driver, queue, VIRTQ_REQUEST_INFLIGHT);

    if (unlikely(out == VIRTQ_DESC_NONE)) {
#if DEBUG_LEVEL_ERROR
        kprintf("virtio-queue: ERROR! device %d has no free descriptor in queue %d\n", driver->device, queue);
#endif
        return errno = ENOSPC, -1;
    }


    scoped_lock(&virtq(driver, queue)->lock) {

        virtq(driver, queue)->descriptors[out].q_address = cpu_to_le64(virtq(driver, queue)->buffers.recvbuf + ((uintptr_t)out * driver->recv_window_size));
        virtq(driver, queue)->descriptors[out].q_length  = cpu_to_le32(outsize);
        virtq(driver, queue)->descriptors[out].q_flags   = cpu_to_le16(VIRTQ_DESC_F_WRITE);
        virtq(driver, queue)->descriptors[out].q_next    = cpu_to_le16(0);

        virtq_publish_locked(driver, queue, out);
    }

    virtq_notify(driver, queue);


    size_t received = outsize;

    int e = virtq_wait(driver, queue, out, &received);

    if (likely(e == 0))
        memcpy(output, (void*)virtq_recvbuf(driver, queue, out), received);

    virtq_free_descriptor(driver, queue, out);

    if (unlikely(e < 0))
        return errno = EIO, -1;

#if DEBUG_LEVEL_TRACE
    kprintf("virtio-queue: device %d has received %ld bytes of data on queue %d\n", driver->device, received, queue);
#endif

    return received;
}


/* Send without waiting. The descriptor is marked fire and forget, which is what makes it
   the reaper's to reclaim -- before, nothing reclaimed it at all and a queue ran itself out
   of descriptors one send at a time. */

ssize_t virtq_send(struct virtio_driver* driver, uint16_t queue, const void* message, size_t size) {

    DEBUG_ASSERT(driver);
    DEBUG_ASSERT(driver->device);
    DEBUG_ASSERT(message);
    DEBUG_ASSERT(queue < driver->internals.num_queues);

    if (unlikely(!size || size > driver->send_window_size))
        return errno = EINVAL, -1;


    uint16_t inp = virtq_alloc_descriptor_wait(driver, queue, VIRTQ_REQUEST_ASYNC);

    if (unlikely(inp == VIRTQ_DESC_NONE)) {
#if DEBUG_LEVEL_ERROR
        kprintf("virtio-queue: ERROR! device %d did not free a descriptor in queue %d within %dms\n", driver->device, queue, VIRTQ_TIMEOUT_MS);
#endif
        return errno = ETIMEDOUT, -1;
    }


    memcpy((void*)virtq_sendbuf(driver, queue, inp), message, size);

    scoped_lock(&virtq(driver, queue)->lock) {

        virtq(driver, queue)->descriptors[inp].q_address = cpu_to_le64(virtq(driver, queue)->buffers.sendbuf + ((uintptr_t)inp * driver->send_window_size));
        virtq(driver, queue)->descriptors[inp].q_length  = cpu_to_le32(size);
        virtq(driver, queue)->descriptors[inp].q_flags   = cpu_to_le16(0);
        virtq(driver, queue)->descriptors[inp].q_next    = cpu_to_le16(0);

        virtq_publish_locked(driver, queue, inp);
    }

    virtq_notify(driver, queue);


#if DEBUG_LEVEL_TRACE
    kprintf("virtio-queue: device %d has sent %ld bytes of data on queue %d\n", driver->device, size, queue);
#endif

    return size;
}


/* Called from the interrupt for every queue the raised vector serves.
 *
 * Nothing is drained here on purpose. A queue stocked with buffers by a device driver is
 * drained by that driver, in order, out of its own handler; draining it here would consume
 * its events out from under it. All this does is tell a waiter to look again. */

void virtq_flush(struct virtio_driver* driver, uint16_t queue) {

    DEBUG_ASSERT(driver);

    if (unlikely(queue >= driver->internals.num_queues))
        return;

    atomic_fetch_add(&virtq(driver, queue)->completions, 1);
}


void init(const char* args) {
}

void dnit(void) {
}
