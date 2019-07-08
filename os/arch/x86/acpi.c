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


#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/mm.h>
#include <arch/x86/mm.h>
#include <arch/x86/acpi.h>
#include <arch/x86/cpu.h>
#include <string.h>

#define RSDT_LOCATION_START         (CONFIG_KERNEL_BASE + 0xE0000)
#define RSDT_LOCATION_END           (CONFIG_KERNEL_BASE + 0xFFFFF)

static acpi_sdt_t* RSDT;
static int extended;

static int acpi_cksum(void* p, size_t s) {
    
    char sum = 0;
    for(int i = 0; i < s; i++) {
        sum += ((char*) p)[i];
    }

    return (sum == 0);
}


static int find_rsdt(acpi_sdt_t** rsdt, int* extended) {

    uintptr_t p;
    for(p = RSDT_LOCATION_START; p < RSDT_LOCATION_END; p += 16) {

        if(strncmp((const char*) p, "RSD PTR ", 8) != 0)
            continue;

        if(!acpi_cksum((void*) p, 20))
            continue;


        acpi_rsdp_t* rsdp = (acpi_rsdp_t*) p;

        uintptr_t address;
        if(!rsdp->revision)
            address = (uintptr_t) rsdp->address;
        else
            address = (uintptr_t) rsdp->xaddress;

        DEBUG_ASSERT(address);


        pmm_claim (
            address, 
            address + PAGE_SIZE - 1
        );

        x86_map_page (
            (x86_page_t*) (x86_get_cr3() + CONFIG_KERNEL_BASE),
            (address & ~(PAGE_SIZE - 1)), 
            (address & ~(PAGE_SIZE - 1)) >> 12,
            X86_MMU_PG_P | X86_MMU_PG_RW | X86_MMU_PG_CD
        );


        *rsdt = (acpi_sdt_t*) address;
        *extended = !!rsdp->revision;
        
        return 0;
    }

    return -1;
}



int acpi_find(acpi_sdt_t** sdt, const char name[4]) {
    if(unlikely(!RSDT))
        return -1;

    int i;
    for(i = 0; i < ((RSDT->length - sizeof(RSDT)) / (extended ? 8 : 4)); i++) {

        acpi_sdt_t* tmp;
        if(unlikely(extended))
            tmp = (acpi_sdt_t*) ((uintptr_t) RSDT->xtables[i]);
        else
            tmp = (acpi_sdt_t*) ((uintptr_t) RSDT->tables[i]);


        DEBUG_ASSERT(tmp);
        DEBUG_ASSERT(acpi_cksum(tmp, tmp->length));

        if(strncmp(tmp->magic, name, 4) != 0)
            continue;

        *sdt = tmp;
        return 0;
    }

    return -1;
}


int acpi_is_extended(void) {
    return extended;
}

void acpi_init(void) {

    RSDT = NULL;
    extended = 0;

    if(find_rsdt(&RSDT, &extended) != 0)
        kpanic("x86-acpi: Root System Descriptor not found, ACPI not supported!");
    
    acpi_sdt_t* facp;
    if(acpi_find(&facp, "FACP") != 0)
        kpanic("x86-acpi: Fixed ACPI Descriptor not found, fallback to APM");



    acpi_fadt_t* fadt;

    if(unlikely(extended))
        fadt = (acpi_fadt_t*) &facp->xtables;
    else
        fadt = (acpi_fadt_t*) &facp->tables;

    DEBUG_ASSERT(fadt);


    if(
        (fadt->smi_command) &&
        (fadt->acpi_enable & fadt->acpi_disable) &&
        !(inw(fadt->pm1a_control_block) & 1)
    ) {
        outb(fadt->smi_command, fadt->acpi_enable);

        kprintf("x86-acpi: Starting ACPI-A...\n");
        while((inw(fadt->pm1a_control_block) & 1) == 0)
            x86_pause();
        
        kprintf("x86-acpi: Starting ACPI-B...\n");
        while((inw(fadt->pm1b_control_block) & 1) == 0)
            x86_pause();
        
    }


    kprintf("x86-acpi: Switching to ACPI complete [intr(%d), pwr(%d), ext(%d)]\n", fadt->sci_interrupt, fadt->pwrmode, extended);
}
