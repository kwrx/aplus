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

#ifndef _DEV_PCI_H
#define _DEV_PCI_H

#ifndef __ASSEMBLY__


#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/syscall.h>
#include <stdint.h>

#include <dev/interface.h>



typedef uint32_t pcidev_t;



#define PCI_VENDOR_ID                   0x00
#define PCI_DEVICE_ID                   0x02
#define PCI_COMMAND                     0x04
#define PCI_STATUS                      0x06
#define PCI_REVISION_ID                 0x08


#define PCI_PROG_IF                     0x09
#define PCI_SUBCLASS                    0x0A
#define PCI_CLASS                       0x0B
#define PCI_CACHE_LINE_SIZE             0x0C
#define PCI_LATENCY_TIMER               0x0D
#define PCI_HEADER_TYPE                 0x0E
#define PCI_BIST                        0x0F

#define PCI_BAR0                        0x10
#define PCI_BAR1                        0x14
#define PCI_BAR2                        0x18
#define PCI_BAR3                        0x1C
#define PCI_BAR4                        0x20
#define PCI_BAR5                        0x24
#define PCI_BAR_MM_MASK                 (0xFFFFFFF0)
#define PCI_BAR_IO_MASK                 (0xFFFFFFFC)

#define PCI_SUBSYSID                    0x2C
#define PCI_SUBVENID                    0x2E

#define PCI_INTERRUPT_PIN               0x3D
#define PCI_INTERRUPT_LINE              0x3C
#define PCI_SECONDARY_BUS               0x09

#define PCI_CAPABILITIES                0x34


#define PCI_HEADER_TYPE_DEVICE          0
#define PCI_HEADER_TYPE_BRIDGE          1
#define PCI_HEADER_TYPE_CARDBUS         2

#define PCI_TYPE_ALL                    -1
#define PCI_TYPE_BRIDGE                 0x0604
#define PCI_TYPE_SATA                   0x0106
#define PCI_TYPE_VGA                    0x0300

#define PCI_ADDRESS_PORT                0xCF8
#define PCI_VALUE_PORT                  0xCFC

#define PCI_NONE                        0xFFFF


#define PCI_COMMAND_REG_PIO             (1 << 0)
#define PCI_COMMAND_REG_MMIO            (1 << 1)
#define PCI_COMMAND_REG_BUS_MASTERING   (1 << 2)
#define PCI_COMMAND_REG_SPECIAL_CYCLES  (1 << 3)
#define PCI_COMMAND_REG_RDWR_INVL       (1 << 4)
#define PCI_COMMAND_REG_INTR_DISABLE    (1 << 10)


#define PCI_STATUS_REG_INTERRUPT        (1 << 3)
#define PCI_STATUS_REG_CAPABILITIES     (1 << 4)

#define PCI_MSIX_CAPID                  (0x11)
#define PCI_MSIX_ENABLE                 (1 << 15)
#define PCI_MSIX_INTR_MASK              (1 << 0)




#define pci_extract_bus(x)      \
    ((uint8_t) (x >> 16))
#define pci_extract_slot(x)     \
    ((uint8_t) (x >> 8))
#define pci_extract_func(x)     \
    ((uint8_t) (x))


#define pci_get_addr(x, y)              \
    (0x80000000                     |   \
    (pci_extract_bus(x) << 16)      |   \
    (pci_extract_slot(x) << 11)     |   \
    (pci_extract_func(x) << 8)      |   \
    ((field) & 0xFC))


#define pci_box_device(x, y, z)             \
    ((uint32_t) ((x << 16) | (y << 8) | z))


#define pci_is_64bit(d, f)              \
    (pci_read(d, f, 4) & 4)




typedef void (*pci_func_t)(uint32_t device, uint16_t vendor_id, uint16_t device_id, void * extra);

__BEGIN_DECLS



typedef struct pci_msix_row {

    volatile uint64_t pr_address;
    volatile uint32_t pr_data;
    volatile uint32_t pr_ctl;

} __packed pci_msix_row_t;

typedef struct pci_msix {
    
    struct {

        uint8_t pci_capid;
        uint8_t pci_capnext;

        union {
            struct {
                uint16_t pci_msgctl_table_size : 10;
                uint16_t pci_msgctl_reserved : 3;
                uint16_t pci_msgctl_mask : 1;
                uint16_t pci_msgctl_enable : 1;
            };
            uint16_t pci_msgctl;
        };

        struct {
            uint32_t pci_bir : 3;
            uint32_t pci_offset : 28;
        };

        struct {
            uint32_t pci_pending_bir : 3;
            uint32_t pci_pending_offset : 28;
        };

    } msix_pci;

    uintptr_t msix_cap;
    pci_msix_row_t volatile* msix_rows;

} __packed pci_msix_t;






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

/* MSI-X */
int pci_find_msix(pcidev_t, pci_msix_t*);
void pci_msix_enable(pcidev_t, pci_msix_t*);
void pci_msix_disable(pcidev_t, pci_msix_t*);
void pci_msix_mask(pcidev_t, pci_msix_t*, uint32_t);
void pci_msix_unmask(pcidev_t, pci_msix_t*, uint32_t);
void pci_msix_map(pcidev_t, pci_msix_t*, uint32_t, void*, void*);


__END_DECLS

#endif
#endif