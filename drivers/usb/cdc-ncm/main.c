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
#include <aplus/endian.h>
#include <aplus/memory.h>
#include <aplus/module.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <dev/interface.h>
#include <dev/network.h>
#include <dev/usb/usb.h>
#include <lwip/dhcp.h>
#include <lwip/tcpip.h>


MODULE_NAME("usb/cdc-ncm");
MODULE_DEPS("dev/interface,dev/network,usb/core");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


#define USB_CLASS_COMM     0x02
#define USB_CLASS_CDC_DATA 0x0a
#define USB_CLASS_MISC     0xef
#define CDC_SUBCLASS_NCM   0x0d

#define CDC_CS_INTERFACE       0x24
#define CDC_ETHERNET_FUNCTION  0x0f
#define CDC_NCM_FUNCTION       0x1a
#define NCM_SET_PACKET_FILTER  0x43
#define NCM_SET_NTB_INPUT_SIZE 0x86

#define NCM_CAP_PACKET_FILTER (1 << 0)
#define NCM_CAP_NTB_INPUT_8   (1 << 5)

#define NCM_FILTER_PROMISCUOUS   (1 << 0)
#define NCM_FILTER_ALL_MULTICAST (1 << 1)
#define NCM_FILTER_DIRECTED      (1 << 2)
#define NCM_FILTER_BROADCAST     (1 << 3)
#define DT_STRING              3
#define LANGID_EN_US           0x0409

#define NTH16_SIGNATURE 0x484d434e
#define NDP16_NOCRC     0x304d434e
#define NDP16_CRC       0x314d434e
#define NTH16_SIZE      12
#define NDP16_SIZE      16
#define NTB_TX_HDR      (NTH16_SIZE + NDP16_SIZE)

#define NCM_FRAME_MAX    1514
#define NCM_TX_SLOT_SIZE (NTB_TX_HDR + NCM_FRAME_MAX + 1)
#define NCM_TX_SLOTS     32
#define NCM_RX_SIZE      16384
#define NCM_RX_DEPTH     8

#define NCM_INST(dev) ((ncm_t*)(dev)->data)

#define get16(p, off) *((uint16_t*)((p) + (off)))
#define get32(p, off) *((uint32_t*)((p) + (off)))
#define put16(p, off, v) *((uint16_t*)((p) + (off))) = (v)
#define put32(p, off, v) *((uint32_t*)((p) + (off))) = (v)


typedef struct {
    usbdev_t* usb;
    endpoint_t* in;
    endpoint_t* out;
    void* rxq;
    uint16_t sequence;
    bool running;

    const uint8_t* rx_data;
    uint16_t rx_length;
    uint16_t rx_offset;

    uint8_t tx[NCM_TX_SLOTS][NCM_TX_SLOT_SIZE];
    uint16_t tx_length[NCM_TX_SLOTS];
    atomic_size_t tx_head;
    atomic_size_t tx_tail;

    device_t device;
} ncm_t;

static atomic_int num_devices = 0;



static int ncm_request(usbdev_t* dev, direction_t dir, dev_req_type type, dev_req_recp recp, int request, int value, int index, void* buf, int len) {

    dev_req_t dr;

    dr.bmRequestType = gen_bmRequestType(dir == OUT ? host_to_device : device_to_host, type, recp);
    dr.bRequest      = request;
    dr.wValue        = value;
    dr.wIndex        = index;
    dr.wLength       = len;

    return dev->controller->control(dev, dir, sizeof(dr), &dr, len, buf);
}


static int ncm_hex(uint16_t c) {

    if (c >= '0' && c <= '9') {
        return c - '0';
    }

    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }

    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }

    return -1;
}


static bool ncm_read_mac(usbdev_t* dev, int index, uint8_t* mac) {

    uint8_t buf[2 + 12 * 2];

    if (!index) {
        return false;
    }

    if (ncm_request(dev, IN, standard_type, dev_recp, GET_DESCRIPTOR, (DT_STRING << 8) | index, LANGID_EN_US, buf, sizeof(buf)) < (int)sizeof(buf)) {
        return false;
    }

    for (int i = 0; i < 12; i++) {

        int n = ncm_hex(get16(buf, 2 + i * 2));

        if (n < 0) {
            return false;
        }

        mac[i / 2] = (i & 1) ? (mac[i / 2] | n) : (n << 4);
    }

    return true;
}


static void ncm_low_level_init(void* internals, uint8_t* address, void* unused) {
    (void)internals;
    (void)address;
    (void)unused;
}


static int ncm_startoutput(void* internals) {

    ncm_t* n = internals;

    size_t head = atomic_load(&n->tx_head);

    if (!n->running || head - atomic_load(&n->tx_tail) >= NCM_TX_SLOTS) {
        return 0;
    }

    n->tx_length[head % NCM_TX_SLOTS] = NTB_TX_HDR;

    return 1;
}


static void ncm_output(void* internals, void* buf, uint16_t len) {

    ncm_t* n = internals;

    size_t slot = atomic_load(&n->tx_head) % NCM_TX_SLOTS;

    uint16_t used = n->tx_length[slot];

    if (used + len > NTB_TX_HDR + NCM_FRAME_MAX) {
        len = NTB_TX_HDR + NCM_FRAME_MAX - used;
    }

    memcpy(&n->tx[slot][used], buf, len);

    n->tx_length[slot] = used + len;
}


static void ncm_endoutput(void* internals, uint16_t len) {

    ncm_t* n = internals;

    size_t head = atomic_load(&n->tx_head);
    size_t slot = head % NCM_TX_SLOTS;

    uint8_t* ntb   = n->tx[slot];
    uint16_t total = n->tx_length[slot];

    if (total % n->out->maxpacketsize == 0) {
        ntb[total++] = 0;
    }

    put32(ntb, 0, NTH16_SIGNATURE);
    put16(ntb, 4, NTH16_SIZE);
    put16(ntb, 6, n->sequence++);
    put16(ntb, 8, total);
    put16(ntb, 10, NTH16_SIZE);

    put32(ntb, 12, NDP16_NOCRC);
    put16(ntb, 16, NDP16_SIZE);
    put16(ntb, 18, 0);
    put16(ntb, 20, NTB_TX_HDR);
    put16(ntb, 22, n->tx_length[slot] - NTB_TX_HDR);
    put16(ntb, 24, 0);
    put16(ntb, 26, 0);

    n->tx_length[slot] = total;

    atomic_store(&n->tx_head, head + 1);
}


static int ncm_startinput(void* internals) {
    return ((ncm_t*)internals)->rx_length;
}


static void ncm_input(void* internals, void* buf, uint16_t len) {

    ncm_t* n = internals;

    if (n->rx_offset + len > n->rx_length) {
        len = n->rx_length - n->rx_offset;
    }

    memcpy(buf, n->rx_data + n->rx_offset, len);

    n->rx_offset += len;
}


static void ncm_endinput(void* internals) {

    ncm_t* n = internals;

    n->rx_data   = NULL;
    n->rx_length = 0;
    n->rx_offset = 0;
}


static void ncm_input_nomem(void* internals, uint16_t len) {
#if DEBUG_LEVEL_WARN
    kprintf("usb-cdc-ncm: WARN! no memory for a %d byte frame, dropping it\n", len);
#endif
}


static void ncm_receive(ncm_t* n, const uint8_t* ntb) {

    if (get32(ntb, 0) != NTH16_SIGNATURE || get16(ntb, 4) != NTH16_SIZE) {
        return;
    }

    size_t block = get16(ntb, 8);
    size_t ndp   = get16(ntb, 10);

    if (block > NCM_RX_SIZE) {
        return;
    }

    for (int tables = 0; ndp && tables < 8; tables++) {

        if (ndp % 4 || ndp + 8 > block) {
            return;
        }

        uint32_t sig = get32(ntb, ndp);
        size_t end   = ndp + get16(ntb, ndp + 4);

        if ((sig != NDP16_NOCRC && sig != NDP16_CRC) || end > block) {
            return;
        }

        for (size_t e = ndp + 8; e + 4 <= end; e += 4) {

            size_t idx = get16(ntb, e);
            size_t len = get16(ntb, e + 2);

            if (!idx || !len) {
                break;
            }

            if (len > NCM_FRAME_MAX || idx + len > block) {
                continue;
            }

            n->rx_data   = ntb + idx;
            n->rx_length = len;
            n->rx_offset = 0;

            ethif_input(&n->device.net.interface);

            ncm_endinput(n);
        }

        ndp = get16(ntb, ndp + 6);
    }
}


static void ncm_poll(usbdev_t* dev) {

    ncm_t* n = NCM_INST(dev);

    if (!n || !n->running) {
        return;
    }

    const uint8_t* buf;

    while ((buf = dev->controller->poll_intr_queue(n->rxq))) {
        ncm_receive(n, buf);
    }

    size_t tail = atomic_load(&n->tx_tail);

    while (tail != atomic_load(&n->tx_head)) {

        size_t slot = tail % NCM_TX_SLOTS;

        if (dev->controller->bulk(n->out, n->tx_length[slot], n->tx[slot], 0) < 0) {
            LINK_STATS_INC(link.drop);
        }

        atomic_store(&n->tx_tail, ++tail);
    }
}


static void ncm_pick_default(struct netif* gone) {

    if (netif_default && netif_default != gone) {
        return;
    }

    struct netif* i;

    NETIF_FOREACH(i) {
        if (i != gone && netif_is_up(i) && !(i->name[0] == 'l' && i->name[1] == 'o')) {
            netif_set_default(i);
            return;
        }
    }
}


static void ncm_destroy(usbdev_t* dev) {

    ncm_t* n = NCM_INST(dev);

    if (!n) {
        return;
    }

    if (n->running) {

        n->running = false;

        LOCK_TCPIP_CORE();

        struct netif* netif = &n->device.net.interface;

        dhcp_release_and_stop(netif);
        ncm_pick_default(netif);
        netif_remove(netif);
        dhcp_cleanup(netif);

        UNLOCK_TCPIP_CORE();

#if DEBUG_LEVEL_INFO
        kprintf("usb-cdc-ncm: %s detached\n", n->device.name);
#endif
    }

    if (n->rxq) {
        dev->controller->destroy_intr_queue(n->in, n->rxq);
    }

    kfree(n);
    dev->data = NULL;
}


static bool ncm_probe(usbdev_t* dev, int* control, int* data, int* imac, uint8_t* caps) {

    configuration_descriptor_t* cd = dev->configuration;

    uint8_t* ptr = (uint8_t*)cd + cd->bLength;
    uint8_t* end = (uint8_t*)cd + cd->wTotalLength;

    *control = -1;
    *data    = -1;
    *imac    = 0;
    *caps    = 0;

    bool inside = false;

    for (; ptr + 2 <= end && ptr[0] && ptr + ptr[0] <= end; ptr += ptr[0]) {

        if (ptr[1] == DT_INTF && ptr[0] >= sizeof(interface_descriptor_t)) {

            const interface_descriptor_t* intf = (const interface_descriptor_t*)ptr;

            inside = false;

            if (intf->bAlternateSetting != 0) {
                continue;
            }

            if (*control < 0 && intf->bInterfaceClass == USB_CLASS_COMM && intf->bInterfaceSubClass == CDC_SUBCLASS_NCM) {
                *control = intf->bInterfaceNumber;
                inside   = true;
            } else if (*control >= 0 && *data < 0 && intf->bInterfaceClass == USB_CLASS_CDC_DATA) {
                *data = intf->bInterfaceNumber;
            }

        } else if (inside && ptr[1] == CDC_CS_INTERFACE && ptr[0] >= 4 && ptr[2] == CDC_ETHERNET_FUNCTION) {
            *imac = ptr[3];
        } else if (inside && ptr[1] == CDC_CS_INTERFACE && ptr[0] >= 6 && ptr[2] == CDC_NCM_FUNCTION) {
            *caps = ptr[5];
        }
    }

    return *control >= 0 && *data >= 0;
}


static endpoint_t* ncm_endpoint(usbdev_t* dev, int intf, direction_t dir) {

    for (int i = 1; i < dev->num_endp; i++) {
        if (dev->endpoints[i].interface == intf && dev->endpoints[i].type == BULK && dev->endpoints[i].direction == dir) {
            return &dev->endpoints[i];
        }
    }

    return NULL;
}


static void ncm_init(usbdev_t* dev) {

    int control;
    int data;
    int imac;
    uint8_t caps;

    if (!ncm_probe(dev, &control, &data, &imac, &caps)) {
        dev->init = usb_generic_init;
        usb_generic_init(dev);
        return;
    }

    dev->data    = NULL;
    dev->destroy = ncm_destroy;

    ncm_t* n = kcalloc(1, sizeof(ncm_t), GFP_KERNEL);
    if (!n) {
        usb_detach_device(dev->controller, dev->address);
        return;
    }

    dev->data = n;

    n->usb = dev;
    n->in  = ncm_endpoint(dev, data, IN);
    n->out = ncm_endpoint(dev, data, OUT);

    if (!n->in || !n->out || !n->out->maxpacketsize) {
#if DEBUG_LEVEL_ERROR
        kprintf("usb-cdc-ncm: ERROR! %04x:%04x has no bulk endpoints\n", dev->descriptor->idVendor, dev->descriptor->idProduct);
#endif
        usb_detach_device(dev->controller, dev->address);
        return;
    }

    int n_dev = atomic_fetch_add(&num_devices, 1);

    if (!ncm_read_mac(dev, imac, n->device.net.address)) {

        static const uint8_t fallback[6] = {
            0x02, 0x00, 0x00, 0x00, 0x4e, 0x00
        };

        memcpy(n->device.net.address, fallback, sizeof(fallback));

        n->device.net.address[5] = (uint8_t)n_dev;
    }

    uint32_t insize[2] = {cpu_to_le32(NCM_RX_SIZE), 0};

    ncm_request(dev, OUT, class_type, iface_recp, NCM_SET_NTB_INPUT_SIZE, 0, control, insize, (caps & NCM_CAP_NTB_INPUT_8) ? 8 : 4);

    if (ncm_request(dev, OUT, standard_type, iface_recp, SET_INTERFACE, 1, data, NULL, 0) < 0) {
#if DEBUG_LEVEL_ERROR
        kprintf("usb-cdc-ncm: ERROR! could not enable the data interface\n");
#endif
        usb_detach_device(dev->controller, dev->address);
        return;
    }

    if (caps & NCM_CAP_PACKET_FILTER) {

        int filter = NCM_FILTER_DIRECTED | NCM_FILTER_BROADCAST | NCM_FILTER_ALL_MULTICAST;

#if defined(CONFIG_NETWORK_PROMISCUOUS)
        filter |= NCM_FILTER_PROMISCUOUS;
#endif

        if (ncm_request(dev, OUT, class_type, iface_recp, NCM_SET_PACKET_FILTER, filter, control, NULL, 0) < 0) {
#if DEBUG_LEVEL_WARN
            kprintf("usb-cdc-ncm: WARN! %04x:%04x refused packet filter 0x%02x\n", dev->descriptor->idVendor, dev->descriptor->idProduct, filter);
#endif
        }
    }

    n->rxq = dev->controller->create_intr_queue(n->in, NCM_RX_SIZE, NCM_RX_DEPTH, 0);

    if (!n->rxq) {
#if DEBUG_LEVEL_ERROR
        kprintf("usb-cdc-ncm: ERROR! could not queue bulk-in transfers\n");
#endif
        usb_detach_device(dev->controller, dev->address);
        return;
    }


    snprintf(n->device.name, DEVICE_MAXNAMELEN, "ncm%d", n_dev);
    strncpy(n->device.description, "USB CDC-NCM Network Device", DEVICE_MAXDESCLEN - 1);

    n->device.type   = DEVICE_TYPE_NETWORK;
    n->device.status = DEVICE_STATUS_READY;

    n->device.net.low_level_init        = ncm_low_level_init;
    n->device.net.low_level_startoutput = ncm_startoutput;
    n->device.net.low_level_output      = ncm_output;
    n->device.net.low_level_endoutput   = ncm_endoutput;
    n->device.net.low_level_startinput  = ncm_startinput;
    n->device.net.low_level_input       = ncm_input;
    n->device.net.low_level_endinput    = ncm_endinput;
    n->device.net.low_level_input_nomem = ncm_input_nomem;
    n->device.net.internals             = n;

    struct netif* netif = &n->device.net.interface;

    LOCK_TCPIP_CORE();

    if (!netif_add(netif, IP4_ADDR_ANY4, IP4_ADDR_ANY4, IP4_ADDR_ANY4, &n->device, ethif_init, tcpip_input)) {
        UNLOCK_TCPIP_CORE();
#if DEBUG_LEVEL_ERROR
        kprintf("usb-cdc-ncm: ERROR! netif_add() failed\n");
#endif
        usb_detach_device(dev->controller, dev->address);
        return;
    }

    n->running = true;

    netif_set_default(netif);
    netif_set_up(netif);
    dhcp_start(netif);

    UNLOCK_TCPIP_CORE();

    dev->poll = ncm_poll;

#if DEBUG_LEVEL_INFO
    kprintf("usb-cdc-ncm: %04x:%04x attached as %s [mac(%02X:%02X:%02X:%02X:%02X:%02X)]\n", dev->descriptor->idVendor, dev->descriptor->idProduct, n->device.name, n->device.net.address[0], n->device.net.address[1],
            n->device.net.address[2], n->device.net.address[3], n->device.net.address[4], n->device.net.address[5]);
#endif
}



void init(const char* args) {

    if (usb_register_class(USB_CLASS_COMM, ncm_init) < 0) {
#if DEBUG_LEVEL_WARN
        kprintf("usb-cdc-ncm: WARN! class 0x%02x already has a driver\n", USB_CLASS_COMM);
#endif
    }

    if (usb_register_class(USB_CLASS_MISC, ncm_init) < 0) {
#if DEBUG_LEVEL_WARN
        kprintf("usb-cdc-ncm: WARN! class 0x%02x already has a driver\n", USB_CLASS_MISC);
#endif
    }
}


void dnit(void) {
    usb_unregister_class(USB_CLASS_COMM);
    usb_unregister_class(USB_CLASS_MISC);
}
