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
#include <stdint.h>
#include <string.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/endian.h>
#include <aplus/errno.h>
#include <aplus/fb.h>
#include <aplus/hal.h>
#include <aplus/memory.h>
#include <aplus/module.h>
#include <aplus/smp.h>

#include <dev/interface.h>
#include <dev/pci.h>
#include <dev/video.h>

#include <dev/virtio/virtio.h>


MODULE_NAME("virtio/virtio-queue");
MODULE_DEPS("dev/interface,dev/pci");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");



int virtq_init(struct virtio_driver* driver, struct virtio_pci_common_cfg volatile* cfg, uint16_t index) {

    DEBUG_ASSERT(driver);
    DEBUG_ASSERT(cfg);
    DEBUG_ASSERT(cfg->queue_size);



    mmio_w16(&cfg->queue_select, cpu_to_le16(index));
    atomic_thread_fence(memory_order_seq_cst);


    size_t qsize = cfg->queue_size;


    uintptr_t pbuf = pmm_alloc_blocks(((qsize * 16) + (qsize * 2 + 6) + (qsize * 8 + 6) + (qsize * driver->send_window_size) + (qsize * driver->recv_window_size)) / PML1_PAGESIZE + 1);

    if (unlikely(!pbuf))
        return errno = ENOMEM, -1;


    void* qdesc = (void*)arch_vmm_p2v(pbuf + (0), ARCH_VMM_AREA_HEAP);
    void* qfree = (void*)arch_vmm_p2v(pbuf + (qsize * 16), ARCH_VMM_AREA_HEAP);
    void* qused = (void*)arch_vmm_p2v(pbuf + (qsize * 16) + (qsize * 2 + 6), ARCH_VMM_AREA_HEAP);

    if (!qdesc || !qfree || !qused)
        return errno = EFAULT, -1;


    memset(qdesc, 0, qsize * 16);
    memset(qfree, 0, qsize * 2 + 6);
    memset(qused, 0, qsize * 8 + 6);


    driver->internals.queues[index].buffers.sendbuf = pbuf + (qsize * 16) + (qsize * 2 + 6) + (qsize * 8 + 6);
    driver->internals.queues[index].buffers.recvbuf = pbuf + (qsize * 16) + (qsize * 2 + 6) + (qsize * 8 + 6) + (qsize * driver->send_window_size);

    driver->internals.queues[index].descriptors = (struct virtq_descriptor volatile*)qdesc;
    driver->internals.queues[index].available   = (struct virtq_available volatile*)qfree;
    driver->internals.queues[index].used        = (struct virtq_used volatile*)qused;
    driver->internals.queues[index].notify      = (struct virtq_notify volatile*)driver->internals.notify_offset + (cfg->queue_notify_off * driver->internals.notify_off_multiplier);
    driver->internals.queues[index].size        = qsize;

    sem_init(&driver->internals.queues[index].iosem, 0);



#if defined(CONFIG_HAVE_PCI_MSIX)
    cfg->queue_msix_vector = cpu_to_le16(index);
#else
    cfg->queue_msix_vector = cpu_to_le16(VIRTIO_MSI_NO_VECTOR);
#endif

    cfg->queue_desc   = cpu_to_le64(pbuf + (0));
    cfg->queue_driver = cpu_to_le64(pbuf + (qsize * 16));
    cfg->queue_device = cpu_to_le64(pbuf + (qsize * 16) + (qsize * 2 + 6));
    cfg->queue_enable = cpu_to_le16(1);

    atomic_thread_fence(memory_order_release);


#if DEBUG_LEVEL_TRACE
    kprintf("virtio-queue: driver %d initialized queue %d successful [desc(%p), device(%p), driver(%p), enable(%d), msix(%p), notify(%p), size(%d)]\n", driver->device, index, cfg->queue_desc, cfg->queue_device, cfg->queue_driver,
            cfg->queue_enable, cfg->queue_msix_vector, cfg->queue_notify_off, cfg->queue_size);
#endif

    return 0;
}



int virtq_get_free_descriptor(struct virtio_driver* driver, uint16_t queue) {

    DEBUG_ASSERT(driver);
    DEBUG_ASSERT(driver->device);

    DEBUG_ASSERT(queue < driver->internals.num_queues);


    for (uint16_t d = 1; d < driver->internals.queues[queue].size; d++) {


        if (driver->internals.queues[queue].descriptors[d].q_address)
            continue;


        if (({ // Search in available buffers
                int free = 1;

                for (uint16_t i = 0; i < driver->internals.queues[queue].size; i++) {

                    if (driver->internals.queues[queue].available->q_ring[i] != cpu_to_le16(d))
                        continue;

                    free = 0;
                    break;
                }

                free;

            }) == 0)
            continue;


        driver->internals.queues[queue].descriptors[d].q_address = 0xFFFFFFFFFFFFFFFF;
        driver->internals.queues[queue].descriptors[d].q_length  = 0;
        driver->internals.queues[queue].descriptors[d].q_flags   = 0;
        driver->internals.queues[queue].descriptors[d].q_next    = 0;

        return d;
    }

    return -1;
}



ssize_t virtq_sendrecv(struct virtio_driver* driver, uint16_t queue, void* message, size_t size, void* output, size_t outsize) {

    DEBUG_ASSERT(driver);
    DEBUG_ASSERT(driver->device);
    DEBUG_ASSERT(message);
    DEBUG_ASSERT(output);
    DEBUG_ASSERT(size);
    DEBUG_ASSERT(outsize);

    DEBUG_ASSERT(queue < driver->internals.num_queues);
    DEBUG_ASSERT(size < driver->send_window_size);
    DEBUG_ASSERT(outsize < driver->recv_window_size);



    ssize_t inp = virtq_get_free_descriptor(driver, queue);
    ssize_t out = virtq_get_free_descriptor(driver, queue);


#if DEBUG_LEVEL_TRACE
    kprintf("virtio-queue: device %d is sendrecv on queue %d [inp(%d), out(%d), message(%p), size(%p), output(%p), outsize(%p)]\n", driver->device, queue, inp, out, message, size, output, outsize);
#endif


    if (unlikely(inp < 0 || out < 0)) {

#if DEBUG_LEVEL_FATAL
        kprintf("virtio-queue: FAIL! device %d has no free descriptor in queue %d\n", driver->device, queue);
#endif

        return errno = ENOSPC, -1;
    }



    memcpy((void*)arch_vmm_p2v(driver->internals.queues[queue].buffers.sendbuf + (inp * driver->send_window_size), ARCH_VMM_AREA_HEAP), message, size);



    atomic_thread_fence(memory_order_acq_rel);

    driver->internals.queues[queue].descriptors[inp].q_address = cpu_to_le64(driver->internals.queues[queue].buffers.sendbuf + (inp * driver->send_window_size));
    driver->internals.queues[queue].descriptors[inp].q_length  = cpu_to_le32(size);
    driver->internals.queues[queue].descriptors[inp].q_flags   = cpu_to_le16(VIRTQ_DESC_F_NEXT);
    driver->internals.queues[queue].descriptors[inp].q_next    = cpu_to_le16(out);

    driver->internals.queues[queue].descriptors[out].q_address = cpu_to_le64(driver->internals.queues[queue].buffers.recvbuf + (out * driver->recv_window_size));
    driver->internals.queues[queue].descriptors[out].q_length  = cpu_to_le32(outsize);
    driver->internals.queues[queue].descriptors[out].q_flags   = cpu_to_le16(VIRTQ_DESC_F_WRITE);
    driver->internals.queues[queue].descriptors[out].q_next    = cpu_to_le16(0);



    atomic_thread_fence(memory_order_acq_rel);

    uint16_t seen = le16_to_cpu(driver->internals.queues[queue].used->q_idx);
    uint16_t next = le16_to_cpu(driver->internals.queues[queue].available->q_idx) % driver->internals.queues[queue].size;

    driver->internals.queues[queue].available->q_ring[next] = cpu_to_le16(inp);
    driver->internals.queues[queue].available->q_flags      = cpu_to_le16(0);
    driver->internals.queues[queue].available->q_idx        = cpu_to_le16(le16_to_cpu(driver->internals.queues[queue].available->q_idx) + 1);

    atomic_thread_fence(memory_order_acq_rel);

    driver->internals.queues[queue].notify->n_idx = cpu_to_le16(queue);

    atomic_thread_fence(memory_order_acq_rel);



    sem_wait(&driver->internals.queues[queue].iosem);


    do {

        for (size_t i = seen; i < le16_to_cpu(driver->internals.queues[queue].used->q_idx); i++) {

            if (le32_to_cpu(driver->internals.queues[queue].used->q_elements[i].e_id) != inp)
                continue;


            memcpy(output, (void*)arch_vmm_p2v(driver->internals.queues[queue].buffers.recvbuf + (out * driver->recv_window_size), ARCH_VMM_AREA_HEAP), outsize);


#if DEBUG_LEVEL_TRACE
            kprintf("virtio-queue: device %d has received %d bytes from idx: %d\n", driver->device, le32_to_cpu(driver->internals.queues[queue].used->q_elements[i].e_length), i);
#endif

            seen = 0xFFFF;
            break;
        }

        atomic_thread_fence(memory_order_seq_cst);

    } while (seen != 0xFFFF);


#if DEBUG_LEVEL_TRACE
    kprintf("virtio-queue: device %d has sent and received %ld bytes of data on queue %d\n", driver->device, outsize, queue);
#endif

    return outsize;
}


ssize_t virtq_send(struct virtio_driver* driver, uint16_t queue, void* message, size_t size) {

    DEBUG_ASSERT(driver);
    DEBUG_ASSERT(driver->device);
    DEBUG_ASSERT(message);
    DEBUG_ASSERT(size);

    DEBUG_ASSERT(queue < driver->internals.num_queues);
    DEBUG_ASSERT(size < driver->send_window_size);



    ssize_t inp = virtq_get_free_descriptor(driver, queue);


#if DEBUG_LEVEL_TRACE
    kprintf("virtio-queue: device %d send on queue %d [inp(%d), message(%p), size(%p)]\n", driver->device, queue, inp, message, size);
#endif


    if (unlikely(inp < 0)) {

#if DEBUG_LEVEL_FATAL
        kprintf("virtio-queue: FAIL! device %d has no free descriptor in queue %d\n", driver->device, queue);
#endif

        return errno = ENOSPC, -1;
    }



    memcpy((void*)arch_vmm_p2v(driver->internals.queues[queue].buffers.sendbuf + (inp * driver->send_window_size), ARCH_VMM_AREA_HEAP), message, size);



    atomic_thread_fence(memory_order_acq_rel);

    driver->internals.queues[queue].descriptors[inp].q_address = cpu_to_le64(driver->internals.queues[queue].buffers.sendbuf + (inp * driver->send_window_size));
    driver->internals.queues[queue].descriptors[inp].q_length  = cpu_to_le32(size);
    driver->internals.queues[queue].descriptors[inp].q_flags   = cpu_to_le16(0);
    driver->internals.queues[queue].descriptors[inp].q_next    = cpu_to_le16(0);

    atomic_thread_fence(memory_order_acq_rel);


    uint16_t next = le16_to_cpu(driver->internals.queues[queue].available->q_idx) % driver->internals.queues[queue].size;

    driver->internals.queues[queue].available->q_ring[next] = cpu_to_le16(inp);
    driver->internals.queues[queue].available->q_flags      = cpu_to_le16(0);
    driver->internals.queues[queue].available->q_idx        = cpu_to_le16(le16_to_cpu(driver->internals.queues[queue].available->q_idx) + 1);


    driver->internals.queues[queue].notify->n_idx = cpu_to_le16(queue);


    atomic_thread_fence(memory_order_acq_rel);


    return size;
}



void virtq_flush(struct virtio_driver* driver, uint16_t queue) {
    sem_post(&driver->internals.queues[queue].iosem);
}



void init(const char* args) {
}

void dnit(void) {
}
