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


    for(uint16_t d = 1; d < driver->internals.queues[queue].size; d++) {

        if(({ // Search in available buffers

            int free = 1;

            for(uint16_t i = 0; i < driver->internals.queues[queue].size; i++) {

                if(driver->internals.queues[queue].available->q_ring[i] != cpu_to_le16(d))
                    continue;

                free = 0;
                break;

            }

            free;

        }) == 0) continue;


        if(({ // Search in used buffers

            int free = 1;

            for(uint16_t i = 0; i < driver->internals.queues[queue].size; i++) {

                if(driver->internals.queues[queue].used->q_elements[i].e_id != cpu_to_le32(d))
                    continue;

                free = 1;
                break;

            }

            free;

        }) == 0) continue;

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

    {

        ssize_t in;
        if((in = virtq_get_free_descriptor(driver, queue)) < 0) {

#if defined(DEBUG) && DEBUG_LEVEL >= 0
            kprintf("virtio-queue: FAIL! device %d has no free descriptor in queue %d\n", driver->device, queue);
#endif

            return -ENOSPC;

        }


        memcpy (
            (void*) arch_vmm_p2v(driver->internals.queues[queue].buffers.sendbuf + (in * driver->send_window_size), ARCH_VMM_AREA_HEAP),
            message,
            size
        );




        uint16_t next = le16_to_cpu(driver->internals.queues[queue].available->q_idx);

        driver->internals.queues[queue].descriptors[in].q_address = cpu_to_le64(driver->internals.queues[queue].buffers.sendbuf + (in * driver->send_window_size));
        driver->internals.queues[queue].descriptors[in].q_length  = cpu_to_le32(size);
        driver->internals.queues[queue].descriptors[in].q_flags   = cpu_to_le16(VIRTQ_DESC_F_NEXT);
        driver->internals.queues[queue].descriptors[in].q_next    = cpu_to_le16(in + 1);
        __atomic_thread_fence(__ATOMIC_SEQ_CST);

        driver->internals.queues[queue].available->q_ring[next++] = cpu_to_le16(in);
        driver->internals.queues[queue].available->q_idx = cpu_to_le16(next % driver->internals.queues[queue].size);

        __atomic_thread_fence(__ATOMIC_SEQ_CST);



#if defined(DEBUG) && DEBUG_LEVEL >= 4
        kprintf("virtio-queue: device %d sent data from queue %d in descriptor %d\n", driver->device, queue, in);
#endif


    }


    {


        ssize_t out;
        if((out = virtq_get_free_descriptor(driver, queue)) < 0) {

#if defined(DEBUG) && DEBUG_LEVEL >= 0
            kprintf("virtio-queue: FAIL! device %d has no free descriptor in queue %d\n", driver->device, queue);
#endif

            return -ENOSPC;

        }


        uint16_t next = le16_to_cpu(driver->internals.queues[queue].available->q_idx);

        driver->internals.queues[queue].descriptors[out].q_address = cpu_to_le64(driver->internals.queues[queue].buffers.recvbuf + (out * driver->recv_window_size));
        driver->internals.queues[queue].descriptors[out].q_length  = cpu_to_le32(outsize);
        driver->internals.queues[queue].descriptors[out].q_flags   = cpu_to_le16(VIRTQ_DESC_F_WRITE);
        driver->internals.queues[queue].descriptors[out].q_next    = cpu_to_le16(0);
        __atomic_thread_fence(__ATOMIC_SEQ_CST);

        driver->internals.queues[queue].available->q_ring[next++] = cpu_to_le16(out);
        driver->internals.queues[queue].available->q_idx = cpu_to_le16(next % driver->internals.queues[queue].size);

        __atomic_thread_fence(__ATOMIC_SEQ_CST);



#if defined(DEBUG) && DEBUG_LEVEL >= 4
        kprintf("virtio-queue: device %d receiving data from queue %d in descriptor %d\n", driver->device, queue, out);
#endif

        __atomic_thread_fence(__ATOMIC_SEQ_CST);


        driver->internals.queues[queue].notify->n_idx = queue;

        __atomic_thread_fence(__ATOMIC_SEQ_CST);

    __asm__ __volatile__("mfence" ::: "memory");
    arch_timer_delay(100000);

        memcpy (
            output,
            (void*) arch_vmm_p2v(driver->internals.queues[queue].descriptors[2].q_address, ARCH_VMM_AREA_HEAP), 
            outsize
        );


            
    }




    // for(;;) {         arch_timer_delay(1000000);

    //     for(int i = 0; i < driver->internals.queues[queue].size; i++) {
    //         if(driver->internals.queues[queue].used->q_elements[i].e_id) {
    //             kprintf("used %d: id %d, len: %d, flags: %p, idx: %d\n\n",  i,
    //             driver->internals.queues[queue].used->q_elements[i].e_id,
    //             driver->internals.queues[queue].used->q_elements[i].e_length,
    //             *driver->internals.isr_status,
    //             driver->internals.queues[queue].used->q_idx
    //             );

    //             // size_t i = 0;
    //             // char* p = (char*) arch_vmm_p2v(driver->internals.queues[queue].buffers.recvbuf + (2 * driver->recv_window_size), ARCH_VMM_AREA_HEAP);
    //             // for(; i < outsize; i++)
    //             //     kprintf("%02X ", *p++ & 0xFF);
    //             // kprintf("\n\n\n");
    //         }



    //         if(driver->internals.queues[queue].available->q_ring[i]) {
    //             kprintf("avail %d: ring: %d, flags: %p, idx: %d\n\n",  i,
    //             driver->internals.queues[queue].available->q_ring[i],
    //             driver->internals.queues[queue].available->q_flags,
    //             driver->internals.queues[queue].available->q_idx);



    //         }

    //         break;

    //     }
    //     __atomic_thread_fence(__ATOMIC_SEQ_CST);

    // }



    
    return outsize;

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

    driver->internals.queues[queue].descriptors[desc].q_address = cpu_to_le64(driver->internals.queues[queue].buffers.sendbuf + (desc * driver->send_window_size));
    driver->internals.queues[queue].descriptors[desc].q_length  = cpu_to_le32(size);
    driver->internals.queues[queue].descriptors[desc].q_flags   = cpu_to_le16(0);
    driver->internals.queues[queue].descriptors[desc].q_next    = cpu_to_le16(0);

    driver->internals.queues[queue].available->q_ring[next++] = cpu_to_le16(desc);
    driver->internals.queues[queue].available->q_idx = cpu_to_le16(next % driver->internals.queues[queue].size);


    driver->internals.queues[queue].notify->n_idx = queue;

    __atomic_thread_fence(__ATOMIC_SEQ_CST);


#if defined(DEBUG) && DEBUG_LEVEL >= 4
    kprintf("virtio-queue: device %d sent data from queue %d in descriptor %d\n", driver->device, queue, desc);
#endif

    
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



    uint16_t next = le16_to_cpu(driver->internals.queues[queue].available->q_idx);

    driver->internals.queues[queue].descriptors[desc].q_address = cpu_to_le64(driver->internals.queues[queue].buffers.recvbuf + (desc * driver->recv_window_size));
    driver->internals.queues[queue].descriptors[desc].q_length  = cpu_to_le32(size);
    driver->internals.queues[queue].descriptors[desc].q_flags   = cpu_to_le16(VIRTQ_DESC_F_WRITE);
    driver->internals.queues[queue].descriptors[desc].q_next    = cpu_to_le16(0);

    driver->internals.queues[queue].available->q_ring[next++] = cpu_to_le16(desc);
    driver->internals.queues[queue].available->q_idx = cpu_to_le16(next % driver->internals.queues[queue].size);

    
    driver->internals.queues[queue].notify->n_idx = queue;

    __atomic_thread_fence(__ATOMIC_SEQ_CST);


#if defined(DEBUG) && DEBUG_LEVEL >= 4
    kprintf("virtio-queue: device %d receiving data from queue %d in descriptor %d\n", driver->device, queue, desc);
#endif

    for(;;) {
        for(int i = 0; i < driver->internals.queues[queue].size; i++) {
            if(driver->internals.queues[queue].used->q_elements[i].e_id)
            kprintf("used %d: id %d, len: %d, flags: %p, idx: %d\n",  i,
            driver->internals.queues[queue].used->q_elements[i].e_id,
            driver->internals.queues[queue].used->q_elements[i].e_length,
            *driver->internals.isr_status,
            driver->internals.queues[queue].used->q_idx
            );
        }
        __atomic_thread_fence(__ATOMIC_SEQ_CST);

        arch_timer_delay(1000000);
    }


    memcpy (
        message,
        (void*) arch_vmm_p2v(driver->internals.queues[queue].buffers.recvbuf + (desc * driver->recv_window_size), ARCH_VMM_AREA_HEAP), 
        size
    );

    
    return size;

}



void virtq_flush(struct virtio_driver* driver, uint16_t queue) {

    

}



void init(const char* args) {

    if(args && strstr(args, "virtio=disable"))
        return;

}

void dnit(void) {

}