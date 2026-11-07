/*
 * GPL3 License
 *
 * Author(s):
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 *
 * Copyright (c) 2013-2019 Antonino Natale
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

#include <stdint.h>
#include <string.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/hal.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>
#include <aplus/errno.h>

#include <arch/x86/acpi.h>
#include <arch/x86/asm.h>
#include <arch/x86/cpu.h>

#define RSDT_LOCATION_START arch_vmm_p2v(0xE0000, ARCH_VMM_AREA_HEAP)
#define RSDT_LOCATION_END   arch_vmm_p2v(0xFFFFF, ARCH_VMM_AREA_HEAP)


static acpi_sdt_t* RSDT;
static int extended;


static int acpi_cksum(const char* p, size_t s) {

    char sum = 0;
    for (int i = 0; i < s; i++)
        sum += p[i];

    return (sum == 0);
}


static int acpi_find_rsdp() {

    acpi_rsdp_t* rsdp = NULL;
    if (core->acpi.rsdp_address) {

        if (memcmp((const void*)core->acpi.rsdp_address, "RSD PTR ", 8) != 0)
            goto search;

        if (!acpi_cksum((const char*)core->acpi.rsdp_address, 20))
            goto search;

        rsdp = (acpi_rsdp_t*)core->acpi.rsdp_address;
    }

search:

    if (rsdp == NULL) {

        for (uintptr_t p = RSDT_LOCATION_START; p < RSDT_LOCATION_END; p += 16) {

            if (memcmp((const void*)p, "RSD PTR ", 8) != 0)
                continue;

            if (!acpi_cksum((const char*)p, 20))
                continue;


            rsdp = (acpi_rsdp_t*)p;
            break;
        }
    }

    if (rsdp == NULL) {
        return -1;
    }

    uintptr_t address;
    if (!rsdp->revision) {
        address = (uintptr_t)rsdp->address;
    } else {
        address = (uintptr_t)rsdp->xaddress;
    }

    DEBUG_ASSERT(address);

    RSDT = (acpi_sdt_t*)arch_vmm_p2v(address, ARCH_VMM_AREA_HEAP);

    address &= ~(PML1_PAGESIZE - 1);

    if (address < ((core->memory.phys_upper + core->memory.phys_lower) * 1024)) {
        pmm_claim_area(address, X86_ACPI_AREA_SIZE);
    }

#if DEBUG_LEVEL_TRACE
    kprintf("x86-acpi: RSDT found at %p\n", RSDT);
#endif

    return !!rsdp->revision;
}


static int acpi_gas_size(acpi_generic_address_t* gas, size_t* size) {

    DEBUG_ASSERT(gas);
    DEBUG_ASSERT(size);

    switch (gas->size) {
        case X86_ACPI_GAS_SIZE_BYTE:
            return *size = 1, 0;
        case X86_ACPI_GAS_SIZE_WORD:
            return *size = 2, 0;
        case X86_ACPI_GAS_SIZE_DWORD:
            return *size = 4, 0;
        case X86_ACPI_GAS_SIZE_QWORD:
            return *size = 8, 0;
        case X86_ACPI_GAS_SIZE_UNDEFINED:
            break;
        default:
            return errno = ENOTSUP, -1;
    }

    switch (gas->bit_width) {
        case 8:
            return *size = 1, 0;
        case 16:
            return *size = 2, 0;
        case 32:
            return *size = 4, 0;
        case 64:
            return *size = 8, 0;
        default:
            return errno = ENOTSUP, -1;
    }
}


static int acpi_gas_mmio(acpi_generic_address_t* gas, size_t size, uintptr_t* address) {

    DEBUG_ASSERT(gas);
    DEBUG_ASSERT(address);

    if ((gas->address & (size - 1)) != 0) {
        return errno = ENOTSUP, -1;
    }

    uintptr_t phys = gas->address & ~(PML1_PAGESIZE - 1);
    uintptr_t off  = gas->address & (PML1_PAGESIZE - 1);

    if (off + size > PML1_PAGESIZE) {
        return errno = ENOTSUP, -1;
    }

    uintptr_t virt = arch_vmm_map(&core->bsp.address_space, phys + KERNEL_MMIO_AREA, phys, PML1_PAGESIZE, ARCH_VMM_MAP_RDWR | ARCH_VMM_MAP_UNCACHED | ARCH_VMM_MAP_NOEXEC | ARCH_VMM_MAP_FIXED);

    if (virt == ARCH_VMM_MAP_FAILED) {
        return errno = EIO, -1;
    }

    return *address = virt + off, 0;
}


static int acpi_gas_pci_select(acpi_generic_address_t* gas, size_t size, uint16_t* offset) {

    DEBUG_ASSERT(gas);
    DEBUG_ASSERT(offset);

    uint16_t device   = X86_ACPI_GAS_PCI_DEVICE(gas->address);
    uint16_t function = X86_ACPI_GAS_PCI_FUNCTION(gas->address);
    uint16_t reg      = X86_ACPI_GAS_PCI_OFFSET(gas->address);

    if (device > 31 || function > 7 || reg > 255) {
        return errno = ENOTSUP, -1;
    }

    if (size > 4 || (reg & (size - 1)) != 0) {
        return errno = ENOTSUP, -1;
    }

    outl(X86_PCI_CONFIG_ADDRESS, 0x80000000U | ((uint32_t)device << 11) | ((uint32_t)function << 8) | (uint32_t)(reg & 0xFC));

    return *offset = reg, 0;
}


static int acpi_gas_check(acpi_generic_address_t* gas, size_t* size) {

    DEBUG_ASSERT(gas);
    DEBUG_ASSERT(size);

    if (!gas->address) {
        return errno = EINVAL, -1;
    }

    if (gas->bit_offset != 0) {
        return errno = ENOTSUP, -1;
    }

    return acpi_gas_size(gas, size);
}


static int acpi_gas_write(acpi_generic_address_t* gas, uint64_t value) {

    DEBUG_ASSERT(gas);

    size_t size;
    if (acpi_gas_check(gas, &size) < 0) {
        return -1;
    }

    switch (gas->type) {

        case X86_ACPI_GAS_TYPE_IO: {

            if (gas->address > 0xFFFF) {
                return errno = ENOTSUP, -1;
            }

            switch (size) {
                case 1:
                    outb(gas->address, value & 0xFF);
                    break;
                case 2:
                    outw(gas->address, value & 0xFFFF);
                    break;
                case 4:
                    outl(gas->address, value & 0xFFFFFFFF);
                    break;
                default:
                    return errno = ENOTSUP, -1;
            }

            break;
        }

        case X86_ACPI_GAS_TYPE_MEMORY: {

            uintptr_t address;
            if (acpi_gas_mmio(gas, size, &address) < 0) {
                return -1;
            }

            switch (size) {
                case 1:
                    mmio_w8(address, value & 0xFF);
                    break;
                case 2:
                    mmio_w16(address, value & 0xFFFF);
                    break;
                case 4:
                    mmio_w32(address, value & 0xFFFFFFFF);
                    break;
                case 8:
                    mmio_w64(address, value);
                    break;
                default:
                    return errno = ENOTSUP, -1;
            }

            break;
        }

        case X86_ACPI_GAS_TYPE_PCI_CONFIG: {

            uint16_t offset;
            if (acpi_gas_pci_select(gas, size, &offset) < 0) {
                return -1;
            }

            uint32_t shift = (offset & 3) * 8;
            uint32_t mask;

            switch (size) {
                case 1:
                    mask = 0xFFU << shift;
                    break;
                case 2:
                    mask = 0xFFFFU << shift;
                    break;
                case 4:
                    mask = 0xFFFFFFFFU;
                    break;
                default:
                    return errno = ENOTSUP, -1;
            }

            uint32_t tmp = 0;
            if (mask != 0xFFFFFFFFU) {
                tmp = inl(X86_PCI_CONFIG_DATA) & ~mask;
            }

            outl(X86_PCI_CONFIG_DATA, tmp | ((((uint32_t)value) << shift) & mask));
            break;
        }

        default:
            return errno = ENOTSUP, -1;
    }

    return 0;
}


static int acpi_gas_read(acpi_generic_address_t* gas, uint64_t* value) {

    DEBUG_ASSERT(gas);
    DEBUG_ASSERT(value);

    size_t size;
    if (acpi_gas_check(gas, &size) < 0) {
        return -1;
    }

    switch (gas->type) {

        case X86_ACPI_GAS_TYPE_IO: {

            if (gas->address > 0xFFFF) {
                return errno = ENOTSUP, -1;
            }

            switch (size) {
                case 1:
                    *value = inb(gas->address);
                    break;
                case 2:
                    *value = inw(gas->address);
                    break;
                case 4:
                    *value = inl(gas->address);
                    break;
                default:
                    return errno = ENOTSUP, -1;
            }

            break;
        }

        case X86_ACPI_GAS_TYPE_MEMORY: {

            uintptr_t address;
            if (acpi_gas_mmio(gas, size, &address) < 0) {
                return -1;
            }

            switch (size) {
                case 1:
                    *value = mmio_r8(address);
                    break;
                case 2:
                    *value = mmio_r16(address);
                    break;
                case 4:
                    *value = mmio_r32(address);
                    break;
                case 8:
                    *value = mmio_r64(address);
                    break;
                default:
                    return errno = ENOTSUP, -1;
            }

            break;
        }

        case X86_ACPI_GAS_TYPE_PCI_CONFIG: {

            uint16_t offset;
            if (acpi_gas_pci_select(gas, size, &offset) < 0) {
                return -1;
            }

            uint32_t tmp   = inl(X86_PCI_CONFIG_DATA);
            uint32_t shift = (offset & 3) * 8;

            switch (size) {
                case 1:
                    *value = (tmp >> shift) & 0xFF;
                    break;
                case 2:
                    *value = (tmp >> shift) & 0xFFFF;
                    break;
                case 4:
                    *value = tmp;
                    break;
                default:
                    return errno = ENOTSUP, -1;
            }

            break;
        }

        default:
            return errno = ENOTSUP, -1;
    }

    return 0;
}



int acpi_is_extended() {
    return extended;
}


int acpi_find(acpi_sdt_t** sdt, const char name[4]) {

    DEBUG_ASSERT(RSDT);
    DEBUG_ASSERT(name);

    for (size_t i = 0; i < ((RSDT->length - sizeof(*RSDT)) / (extended ? 8 : 4)); i++) {

        uintptr_t address;
        if (unlikely(extended))
            address = (uintptr_t)RSDT->xtables[i];
        else
            address = (uintptr_t)RSDT->tables[i];

        DEBUG_ASSERT(address);


        acpi_sdt_t* tmp = (acpi_sdt_t*)arch_vmm_p2v(address, ARCH_VMM_AREA_HEAP);

#if DEBUG_LEVEL_WARN
        if(!acpi_cksum((const char*)tmp, tmp->length)) {
            kprintf("x86-acpi: WARN! table %.4s as wrong checksum", name);
        }
#endif

        if (memcmp(tmp->magic, name, 4) != 0)
            continue;

#if DEBUG_LEVEL_TRACE
        kprintf("x86-acpi: %.4s found at 0x%lX\n", name, address);
#endif

        if (sdt) {
            *sdt = tmp;
        }

        return 0;
    }

#if DEBUG_LEVEL_WARN
    kprintf("x86-acpi: %.4s not found\n", name);
#endif

    return errno = ENOENT, -1;
}


int acpi_find_dsdt(acpi_fadt_t* fadt, acpi_sdt_t** dsdt) {

    uintptr_t address;
    if (fadt->x_dsdt) {
        address = (uintptr_t) fadt->x_dsdt;
    } else if (fadt->dsdt) {
        address = (uintptr_t) fadt->dsdt;
    } else {
#if DEBUG_LEVEL_WARN
        kprintf("x86-acpi: WARN! DSDT pointer is NULL on FADT table\n");
#endif
        errno = EINVAL;
        return -1;
    }

    acpi_sdt_t* sdt = (acpi_sdt_t*) arch_vmm_p2v(address, ARCH_VMM_AREA_HEAP);

#if DEBUG_LEVEL_WARN
    if(!acpi_cksum((const char*)sdt, sdt->length)) {
        kprintf("x86-acpi: WARN! table DSDT as wrong checksum");
    }
#endif

#if DEBUG_LEVEL_TRACE
        kprintf("x86-acpi: DSDT found at 0x%lX\n", address);
#endif

    if (dsdt) {
        *dsdt = sdt;
    }

    return 0;
}


void acpi_init(void) {

    acpi_sdt_t* facp  = NULL;
    acpi_fadt_t* fadt = NULL;

    RSDT     = NULL;
    extended = 0;

    if ((extended = acpi_find_rsdp()) == -1)
        kpanicf("x86-acpi: Root System Descriptor Pointer not found, ACPI not supported!");

    if (acpi_find(&facp, "FACP") != 0)
        kpanicf("x86-acpi: Fixed ACPI Descriptor not found, ACPI not supported!");

    fadt = (acpi_fadt_t*)&facp->data;
    DEBUG_ASSERT(fadt);

    if ((fadt->smi_command) && (fadt->acpi_enable & fadt->acpi_disable) && !(inw(fadt->pm1a_control_block) & 1)) {

        outb(fadt->smi_command, fadt->acpi_enable);

#if DEBUG_LEVEL_TRACE
        kprintf("x86-acpi: Starting ACPI-A...\n");
#endif

        while ((inw(fadt->pm1a_control_block) & 1) == 0)
            __builtin_ia32_pause();

#if DEBUG_LEVEL_TRACE
        kprintf("x86-acpi: Starting ACPI-B...\n");
#endif

        while (fadt->pm1b_control_block && (inw(fadt->pm1b_control_block) & 1) == 0)
            __builtin_ia32_pause();
    }

#if DEBUG_LEVEL_INFO
    kprintf("x86-acpi: Switching to ACPI complete [base(%p), intr(%d), pwr(%d), ext(%d)]\n", RSDT, fadt->sci_interrupt, fadt->pwrmode, extended);
#endif
}


int acpi_shutdown(void) {

    acpi_sdt_t* facp = NULL;
    if (acpi_find(&facp, "FACP") != 0) {
        return -1;
    }

    acpi_fadt_t* fadt = NULL;
    fadt = (acpi_fadt_t*)&facp->data;

    acpi_sdt_t* dsdt;
    if (acpi_find_dsdt(fadt, &dsdt) != 0) {
        return -1;
    }

    uintptr_t s5_offset = sizeof(*dsdt);
    uint8_t* s5_address = 0;

    for (size_t i = 0; i < (dsdt->length - sizeof(*dsdt) - 4); i++, s5_offset++) {
        if (memcmp(&((uint8_t*)dsdt)[s5_offset], "_S5_", 4) != 0)
            continue;

        s5_address = &((uint8_t*)dsdt)[s5_offset];
        break;
    }

    if (unlikely(!s5_address)) {
#if DEBUG_LEVEL_ERROR
        kprintf("x86-acpi: ERROR! _S5_ not found in DSDT, ACPI shutdown not supported!");
#endif
        return errno = ENOSYS, -1;
    }

    if ((s5_address[-1] != 0x08) && (s5_address[-2] != 0x08 || s5_address[-1] != '\\')) {
#if DEBUG_LEVEL_ERROR
        kprintf("x86-acpi: ERROR! _S5_ is not located in the proper location, ACPI shutdown not supported!");
#endif
        return errno = ENOSYS, -1;
    }

    if (s5_address[4] != 0x12) {
#if DEBUG_LEVEL_ERROR
        kprintf("x86-acpi: ERROR! _S5_ has invalid syntax, ACPI shutdown not supported!");
#endif
        return errno = ENOSYS, -1;
    }

    uint64_t SLP_TYPa = 0;
    uint64_t SLP_TYPb = 0;

    s5_address += 5;
    s5_address += (*s5_address & 0xC0) >> 6;
    s5_address += 2;

    if (*s5_address == 0x0A) {
        s5_address++;
    }
    SLP_TYPa = *s5_address++ << 10;

    if (*s5_address == 0x0A) {
        s5_address++;
    }
    SLP_TYPb = *s5_address++ << 10;

    if (facp->length < offsetof(acpi_fadt_t, __x_pm1_control_block_supported) + sizeof(acpi_sdt_t)) {
        if (fadt->pm1a_control_block) {
            outw(fadt->pm1a_control_block, inw(fadt->pm1a_control_block) | SLP_TYPa | (1 << 13));
        }
        if (fadt->pm1b_control_block) {
            outw(fadt->pm1b_control_block, inw(fadt->pm1b_control_block) | SLP_TYPb | (1 << 13));
        }
    } else {
        uint64_t x_pm1a_control_block_value;
        if (acpi_gas_read(&fadt->x_pm1a_control_block, &x_pm1a_control_block_value) == 0) {
            acpi_gas_write(&fadt->x_pm1a_control_block, x_pm1a_control_block_value | SLP_TYPa | (1 << 13));
        }
        uint64_t x_pm1b_control_block_value;
        if (acpi_gas_read(&fadt->x_pm1b_control_block, &x_pm1b_control_block_value) == 0) {
            acpi_gas_write(&fadt->x_pm1b_control_block, x_pm1b_control_block_value | SLP_TYPb | (1 << 13));
        }
    }

    return 0;
}


int acpi_reboot(void) {

    acpi_sdt_t* facp = NULL;
    if (acpi_find(&facp, "FACP") != 0) {
        return -1;
    }

    acpi_fadt_t* fadt = NULL;
    fadt = (acpi_fadt_t*)&facp->data;

    if ((fadt->flags & (1 << 10)) == 0) {
#if DEBUG_LEVEL_WARN
        kprintf("x86-acpi: WARN! RESET_REG_SUP is clear, ACPI reboot not supported!\n");
#endif
        return errno = ENOSYS, -1;
    }

    if (facp->length < offsetof(acpi_fadt_t, __reset_reg_supported) + sizeof(acpi_sdt_t)) {
#if DEBUG_LEVEL_WARN
        kprintf("x86-acpi: WARN! FACP length is too short for reset register, ACPI reboot not supported!\n");
#endif
        return errno = ENOSYS, -1;
    }

    if (acpi_gas_write(&fadt->reset_reg, fadt->reset_value) != 0) {
#if DEBUG_LEVEL_WARN
        kprintf("x86-acpi: WARN! failed to write reset register, ACPI reboot not supported!\n");
#endif
        return errno = ENOSYS, -1;
    }

    return 0;
}
