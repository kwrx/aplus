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


#include <aplus.h>
#include <aplus/multiboot.h>
#include <arch/x86/asm.h>
#include <arch/x86/cpu.h>
#include <asm/linkage.h>



.global __ap_begin
.global __ap_end

.extern bootstrap_gdtp_ptr
.extern bootstrap_idtp_ptr


#define AP_RELOC(p)     \
    (AP_BOOT_OFFSET + (p - __ap_begin))



        .org AP_BOOT_OFFSET
        .text
        .code16
__ap_begin:

        cli
        cld

        # Load GDT
        xorw    %ax, %ax
        movw    %ax, %ds
        lgdt    AP_RELOC(ap_gdt32p)

        # Enable Protected Mode
        movl    %cr0, %eax
        btsl    $0, %eax        # PE
        movl    %eax, %cr0

        ljmp    $KERNEL_CS, $AP_RELOC(ap32_start)


        .code32
ap32_start:

        # Set Kernel Descriptor
        movw    $KERNEL_DS, %ax
        movw    %ax,  %ds
        movw    %ax,  %es
        movw    %ax,  %fs
        movw    %ax,  %gs
        movw    %ax,  %ss
        

        # Enable PAE
        movl    %cr4, %eax
        btsl    $5,   %eax      # PAE
        btrl    $17,  %eax      # ~PCID
        movl    %eax, %cr4

        # Load PML4
        movl    AP_RELOC(ap_header_cr3), %eax
        movl    %eax, %cr3

        # Enable Long Mode
        movl    $X86_MSR_EFER, %ecx
        rdmsr
        btsl    $8,   %eax      # LME
        wrmsr

        # Enable Paging
        movl    %cr0, %eax
        btsl    $16,  %eax      # WP
        btsl    $31,  %eax      # PG
        btsl    $0,   %eax      # PE
        btrl    $29,  %eax      # ~NW
        btrl    $30,  %eax      # ~CD
        movl    %eax, %cr0

        # Load GDT and Code Segment
        lgdt    AP_RELOC(ap_gdt64p)

        ljmp    $KERNEL_CS, $AP_RELOC(ap64_start)


        .code64
ap64_start:

         # Set data-segments
        movq    $KERNEL_DS, %rax
        movq    %rax, %ds
        movq    %rax, %es
        movq    %rax, %ss

        xorq    %rax, %rax
        movq    %rax, %fs
        movq    %rax, %gs


        # Jump to High Area
        lea     ap64_highstart, %rax
        jmp     *%rax

ap64_highstart:

        # Set Stack
        movq    AP_RELOC(ap_header_stack), %rsp
        addq    $KERNEL_STACK_SIZE, %rsp

        # Enable PGE
        movq    %cr4, %rcx
        btsq    $7,   %rcx      # PGE
        movq    %rcx, %cr4

        # Invalidate TLB
        movq    %cr3, %rcx
        movq    %ecx, %cr3

        # Enable PAT
        movq    $X86_MSR_PAT, %rcx
        rdmsr
        andl    $0xFFFFFF00, %edx
        orl     $0x00000001, %edx
        wrmsr

        # Load High GDT
        lgdt    (bootstrap_gdtp_ptr)

        # Load TSS
        movq    AP_RELOC(ap_header_cpu), %rax
        shlq    $4, %rax
        addq    $(KERNEL_TSS | 3), %rax
        ltrw    %ax

        # Load IDT
        lidt    (bootstrap_idtp_ptr)

        # Stack unwinding
        pushq   $0
        pushq   $0

        # Reset RFLAGS
        pushq   $0
        popfq

        # Load Boot Arguments
        movq    AP_RELOC(ap_header), %rdi
        movq    AP_RELOC(ap_header_cpu), %rsi

        # Call AP Boot Main()
        .extern ap_bmain
        STDCALL(ap_bmain)

        # Call Kernel AP Main()
        .extern cmain
        STDCALL(cmain)

halt:
        hlt
        jmp halt




.align 0x100, 0x90

    ap_gdt32:
        .quad 0x0000000000000000    //?
        .quad 0x00CF98000000FFFF    //? 0x08 Kernel Code
        .quad 0x00CF92000000FFFF    //? 0x10 Kernel Data
    ap_gdt32p:
        .word AP_RELOC(.) - AP_RELOC(ap_gdt32) - 1
        .long AP_RELOC(ap_gdt32)
    

.align 0x100, 0x90

    ap_gdt64:
        .quad 0x0000000000000000    //?
        .quad 0x00A09A0000000000    //? 0x08 Kernel Code
        .quad 0x0080920000000000    //? 0x10 Kernel Data
    ap_gdt64p:
        .word AP_RELOC(.) - AP_RELOC(ap_gdt64) - 1
        .quad AP_RELOC(ap_gdt64)


.align 0x100, 0x90

    ap_header:
        .quad   AP_BOOT_HEADER_MAGIC
    ap_header_cpu:
        .quad   0
    ap_header_cr3:
        .quad   0
    ap_header_stack:
        .quad   0
  

__ap_end: