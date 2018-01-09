#include <aplus.h>
#include <aplus/base.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <aplus/debug.h>
#include <aplus/mm.h>
#include <aplus/virtio.h>
#include <aplus/utils/list.h>
#include <libc.h>


#if defined(__i386__)
#include <arch/i386/i386.h>
#include <arch/i386/pci.h>
#endif

MODULE_NAME("virtio/block");
MODULE_DEPS("virtio,block/blkdev");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");



#define VIRTIO_BLK_F_SIZE_MAX               1
#define VIRTIO_BLK_F_SEG_MAX                2
#define VIRTIO_BLK_F_GEOMETRY               4
#define VIRTIO_BLK_F_RO                     5
#define VIRTIO_BLK_F_BLK_SIZE               6
#define VIRTIO_BLK_F_FLUSH                  9
#define VIRTIO_BLK_F_TOPOLOGY               10
#define VIRTIO_BLK_F_CONFIG_WCE             11
#define VIRTIO_BLK_T_IN                     0 
#define VIRTIO_BLK_T_OUT                    1 
#define VIRTIO_BLK_T_FLUSH                  4
#define VIRTIO_BLK_S_OK                     0 
#define VIRTIO_BLK_S_IOERR                  1 
#define VIRTIO_BLK_S_UNSUPP                 2

typedef struct {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
} virtio_block_request;


static void virtio_block_handler(void* unused) {
    list_each(virtio_devices, d) {
        if(d->type != VIRTIO_TYPE_BLOCK)
            continue;

        if(!d->io)
            continue;

        uint8_t v = virtio_r8(d->io + VIRTIO_IO_ISR_STATUS);
        if(!(v & 1))
            continue;

        vqueue_t* vq = &d->vqueue[0];
        while(vq->used_index != vq->used->index) {
            /* TODO */
        }
    }
}

static void virtio_block_init(virtio_device_t* d) {
#if defined(__i386__) || defined(__x86_64__)
    uint16_t cmd = pci_read_field(d->pci, PCI_COMMAND, 4);
    if(!(cmd & (1 << 2))) 
        pci_write_field(d->pci, PCI_COMMAND, 4, cmd | (1 << 2));
#endif



    uint64_t size;
    size = (uint64_t) virtio_r32(d->io + 0x18) << 32;
    size |= (uint64_t) virtio_r32(d->io + 0x14);
    size *= 512;



    void negotiate(uint32_t* f) {
        DISABLE_FEATURE(*f, VIRTIO_BLK_F_RO);
        DISABLE_FEATURE(*f, VIRTIO_BLK_F_BLK_SIZE);
        DISABLE_FEATURE(*f, VIRTIO_BLK_F_TOPOLOGY);
    }

    if(virtio_init_device(d, &negotiate) != E_OK) {
        kprintf(ERROR "virtio-blk: virtio_init_device() failed for device %d\n", d->pci);
        return;
    }

    vqueue_t* vq = &d->vqueue[0];
    if(!vq->address) {
        kprintf(ERROR "virtio-blk: invalid vqueue[0] address for device %d\n", d->pci);
        return;
    }

    vq->buffer = (uint8_t*) kvalloc(((vq->size * (sizeof(virtio_block_request) + 1)) + 0xFFF) & ~0xFFF, GFP_KERNEL);
    if(!vq->buffer) {
        kprintf(ERROR "virtio-blk: no kernel memory for device %d\n", d->pci);
        return;
    }

    irq_enable(d->irq, virtio_block_handler);
}

int init(void) {
    list_each(virtio_devices, d) {
        if(d->type != VIRTIO_TYPE_BLOCK)
            continue;

        virtio_block_init(d);
    }

    return E_OK;
}

int dnit(void) {

}