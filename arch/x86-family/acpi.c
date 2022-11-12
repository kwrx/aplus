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
#include <aplus/multiboot.h>
#include <aplus/debug.h>
#include <aplus/memory.h>
#include <aplus/ipc.h>
#include <aplus/hal.h>

#include <arch/x86/cpu.h>
#include <arch/x86/asm.h>
#include <arch/x86/intr.h>
#include <arch/x86/acpi.h>




#define RSDT_LOCATION_START         arch_vmm_p2v(0xE0000, ARCH_VMM_AREA_HEAP)
#define RSDT_LOCATION_END           arch_vmm_p2v(0xFFFFF, ARCH_VMM_AREA_HEAP)


/*!
 * @brief RSDT.
 *        Root System Description Table.
 * 
 * RSDT is a data structure used in the ACPI programming interface.
 * This table contains pointers to all the other System Description Tables.
 */
static acpi_sdt_t* RSDT;

/*!
 * @brief Extended.
 *        ACPI >= 2.0.
 *
 * Check if ACPI Version is greater or equal than 2.0
 */
static int extended;



/*!
 * @brief acpi_cksum().
 *        Validate ACPI Table data calculating checksum
 * 
 * @param p: Pointer of data
 * @param s: Size of data
 */
static int acpi_cksum(const char* p, size_t s) {

    char sum = 0;
    for(int i = 0; i < s; i++)
        sum += p[i];

    return (sum == 0);

}


/*!
 * @brief acpi_find_rsdp().
 *        Locate and check RSDP in EBDA (Extended BIOS Data Area).
 */
static int acpi_find_rsdp() {


    acpi_rsdp_t* rsdp = NULL;


    if(core->acpi.rsdp_address) {

        if(memcmp((const void*) core->acpi.rsdp_address, "RSD PTR ", 8) != 0)
            goto search;

        if(!acpi_cksum((const char*) core->acpi.rsdp_address, 20))
            goto search;

        rsdp = (acpi_rsdp_t*) core->acpi.rsdp_address;

    }


search:

    if(rsdp == NULL) {

        for(uintptr_t p = RSDT_LOCATION_START; p < RSDT_LOCATION_END; p += 16) {

            if(memcmp((const void*) p, "RSD PTR ", 8) != 0)
                continue;

            if(!acpi_cksum((const char*) p, 20))
                continue;


            rsdp = (acpi_rsdp_t*) p;
            break;

        }

    }


    if(rsdp == NULL) {
        return -1;
    }



    uintptr_t address;

    if(!rsdp->revision) {
        address = (uintptr_t) rsdp->address;
    } else {
        address = (uintptr_t) rsdp->xaddress;
    }

    DEBUG_ASSERT(address);



    RSDT = (acpi_sdt_t*) arch_vmm_p2v(address, ARCH_VMM_AREA_HEAP);


    address &= ~(PML1_PAGESIZE - 1);

    if(address < ((core->memory.phys_upper + core->memory.phys_lower) * 1024)) {
        
        pmm_claim_area (
            address, X86_ACPI_AREA_SIZE
        );

    }
    

#if DEBUG_LEVEL_TRACE
    kprintf("x86-acpi: RSDT found at %p\n", RSDT);
#endif

    return !!rsdp->revision;

}


/*!
 * @brief acpi_is_extended().
 *        Return 1 when ACPI Version is greater or equal than 2.0, 0 otherwise.
 */
int acpi_is_extended() {
    return extended;
}



/*!
 * @brief acpi_find().
 *        Locate and check ACPI Tables in ACPI Memory Area.
 *
 * @param sdt: Table's Pointer to return when success.
 * @param name: Table's Name to find.
 */
int acpi_find(acpi_sdt_t** sdt, const char name[4]) {
    
    DEBUG_ASSERT(RSDT);
    DEBUG_ASSERT(sdt);
    DEBUG_ASSERT(name);

    
    for(size_t i = 0; i < ((RSDT->length - sizeof(*RSDT)) / (extended ? 8 : 4)); i++) {

        uintptr_t address;
        if(unlikely(extended))
            address = (uintptr_t) RSDT->xtables[i];
        else
            address = (uintptr_t) RSDT->tables[i];

        DEBUG_ASSERT(address);


        acpi_sdt_t* tmp = (acpi_sdt_t*) arch_vmm_p2v(address, ARCH_VMM_AREA_HEAP);

        //! Check if it's a valid ACPI table
        PANIC_ON(acpi_cksum((const char*) tmp, tmp->length));
               
        if(memcmp(tmp->magic, name, 4) != 0)
            continue;


#if DEBUG_LEVEL_TRACE
        kprintf("x86-acpi: %s found at 0x%lX\n", name, address);
#endif

        *sdt = tmp;
        return 0;
    }

    return -1;
}




/*!
 * @brief acpi_init().
 *        Initialize ACPI (Advanced Configuration and Power Interface)
 *
 * ACPI is a Power Management and configuration standard for the PC.
 * It allows the operating system to control power device, control and/or check
 * thermal zones, battery levels, PCI IRQ routing, CPUs, NUMA domains and many other things.
 */
void acpi_init(void) {


    acpi_sdt_t* facp  = NULL;
    acpi_fadt_t* fadt = NULL;

    
    RSDT = NULL;
    extended = 0;

    if((extended = acpi_find_rsdp()) == -1)
        kpanicf("x86-acpi: Root System Descriptor Pointer not found, ACPI not supported!");
    
    if(acpi_find(&facp, "FACP") != 0)
        kpanicf("x86-acpi: Fixed ACPI Descriptor not found, ACPI not supported!");

    
    
    if(extended)
        fadt = (acpi_fadt_t*) &facp->xtables;
    else
        fadt = (acpi_fadt_t*) &facp->tables;

    DEBUG_ASSERT(fadt);


    if (
        (fadt->smi_command)                         &&
        (fadt->acpi_enable & fadt->acpi_disable)    &&
       !(inw(fadt->pm1a_control_block) & 1)
    ) {


        outb(fadt->smi_command, fadt->acpi_enable);


#if DEBUG_LEVEL_TRACE
        kprintf("x86-acpi: Starting ACPI-A...\n");
#endif

        while((inw(fadt->pm1a_control_block) & 1) == 0)
            __builtin_ia32_pause();


#if DEBUG_LEVEL_TRACE
        kprintf("x86-acpi: Starting ACPI-B...\n");
#endif

        while((inw(fadt->pm1b_control_block) & 1) == 0)
            __builtin_ia32_pause();
            

    }

#if DEBUG_LEVEL_INFO
    kprintf("x86-acpi: Switching to ACPI complete [base(%p), intr(%d), pwr(%d), ext(%d)]\n", RSDT, fadt->sci_interrupt, fadt->pwrmode, extended);
#endif

}


