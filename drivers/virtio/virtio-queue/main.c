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

#include <stdint.h>
#include <string.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/module.h>
#include <aplus/memory.h>
#include <aplus/smp.h>
#include <aplus/hal.h>
#include <aplus/fb.h>
#include <aplus/errno.h>
#include <aplus/endian.h>

#include <dev/interface.h>
#include <dev/video.h>
#include <dev/pci.h>

#include <dev/virtio/virtio.h>


MODULE_NAME("virtio/virtio-queue");
MODULE_DEPS("dev/interface,dev/pci");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");



int virtq_init(struct virtio_driver* driver, struct virtio_pci_common_cfg volatile* cfg, uint16_t index) {

    DEBUG_ASSERT(driver);
    DEBUG_ASSERT(cfg);
    DEBUG_ASSERT(cfg->queue_size);


    __atomic_store_n(&cfg->queue_select, cpu_to_le16(index), __ATOMIC_SEQ_CST);


    size_t qsize = cfg->queue_size;


    uintptr_t pbuf = pmm_alloc_blocks(((qsize * 16) + (qsize * 2 + 6) + (qsize * 8 + 6) + (qsize * driver->send_window_size) + (qsize * driver->recv_window_size)) / PML1_PAGESIZE + 1);

    if(unlikely(!pbuf))
        return -ENOMEM;


    void* qdesc = (void*) arch_vmm_p2v(pbuf + (0),                            ARCH_VMM_AREA_HEAP);
    void* qfree = (void*) arch_vmm_p2v(pbuf + (qsize * 16),                   ARCH_VMM_AREA_HEAP);
    void* qused = (void*) arch_vmm_p2v(pbuf + (qsize * 16) + (qsize * 2 + 6), ARCH_VMM_AREA_HEAP);

    if(!qdesc || !qfree || !qused)
        return -EFAULT;


    memset(qdesc, 0, qsize * 16);
    memset(qfree, 0, qsize * 2 + 6);
    memset(qused, 0, qsize * 8 + 6);


    driver->internals.queues[index].buffers.sendbuf = pbuf + (qsize * 16) + (qsize * 2 + 6) + (qsize * 8 + 6);
    driver->internals.queues[index].buffers.recvbuf = pbuf + (qsize * 16) + (qsize * 2 + 6) + (qsize * 8 + 6) + (qsize * driver->send_window_size);

    driver->internals.queues[index].descriptors = (struct virtq_descriptor volatile*) qdesc;
    driver->internals.queues[index].available   = (struct virtq_available volatile*) qfree;
    driver->internals.queues[index].used        = (struct virtq_used volatile*) qused;
    driver->internals.queues[index].notify      = (struct virtq_notify volatile*) driver->internals.notify_offset + (cfg->queue_notify_off * driver->internals.notify_off_multiplier);
    driver->internals.queues[index].size        = qsize;


    cfg->queue_desc   = cpu_to_le64(pbuf + (0));
    cfg->queue_driver = cpu_to_le64(pbuf + (qsize * 16));
    cfg->queue_device = cpu_to_le64(pbuf + (qsize * 16) + (qsize * 2 + 6));
    cfg->queue_msix_vector = cpu_to_le16(index);
    cfg->queue_enable = cpu_to_le16(1);

    __atomic_thread_fence(__ATOMIC_SEQ_CST);


#if defined(DEBUG) && DEBUG_LEVEL >= 4
    kprintf("virtio-queue: driver %d initialized queue %d successful [desc(%p), device(%p), driver(%p), enable(%d), msix(%p), notify(%p), size(%d)]\n",
        driver->device, index,
        cfg->queue_desc,
        cfg->queue_device,
        cfg->queue_driver,
        cfg->queue_enable,
        cfg->queue_msix_vector,
        cfg->queue_notify_off,
        cfg->queue_size
    );
#endif

    return 0;

}



int virtq_get_free_descriptor(struct virtio_driver* driver, uint16_t queue) {

    DEBUG_ASSERT(driver);
    DEBUG_ASSERT(driver->device);

    DEBUG_ASSERT(queue < driver->internals.num_queues);



    for(uint16_t d = 0; d < driver->internals.queues[queue].size; d++) {

        if(({ // Search in available buffers

            for(uint16_t i = 0; i < driver->internals.queues[queue].size; i++) {

                if(driver->internals.queues[queue].available->q_ring[i] == cpu_to_le16(d))
                    return 0;

            }

            1;

        }) == 0) continue;


        if(({ // Search in used buffers

            for(uint16_t i = 0; i < driver->internals.queues[queue].size; i++) {

                if(driver->internals.queues[queue].used->q_elements[i].e_id == cpu_to_le32(d))
                    return 0;

            }

            1;

        }) == 0) continue;

        return d;

    }

    return -1;

}


ssize_t virtq_send(struct virtio_driver* driver, uint16_t queue, void* message, size_t size) {

    DEBUG_ASSERT(driver);
    DEBUG_ASSERT(driver->device);
    DEBUG_ASSERT(message);
    DEBUG_ASSERT(size);

    DEBUG_ASSERT(queue < driver->internals.num_queues);
    DEBUG_ASSERT(size < driver->send_window_size);


    int desc;

    if((desc = virtq_get_free_descriptor(driver, queue)) < 0) {

#if defined(DEBUG) && DEBUG_LEVEL >= 0
        kprintf("virtio-queue: FAIL! device %d has no free descriptor in queue %d\n", driver->device, queue);
#endif

        return -ENOSPC;

    }


    memcpy (
        (void*) arch_vmm_p2v(driver->internals.queues[queue].buffers.sendbuf + (desc * driver->send_window_size), ARCH_VMM_AREA_HEAP), 
        message, 
        size
    );


    uint16_t next = le16_to_cpu(driver->internals.queues[queue].available->q_idx);

    driver->internals.queues[queue].available->q_ring[next++] = cpu_to_le16(desc);
    driver->internals.queues[queue].available->q_idx = cpu_to_le16(next % driver->internals.queues[queue].size);

    driver->internals.queues[queue].descriptors[desc].q_address = cpu_to_le64(driver->internals.queues[queue].buffers.sendbuf + (desc * driver->send_window_size));
    driver->internals.queues[queue].descriptors[desc].q_length  = cpu_to_le32(size);
    driver->internals.queues[queue].descriptors[desc].q_flags   = cpu_to_le16(VIRTQ_DESC_F_WRITE);
    driver->internals.queues[queue].descriptors[desc].q_next    = cpu_to_le16(0);


    driver->internals.queues[queue].notify->n_idx = desc;

    __atomic_thread_fence(__ATOMIC_SEQ_CST);

    
    return size;

}



ssize_t virtq_recv(struct virtio_driver* driver, uint16_t queue, void* message, size_t size) {

    DEBUG_ASSERT(driver);
    DEBUG_ASSERT(driver->device);
    DEBUG_ASSERT(message);
    DEBUG_ASSERT(size);

    DEBUG_ASSERT(queue < driver->internals.num_queues);
    DEBUG_ASSERT(size < driver->send_window_size);


    int desc;

    if((desc = virtq_get_free_descriptor(driver, queue)) < 0) {

#if defined(DEBUG) && DEBUG_LEVEL >= 0
        kprintf("virtio-queue: FAIL! device %d has no free descriptor in queue %d\n", driver->device, queue);
#endif

        return -ENOSPC;

    }



    uint16_t next = le16_to_cpu(driver->internals.queues[queue].used->q_idx);

    driver->internals.queues[queue].used->q_elements[next].e_id = cpu_to_le16(desc);
    driver->internals.queues[queue].used->q_elements[next].e_length = cpu_to_le32(size);
    driver->internals.queues[queue].used->q_idx = cpu_to_le16(next % driver->internals.queues[queue].size);


    driver->internals.queues[queue].descriptors[desc].q_address = cpu_to_le64(driver->internals.queues[queue].buffers.recvbuf + (desc * driver->recv_window_size));
    driver->internals.queues[queue].descriptors[desc].q_length  = cpu_to_le32(size);
    driver->internals.queues[queue].descriptors[desc].q_flags   = cpu_to_le16(0);
    driver->internals.queues[queue].descriptors[desc].q_next    = cpu_to_le16(0);


    driver->internals.queues[queue].notify->n_idx = desc;

    __atomic_thread_fence(__ATOMIC_SEQ_CST);



    memcpy (
        message,
        (void*) arch_vmm_p2v(driver->internals.queues[queue].buffers.recvbuf + (desc * driver->recv_window_size), ARCH_VMM_AREA_HEAP), 
        size
    );

    
    return size;

}



void init(const char* args) {

    if(args && strstr(args, "virtio=disable"))
        return;

}

void dnit(void) {

}