#include <aplus.h>
#include <aplus/base.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <aplus/debug.h>
#include <aplus/mm.h>
#include <aplus/network.h>
#include <aplus/virtio.h>
#include <aplus/utils/list.h>
#include <libc.h>

#if defined(__i386__) 
#include <arch/i386/i386.h>
#include <arch/i386/pci.h>
#endif

MODULE_NAME("virtio/network");
MODULE_DEPS("virtio");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


static void virtio_network_handler(void* unused) {
    list_each(virtio_devices, d) {
        if(d->type != VIRTIO_TYPE_NETWORK)
            continue;

        if(!d->io)
            continue;

        uint8_t v = virtio_r8(d->io + VIRTIO_IO_ISR_STATUS);
        if(!(v & 1))
            continue;

        /* RECEIVE */
    }
}


static int virtio_network_startoutput(void* internals) {

}

static void virtio_network_output(void* internals, void* buf, uint16_t len) {

}

static void virtio_network_endoutput(void* internals, uint16_t len) {

}

static int virtio_network_startinput(void* internals) {

}

static void virtio_network_input(void* internals, void* buf, uint16_t len) {

}

static void virtio_network_endinput(void* internals) {

}

static void virtio_network_input_nomem(void* internals, uint16_t len) {

}

static void virtio_network_init(void* internals, uint8_t* address, void* mcast) {

}


static void virtio_network_init_device(virtio_device_t* d) {
    struct ethif* eth = (struct ethif*) kmalloc(sizeof(struct ethif), GFP_KERNEL);
    if(!eth)
        return;

    memset(eth, 0, sizeof(struct ethif));
    eth->internals = (void*) d;


#if defined(__i386__) || defined(__x86_64__)
    uint16_t cmd = pci_read_field(d->pci, PCI_COMMAND, 4);
    if(!(cmd & (1 << 2))) 
        pci_write_field(d->pci, PCI_COMMAND, 4, cmd | (1 << 2));
#endif

    irq_enable(d->irq, virtio_network_handler);


    int i;
    for(i = 0; i < 6; i++)
        eth->address[i] = virtio_r8(d->io + 0x14 + i);


    eth->low_level_init = virtio_network_init;
    eth->low_level_startoutput = virtio_network_startoutput;
    eth->low_level_output = virtio_network_output;
    eth->low_level_endoutput = virtio_network_endoutput;
    eth->low_level_startinput = virtio_network_startinput;
    eth->low_level_input = virtio_network_input;
    eth->low_level_endinput = virtio_network_endinput;
    eth->low_level_input_nomem = virtio_network_input_nomem;


    IP4_ADDR(&eth->ip, 10, 0, 2, 15);
    IP4_ADDR(&eth->nm, 255, 255, 255, 0);
    IP4_ADDR(&eth->gw, 10, 0, 2, 2);

    struct netif* netif = (struct netif*) kmalloc(sizeof(struct netif), GFP_KERNEL);
    if(unlikely(!netif)) {
        kprintf(ERROR "virtio-network: no memory left!\n");

        kfree(eth);
        return;
    }

    if(!netif_add(netif, &eth->ip, &eth->nm, &eth->gw, eth, ethif_init, ethernet_input)) {
        kprintf(ERROR "virtio-network: netif_add() failed\n");

        kfree(eth);
        kfree(netif);
        return;
    }

    netif_set_default(netif);
    netif_set_up(netif);
}

int init(void) {
    list_each(virtio_devices, d) {
        if(d->type != VIRTIO_TYPE_NETWORK)
            continue;

        virtio_network_init_device(d);
    }

    return 0;
}

int dnit(void) {
    return 0;
}