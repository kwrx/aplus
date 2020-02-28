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


#include <aplus/core/base.h>
#include <aplus/core/linkage.h>
#include <aplus/core/multiboot.h>
#include <arch/x86/asm.h>



/*!
 * @brief AP Startup Layout
 * 
 *    AP_BOOT_OFFSET:     16bit Entrypoint
 *    +0x100:             32bit Entrypoint
 *    +0x200:             64bit Entrypoint
 *    +0xF00:             GDT32
 *    +0xF18:             GDT32 Pointer
 *
 *    +0xF20:     AP Startup Header
 *                  - 0x00: MAGIC
 *                  - 0x08: Core Entrypoint
 *                  - 0x10: CR3
 *                  - 0x18: Stack Pointer
 *                  
 */


#define AP16_OFFSET           0x000
#define AP32_OFFSET           0x100
#define AP64_OFFSET           0x200
#define GDT32_OFFSET          0xF00
#define GDT32_PTR_OFFSET      0xF18

#define AP_BOOT_DATA_MAGIC    0xF20
#define AP_BOOT_DATA_MAIN     0xF28
#define AP_BOOT_DATA_CR3      0xF30
#define AP_BOOT_DATA_STACK    0xF38
#define AP_BOOT_DATA_GDT64    0xF40


.global __ap16_start
.global __ap32_start
.global __ap64_start
.global __ap16_end
.global __ap32_end
.global __ap64_end



        .org AP_BOOT_OFFSET
        .text
        .code16
__ap16_start:

        cli
        cld

        jmp .

        # Load GDT
        xorw    %ax, %ax
        movw    %ax, %ds
        lgdt    (AP_BOOT_OFFSET + GDT32_PTR_OFFSET)

        # Enable Protected Mode
        movl    %cr0, %eax
        btsl    $0, %eax        # PE
        movl    %eax, %cr0

        ljmp    $KERNEL_CS, $(AP_BOOT_OFFSET + AP32_OFFSET)

__ap16_end:     



        .code32
__ap32_start:
        
        # Set Kernel Descriptor
        movw    $KERNEL_DS, %ax
        movw    %ax,  %ds
        movw    %ax,  %es
        movw    %ax,  %fs
        movw    %ax,  %gs
        movw    %ax,  %ss

        # Load PML4
        movl    $(AP_BOOT_OFFSET + AP_BOOT_DATA_CR3), %eax
        movl    %eax, %cr3

        # Enable PAE
        movl    %cr4, %eax
        btsl    $5,   %eax      # PAE
        btrl    $17,  %eax      # ~PCID
        movl    %eax, %cr4

        # Enable Long Mode
        movl    $0xC0000080, %ecx
        rdmsr
        btsl    $8,   %eax      # LME
        wrmsr

        # Enable Paging
        movl    %cr0, %eax
        btsl    $31,  %eax      # PG
        btsl    $0,   %eax      # PE
        btrl    $29,  %eax      # ~NW
        btrl    $30,  %eax      # ~CD
        movl    %eax, %cr0

        # Load GDT and Code Segment
        lgdt    AP_BOOT_OFFSET + AP_BOOT_DATA_GDT64
        ljmp    $KERNEL_CS, $(AP_BOOT_OFFSET + AP64_OFFSET)

__ap32_end:



        .code64
__ap64_start:

__ap64_end:





__ap_gdt32_start:
        .quad 0x0000000000000000    //?
        .quad 0x00009A0000000000    //? 0x08 Kernel Code
        .quad 0x0000920000000000    //? 0x10 Kernel Data
__ap_gdt32_ptr:
        .word GDT32_PTR_OFFSET - GDT32_OFFSET - 1
        .long AP_BOOT_OFFSET + GDT32_OFFSET
__ap_gdt32_end:



__ap_header_start:
        .quad   AP_BOOT_HEADER_MAGIC
        .quad   0
        .quad   0
        .quad   0
        .quad   0
        .quad   0
__ap_header_end: