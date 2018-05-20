#include <aplus.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <aplus/debug.h>
#include <aplus/mm.h>
#include <libc.h>

#include <aplus/virtio.h>
#include <aplus/base.h>
#include <aplus/utils/list.h>

#if defined(__i386__)
#include <arch/i386/i386.h>
#include <arch/i386/pci.h>
#include "virtio-i386.h"
#endif


MODULE_NAME("virtio");
MODULE_DEPS("");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


list(virtio_device_t*, virtio_devices);


int virtio_init_device(virtio_device_t* dev, void (*negotiate) (uint32_t* features)) {

    virtio_w8(dev->io + VIRTIO_IO_DEVICE_STATUS, VIRTIO_STATUS_ACK);
    virtio_w8(dev->io + VIRTIO_IO_DEVICE_STATUS, VIRTIO_STATUS_ACK | VIRTIO_STATUS_LOADED);

    uint32_t f = virtio_r32(dev->io + VIRTIO_IO_DEVICE_FEATURES);
    if(negotiate)
        negotiate(&f);

    virtio_w32(dev->io + VIRTIO_IO_GUEST_FEATURES, f);
    virtio_w8(dev->io + VIRTIO_IO_DEVICE_STATUS, VIRTIO_STATUS_ACK | VIRTIO_STATUS_LOADED | VIRTIO_STATUS_FEATURES_OK);

    if(!(virtio_r8(dev->io + VIRTIO_IO_DEVICE_STATUS) & VIRTIO_STATUS_FEATURES_OK)) {
        kprintf("virtio: device %d failed to negotiate features", dev->pci);
        return -1;
    }


    int i;
    for(i = 0; i < 16; i++) {
        vqueue_t* v = &dev->vqueue[i];
        memset(v, 0, sizeof(vqueue_t));


        virtio_w16(dev->io + VIRTIO_IO_QUEUE_SELECT, i);
        
        if((v->size = virtio_r16(dev->io + VIRTIO_IO_QUEUE_SIZE)) == 0)
            continue;
        


        #define P(x)    ((x + 0xFFF) >> 12)

        size_t vsize = ((
                            P((sizeof(vqueue_buffer_t) * v->size) + (4 + v->size * 2)) +
                            P(4 + v->size * sizeof(vqueue_used_item_t))
                        )) << 12;

        #undef P


        v->address = (uintptr_t) kvalloc(vsize, GFP_KERNEL);
        if(!v->address) {
            kprintf(INFO "virtio: no memory left for %d bytes. Device %d, queue %d\n", vsize, dev->pci, i);
            return -1;
        }



        memset((void*) v->address, 0, vsize);
        v->available = (vqueue_available_t*) (v->address + sizeof(vqueue_buffer_t) * v->size);
        v->used = (vqueue_used_t*) (v->address + (((sizeof(vqueue_buffer_t) * v->size) + (4 + v->size * 2) + 0xFFF) & ~0xFFF));
        v->next_buffer = 0;
        v->lock = 0;


        virtio_w32(dev->io + VIRTIO_IO_QUEUE_ADDRESS, V2P(v->address) >> 12);
        v->available->flags = 0;
    }

    virtio_w8(dev->io + VIRTIO_IO_DEVICE_STATUS, VIRTIO_STATUS_ACK | VIRTIO_STATUS_LOADED | VIRTIO_STATUS_FEATURES_OK | VIRTIO_STATUS_READY);
    return 0;
}

int virtio_send_buffer(virtio_device_t* dev, uint16_t index, vqueue_buffer_info_t* b, uint64_t count) {
    vqueue_t* v = &dev->vqueue[index];
    if(unlikely(v->lock)) {
        errno = EAGAIN;
        return -1;
    }

    v->lock = 1;

    uint8_t* s = (uint8_t*) &v->buffer[v->chunk_size * v->next_buffer];
    uint8_t* p = s;

    v->available->rings[v->available->index % v->size] = v->next_buffer;
    
    uint16_t bidx = v->next_buffer;
    uint16_t nidx = 0;

    uint64_t i;
    for(i = 0; i < count; i++) {
        nidx = (bidx + 1) % v->size;

        vqueue_buffer_info_t* bi = &b[i];
        v->buffers[bidx].flags = bi->flags;

        if(i != (count - 1))
            v->buffers[bidx].flags |= VIRTIO_DESC_FLAG_NEXT;

        v->buffers[bidx].next = nidx;
        v->buffers[bidx].length = bi->size;

        if(bi->copy) {
            v->buffers[bidx].address = V2P(p);

            if(bi->buffer != 0)
                memcpy(bi->buffer, p, bi->size);
            p += bi->size;
        } else
            v->buffers[bidx].address = (uint64_t) ((uintptr_t) bi->buffer);

        bidx = nidx;
    }

    v->next_buffer = bidx;
    v->available->index++;

    virtio_w32(dev->io + VIRTIO_IO_QUEUE_NOTIFY, index);
    v->lock = 0;
}

void vqueue_enable_interrupts(vqueue_t* v) {
    v->used->flags = 0;
}

void vqueue_disable_interrupts(vqueue_t* v) {
    v->used->flags = 1;
}



int init(void) {
    memset(&virtio_devices, 0, sizeof(virtio_devices));

#if DEBUG
    static char* devtype[] = {
        NULL,
        "Network Card",
        "Block Device",
        "Console",
        "Entropy Source",
        "Memory Ballooing",
        "IO Memory",
        "RPMSG",
        "SCSI Host",
        "9P Transport",
        "802.11 WLAN",
        NULL
    };
#endif

#if defined(__i386__)
    void find_pci(uint32_t device, uint16_t venid, uint16_t devid, void* data) {
        if(!((venid == 0x1AF4) && ((devid >= 0x1000) && (devid <= 0x103F))))
            return;
       

        virtio_device_t* d = (virtio_device_t*) kmalloc(sizeof(virtio_device_t), GFP_KERNEL);
        d->pci = device;
        d->irq = pci_read_field(device, PCI_INTERRUPT_LINE, 1);
        d->type = pci_read_field(device, PCI_SUBVENID, 2) & 0xFFFF;
        d->io = pci_read_field(device, PCI_BAR0, 4) & 0xFFFC;
        d->mmio = pci_read_field(device, PCI_BAR1, 4) & 0xFFFFFFF0;


        kprintf(INFO "virtio: found \'%s\', io: %p, irq: %d\n", devtype[d->type], d->io, d->irq);
        list_push(virtio_devices, d);
    }

    pci_scan(&find_pci, -1, NULL);
#endif


    return 0;
}


int dnit(void) {
    return 0;
}
