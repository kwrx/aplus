/*                                                                      
 * GPL3 License                                                         
 *                                                                      
 * Author(s):                                                              
 *      Antonino Natale <antonio.natale97@hotmail.com>                  
 *                                                                      
 *                                                                      
 * Copyright (c) 2013-2019 Antonino Natale                              
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
#include <aplus/multiboot.h>
#include <aplus/debug.h>
#include <aplus/memory.h>
#include <aplus/elf.h>
#include <aplus/hal.h>

#include <arch/x86/cpu.h>
#include <arch/x86/fpu.h>
#include <arch/x86/asm.h>
#include <arch/x86/acpi.h>
#include <arch/x86/apic.h>
#include <arch/x86/intr.h>



/*!
 * @brief bmain().
 *        Boot Entrypoint.
 * 
 * Initialize Hardware and boot services.
 */
void bmain(multiboot_uint32_t magic, struct multiboot_tag* btags) {
    
    arch_debug_init();

    DEBUG_ASSERT(magic == MULTIBOOT2_BOOTLOADER_MAGIC);
    DEBUG_ASSERT(btags);


    //* AP Startup memory area
    core->mmap.ptr[core->mmap.count].address = AP_BOOT_OFFSET;
    core->mmap.ptr[core->mmap.count].length  = PML1_PAGESIZE;
    core->mmap.ptr[core->mmap.count].type    = MULTIBOOT_MEMORY_RESERVED;

    core->mmap.count += 1;


    //* ISA Memory Hole 15-16MB
    core->mmap.ptr[core->mmap.count].address = 0x00E00000;
    core->mmap.ptr[core->mmap.count].length  = 0x00100000;
    core->mmap.ptr[core->mmap.count].type    = MULTIBOOT_MEMORY_RESERVED;

    core->mmap.count += 1;


    //* PCI Devices Memory Area
    core->mmap.ptr[core->mmap.count].address = 0xC0000000;
    core->mmap.ptr[core->mmap.count].length  = 0x40000000;
    core->mmap.ptr[core->mmap.count].type    = MULTIBOOT_MEMORY_RESERVED;

    core->mmap.count += 1;




    //* Initialize Bootstrap CPU
    arch_cpu_init(SMP_CPU_BOOTSTRAP_ID);


    do {
        
        if(btags->type == MULTIBOOT_TAG_TYPE_END) {
            DEBUG_ASSERT(btags->size == 8);
            break;
        }

        switch(btags->type) {

            case MULTIBOOT_TAG_TYPE_CMDLINE:
                strncpy(&core->boot.cmdline[0], ((struct multiboot_tag_string*) btags)->string, CONFIG_BUFSIZ);
                break;

            case MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME:
                strncpy(&core->boot.bootloader[0], ((struct multiboot_tag_string*) btags)->string, CONFIG_BUFSIZ);
                break;

            case MULTIBOOT_TAG_TYPE_MODULE:

                {

                    int i = core->modules.count++;

                    core->modules.ko[i].ptr       = ((struct multiboot_tag_module*) btags)->mod_start;
                    core->modules.ko[i].size      = ((struct multiboot_tag_module*) btags)->mod_end -
                                                    ((struct multiboot_tag_module*) btags)->mod_start;


                    if(strlen(((struct multiboot_tag_module*) btags)->cmdline) > 0) {
                    
                        strncpy(&core->modules.ko[i].cmdline[0], ((struct multiboot_tag_module*) btags)->cmdline, CONFIG_BUFSIZ);
                    
                    } else {
                    
                        strncpy(&core->modules.ko[i].cmdline[0], &core->boot.cmdline[0], CONFIG_BUFSIZ);
                    
                    }

                }
                
                break;

            case MULTIBOOT_TAG_TYPE_BASIC_MEMINFO:

                core->memory.phys_upper = ((struct multiboot_tag_basic_meminfo*) btags)->mem_upper;
                core->memory.phys_lower = ((struct multiboot_tag_basic_meminfo*) btags)->mem_lower;
                break;

            case MULTIBOOT_TAG_TYPE_BOOTDEV:
                break;

            case MULTIBOOT_TAG_TYPE_MMAP:

                {

                    struct multiboot_tag_mmap* mmap = (struct multiboot_tag_mmap*) btags;

                    for(size_t i = 0; i < (mmap->size - 16) / mmap->entry_size; i++) {

                        core->mmap.ptr[core->mmap.count].address = mmap->entries[i].addr;
                        core->mmap.ptr[core->mmap.count].length  = mmap->entries[i].len;
                        core->mmap.ptr[core->mmap.count].type    = mmap->entries[i].type;

                        core->mmap.count += 1;

                    }

                }

                break;

            case MULTIBOOT_TAG_TYPE_VBE:
                break;

            case MULTIBOOT_TAG_TYPE_FRAMEBUFFER:

                if(((struct multiboot_tag_framebuffer*) btags)->common.framebuffer_type != MULTIBOOT_FRAMEBUFFER_TYPE_RGB)
                    break;

                core->framebuffer.width   = ((struct multiboot_tag_framebuffer*) btags)->common.framebuffer_width;
                core->framebuffer.height  = ((struct multiboot_tag_framebuffer*) btags)->common.framebuffer_height;
                core->framebuffer.depth   = ((struct multiboot_tag_framebuffer*) btags)->common.framebuffer_bpp;
                core->framebuffer.pitch   = ((struct multiboot_tag_framebuffer*) btags)->common.framebuffer_pitch;
                core->framebuffer.address = ((struct multiboot_tag_framebuffer*) btags)->common.framebuffer_addr;

                core->framebuffer.red_field_position   = ((struct multiboot_tag_framebuffer*) btags)->framebuffer_red_field_position;
                core->framebuffer.red_mask_size        = ((struct multiboot_tag_framebuffer*) btags)->framebuffer_red_mask_size;
                core->framebuffer.green_field_position = ((struct multiboot_tag_framebuffer*) btags)->framebuffer_green_field_position;
                core->framebuffer.green_mask_size      = ((struct multiboot_tag_framebuffer*) btags)->framebuffer_green_mask_size;
                core->framebuffer.blue_field_position  = ((struct multiboot_tag_framebuffer*) btags)->framebuffer_blue_field_position;
                core->framebuffer.blue_mask_size       = ((struct multiboot_tag_framebuffer*) btags)->framebuffer_blue_mask_size;
            
                break;

            case MULTIBOOT_TAG_TYPE_ELF_SECTIONS:

                core->exe.sh_num        = ((struct multiboot_tag_elf_sections*) btags)->num;
                core->exe.sh_entsize    = ((struct multiboot_tag_elf_sections*) btags)->entsize;
                core->exe.sh_shndx      = ((struct multiboot_tag_elf_sections*) btags)->shndx;

                memcpy(&core->exe.sections[0], &((struct multiboot_tag_elf_sections*) btags)->sections[0], core->exe.sh_entsize * core->exe.sh_num);
                break;

            case MULTIBOOT_TAG_TYPE_APM:
                break;

            case MULTIBOOT_TAG_TYPE_EFI32:
            case MULTIBOOT_TAG_TYPE_EFI64:
                break;

            case MULTIBOOT_TAG_TYPE_SMBIOS:
                break;
            
            case MULTIBOOT_TAG_TYPE_ACPI_OLD:
            case MULTIBOOT_TAG_TYPE_ACPI_NEW:
                break;

            case MULTIBOOT_TAG_TYPE_NETWORK:
                break;
            
            case MULTIBOOT_TAG_TYPE_EFI_MMAP:
            case MULTIBOOT_TAG_TYPE_EFI_BS:
            case MULTIBOOT_TAG_TYPE_EFI32_IH:
            case MULTIBOOT_TAG_TYPE_EFI64_IH:
                break;

            case MULTIBOOT_TAG_TYPE_LOAD_BASE_ADDR:
                core->exe.load_base_address = ((struct multiboot_tag_load_base_addr*) btags)->load_base_addr;
                break;

            default:
                kpanicf("bmain(): PANIC! invalid MULTIBOOT_TAG_TYPE_*: %d\n", btags->type);

        }


        if(btags->size & 7)
            btags->size = (btags->size & ~7) + 8;

        btags = (struct multiboot_tag*) ((uintptr_t) btags + btags->size);
        

    } while(btags);





#if defined(DEBUG) && DEBUG_LEVEL >= 1
    kprintf("boot: %s '%s'\n", core->boot.bootloader, 
                               core->boot.cmdline);
#endif



    //* Map Modules

    int i;
    for(i = 0; i < core->modules.count; i++) {

        core->mmap.ptr[core->mmap.count].address = core->modules.ko[i].ptr;
        core->mmap.ptr[core->mmap.count].length  = core->modules.ko[i].size;
        core->mmap.ptr[core->mmap.count].type    = MULTIBOOT_MEMORY_RESERVED;

        core->mmap.count += 1;

    }



    //* Map ELF Sections

    Elf_Shdr* shdr = (Elf_Shdr*) &core->exe.sections[0];

    for(i = 1; i < core->exe.sh_num; i++) {

        switch(shdr[i].sh_type) {

            case SHT_STRTAB:
            case SHT_SYMTAB:
            
                core->mmap.ptr[core->mmap.count].address = shdr[i].sh_addr;
                core->mmap.ptr[core->mmap.count].length  = shdr[i].sh_size;
                core->mmap.ptr[core->mmap.count].type    = MULTIBOOT_MEMORY_RESERVED;

                core->mmap.count += 1;

        }

    }


    DEBUG_ASSERT(core->mmap.count < (CONFIG_BUFSIZ << 2));


    //* Initialize Physical Memory Manager
    pmm_init((core->memory.phys_upper + core->memory.phys_lower) * 1024);

    //* Spawn BSP INIT Process
    arch_task_spawn_init();

    //* Initialize ACPI
    acpi_init();

    //* Initialize Timer
    timer_init();

    //* Initialize APIC
    apic_init();

}
