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

#ifndef _DEV_PCI_H
#define _DEV_PCI_H

#ifndef __ASSEMBLY__


    #include <aplus.h>
    #include <aplus/debug.h>
    #include <aplus/syscall.h>
    #include <stdint.h>

    #include <dev/interface.h>



typedef uint32_t pcidev_t;


    #define PCI_DEVICES_MAX     128

    #define PCI_VENDOR_ID   0x00
    #define PCI_DEVICE_ID   0x02
    #define PCI_COMMAND     0x04
    #define PCI_STATUS      0x06
    #define PCI_REVISION_ID 0x08


    #define PCI_PROG_IF         0x09
    #define PCI_SUBCLASS        0x0A
    #define PCI_CLASS           0x0B
    #define PCI_CACHE_LINE_SIZE 0x0C
    #define PCI_LATENCY_TIMER   0x0D
    #define PCI_HEADER_TYPE     0x0E
    #define PCI_BIST            0x0F

    #define PCI_BAR(i)         (PCI_BAR0 + (i * 4))
    #define PCI_BAR0           0x10
    #define PCI_BAR1           0x14
    #define PCI_BAR2           0x18
    #define PCI_BAR3           0x1C
    #define PCI_BAR4           0x20
    #define PCI_BAR5           0x24
    #define PCI_BAR_MM_MASK    (0xFFFFFFF0)
    #define PCI_BAR_IO_MASK    (0xFFFFFFFC)
    #define PCI_BAR_64_MM_MASK (0xFFFFFFFFFFFFFFF0)
    #define PCI_BAR_64_IO_MASK (0xFFFFFFFFFFFFFFFC)


    #define PCI_SUBSYSID 0x2C
    #define PCI_SUBVENID 0x2E

    #define PCI_INTERRUPT_PIN       0x3D
    #define PCI_INTERRUPT_LINE      0x3C
    #define PCI_INTERRUPT_LINE_NONE 0xFF

    #define PCI_SECONDARY_BUS 0x09

    #define PCI_CAPABILITIES 0x34


    #define PCI_HEADER_TYPE_DEVICE  0
    #define PCI_HEADER_TYPE_BRIDGE  1
    #define PCI_HEADER_TYPE_CARDBUS 2

    #define PCI_TYPE_ALL      -1
    #define PCI_TYPE_BRIDGE   0x0604
    #define PCI_TYPE_SATA     0x0106
    #define PCI_TYPE_VGA      0x0300
    #define PCI_TYPE_ETHERNET 0x0200
    #define PCI_TYPE_USB      0x0C03
    #define PCI_TYPE_SCSI     0x0100
    #define PCI_TYPE_AUDIO    0x0403



    #define PCI_ADDRESS_PORT 0xCF8
    #define PCI_VALUE_PORT   0xCFC

    #define PCI_NONE 0xFFFF


    #define PCI_COMMAND_REG_PIO            (1 << 0)
    #define PCI_COMMAND_REG_MMIO           (1 << 1)
    #define PCI_COMMAND_REG_BUS_MASTERING  (1 << 2)
    #define PCI_COMMAND_REG_SPECIAL_CYCLES (1 << 3)
    #define PCI_COMMAND_REG_RDWR_INVL      (1 << 4)
    #define PCI_COMMAND_REG_INTR_DISABLE   (1 << 10)


    #define PCI_STATUS_REG_INTERRUPT    (1 << 3)
    #define PCI_STATUS_REG_CAPABILITIES (1 << 4)


    #define PCI_MSI_CAPID        (0x05)
    
    #define PCI_MSI_HDR_CAPID    (0)
    #define PCI_MSI_HDR_CAPNEXT  (1)
    #define PCI_MSI_HDR_MSGCTL   (2)
    #define PCI_MSI_HDR_MSGADDR  (4)

    #define PCI_MSI_MSGCTL_ENABLE               (1 << 0)
    #define PCI_MSI_MSGCTL_ADDR_64BIT           (1 << 7)
    #define PCI_MSI_MSGCTL_MSG_COUNT_MASK       (0x0070)

    #define PCI_MSI_INTR_BASE   66
    

    #define PCI_MSIX_CAPID     (0x11)

    #define PCI_MSIX_HDR_CAPID        (0)
    #define PCI_MSIX_HDR_CAPNEXT      (1)
    #define PCI_MSIX_HDR_MSGCTL       (2)
    #define PCI_MSIX_HDR_TABLE        (4)
    #define PCI_MSIX_HDR_PBA          (8)

    #define PCI_MSIX_MSGCTL_ENABLE         (1 << 15)
    #define PCI_MSIX_MSGCTL_MASK           (1 << 14)
    #define PCI_MSIX_INTR_MASK             (1 << 0)

    #define PCI_MSIX_INTR_BASE   66

    #define pci_extract_bus(x)  ((uint8_t)(x >> 16))
    #define pci_extract_slot(x) ((uint8_t)(x >> 8))
    #define pci_extract_func(x) ((uint8_t)(x))


    #define pci_get_addr(x, y) (0x80000000 | (pci_extract_bus(x) << 16) | (pci_extract_slot(x) << 11) | (pci_extract_func(x) << 8) | ((y) & 0xFC))


    #define pci_box_device(x, y, z) ((uint32_t)((x << 16) | (y << 8) | z))


    #define pci_is_64bit(d, f) !!(pci_read((d), (f), 4) & 4)



typedef void (*pci_func_t)(uint32_t device, uint16_t vendor_id, uint16_t device_id, void* extra);

__BEGIN_DECLS



typedef struct pci_msi_row {

    struct {
        uint8_t pci_capid;
        uint8_t pci_capnext;

        union {
            struct {
                uint16_t pci_msgctl_enabled               : 1;
                uint16_t pci_msgctl_multiple_msg_capable  : 3;
                uint16_t pci_msgctl_multiple_msg_enable   : 3;
                uint16_t pci_msgctl_64bit_address         : 1;
                uint16_t pci_msgctl_reserved              : 8;
            };
            uint16_t pci_msgctl;
        };

    } msi_header;

    struct {
        union {
            uint64_t pci_msgaddr64;
            uint32_t pci_msgaddr32;
        };

        uint16_t pci_msgdata;
    } msi_data;

} __packed pci_msi_row_t;

typedef struct pci_msi {
    uintptr_t msi_cap;
} __packed pci_msi_t;

typedef struct pci_msix_row {
    volatile uint32_t pr_address;
    volatile uint32_t pr_address64;
    volatile uint32_t pr_data;
    volatile uint32_t pr_ctl;
} __packed pci_msix_row_t;

typedef struct pci_msix_pba {
    volatile uint32_t pb_bitmap[1];
} __packed pci_msix_pba_t;

typedef struct pci_msix {

    struct {

        uint8_t pci_capid;
        uint8_t pci_capnext;

        union {
            struct {
                uint16_t pci_msgctl_table_size : 10;
                uint16_t pci_msgctl_reserved   : 3;
                uint16_t pci_msgctl_mask       : 1;
                uint16_t pci_msgctl_enable     : 1;
            };
            uint16_t pci_msgctl;
        };

        struct {
            uint32_t pci_bir    : 3;
            uint32_t pci_offset : 28;
        };

        struct {
            uint32_t pci_pending_bir    : 3;
            uint32_t pci_pending_offset : 28;
        };

    } msix_pci;

    uintptr_t msix_cap;
    pci_msix_row_t volatile* msix_rows;
    pci_msix_pba_t volatile* msix_pba;

} __packed pci_msix_t;



typedef void* pci_irq_data_t;
typedef void (*pci_irq_handler_t)(pcidev_t, irq_t, pci_irq_data_t, uint16_t);



/* Platform dependents */
uint64_t pci_read(pcidev_t, int, size_t);
void pci_write(pcidev_t, int, size_t, uint64_t);

/* Scan */
void pci_scan(pci_func_t, int, void*);
void pci_enable_bus_mastering(pcidev_t);
void pci_enable_pio(pcidev_t);
void pci_enable_mmio(pcidev_t);

/* Utils */
uintptr_t pci_find_capabilities(pcidev_t);
uintptr_t pci_bar_size(pcidev_t, int, size_t);
void pci_memcpy(pcidev_t, void*, uintptr_t, size_t);

/* MSI */
int pci_find_msi(pcidev_t, pci_msi_t*);
void pci_msi_enable(pcidev_t, pci_msi_t*);
void pci_msi_disable(pcidev_t, pci_msi_t*);
bool pci_msi_is_enabled(pcidev_t, pci_msi_t*);
int pci_msi_map_irq(pcidev_t, pci_msi_t*, pci_irq_handler_t, pci_irq_data_t);
int pci_msi_unmap_irq(pcidev_t, pci_msi_t*);

/* MSI-X */
int pci_find_msix(pcidev_t, pci_msix_t*);
void pci_msix_enable(pcidev_t, pci_msix_t*);
void pci_msix_disable(pcidev_t, pci_msix_t*);
bool pci_msix_is_enabled(pcidev_t, pci_msix_t*);
void pci_msix_mask(pcidev_t, pci_msix_t*, uint32_t);
void pci_msix_unmask(pcidev_t, pci_msix_t*, uint32_t);
int pci_msix_map_irq(pcidev_t, pci_msix_t*, pci_irq_handler_t, pci_irq_data_t, uint16_t);
int pci_msix_unmap_irq(pcidev_t, pci_msix_t*);

/* INTx */
int pci_intx_map_irq(pcidev_t, irq_t, pci_irq_handler_t, pci_irq_data_t);
int pci_intx_unmap_irq(pcidev_t);
void pci_intx_mask(pcidev_t);
void pci_intx_unmask(pcidev_t);

/* Devices */
uint16_t pci_dev_register(pcidev_t, pci_irq_handler_t, pci_irq_data_t, uint16_t);
uint16_t pci_dev_unregister(pcidev_t);
pcidev_t pci_dev_get_device(uint16_t);
int pci_dev_call_handler(uint16_t index, irq_t irq);

__END_DECLS

#endif
#endif
