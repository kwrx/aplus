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


#include <aplus/core/linkage.h>
#include <aplus/core/multiboot.h>
#include <arch/x86_64/asm.h>



    .code32
    .section .multiboot

multiboot_header: 

    .align MULTIBOOT_INFO_ALIGN
    .globl multiboot_header

  # Multiboot 2
    .long MULTIBOOT2_HEADER_MAGIC
    .long MULTIBOOT_ARCHITECTURE_I386
    .long multiboot_header_end - multiboot_header
    .long -(MULTIBOOT2_HEADER_MAGIC + MULTIBOOT_ARCHITECTURE_I386 + (multiboot_header_end - multiboot_header))

  # Framebuffer
    .short MULTIBOOT_HEADER_TAG_FRAMEBUFFER
    .short MULTIBOOT_HEADER_TAG_OPTIONAL
    .long 20
    .long 1280
    .long 720
    .long 32

  # Module Alignment
    .short MULTIBOOT_HEADER_TAG_MODULE_ALIGN
    .short 0
    
  # End
    .short MULTIBOOT_HEADER_TAG_END
    .short 0

multiboot_header_end:
   




.bss
.align 4096

    startup_pml4:
        .skip 4096
    startup_pml3_low_area: 
        .skip 4096
    startup_pml3_high_area: 
        .skip 4096
    startup_pml2_stack_area:
        .skip 4096
        .align KERNEL_STACK_SIZE
    startup_stack:
        .skip  KERNEL_STACK_SIZE


.data
.align 4096

    startup_gdtp:
        .quad 0x000100000000FFFF
        .quad 0x00AF9A0000000000
        .quad 0x0000920000000000
    startup_gdtp_ptr:
        .word . - startup_gdtp - 1
        .quad V2P(startup_gdtp)

    __raw_args:
        .quad 0, 0






        .text
        .code32

        .global _start
        .type _start, @function
_start:

        cli
        cld

        # Clear Screen
        movl    $0xB8000, %edi
        movl    $0, %eax
        movl    80 * 25, %ecx
        rep stosb


        # Clear .bss
        .extern __bss_start
        .extern __bss_end

        movl    $V2P(__bss_end), %ecx
        subl    $V2P(__bss_start), %ecx
        movl    $0, %eax
        movl    $V2P(__bss_start), %edi
        cld
        rep stosb
    
        
        # Save boot arguments
        movl    $V2P(__raw_args), %edi
        movl    %eax, 0(%edi)
        movl    %ebx, 8(%edi)

        # PML4
        movl    $V2P(startup_pml4), %edi
        movl    $V2P(startup_pml3_low_area), %esi
        btsl    $0,   %esi                              // Present
        movl    %esi, (%edi)

        movl    $V2P(startup_pml3_high_area), %esi
        btsl    $0,   %esi                              // Present
        movl    %esi, (511 * 8)(%edi)

        # PML3
        movl    $V2P(startup_pml3_low_area), %edi
        movl    $0x183, %esi                            // Global, 1GiB, R/W, Present
        movl    %esi, (%edi)

        movl    $V2P(startup_pml3_high_area), %edi
        movl    $0x183, %esi                            // Global, 1GiB, R/W, Present
        movl    %esi, (510 * 8)(%edi)

        movl    $V2P(startup_pml2_stack_area), %esi
        btsl    $0,   %esi                              // Present
        movl    %esi, (511 * 8)(%edi)

        # PML2
        movl    $V2P(startup_pml2_stack_area), %edi
        movl    $V2P(startup_stack), %esi
        orl     $0x83, %esi                             // 2MiB, R/W, Present
        movl    %esi, (%edi)
        

        # Enable PAE, PGE
        movl    %cr4, %eax
        btsl    $5,   %eax      # PAE
        btsl    $7,   %eax      # PGE
        movl    %eax, %cr4

        # Setup startup boot page tables
        movl    $V2P(startup_pml4), %eax
        movl    %eax, %cr3

        # Enable Long Mode
        movl    $0xC0000080, %ecx
        rdmsr
        btsl    $8,   %eax      # LME
        btsl    $11,  %eax      # NX
        wrmsr

        # Enable Paging
        movl    %cr0, %eax
        btsl    $31,  %eax      # PG
        btsl    $0,   %eax      # PE
        movl    %eax, %cr0

        # Load GDT and Code Segment
        lgdt    V2P(startup_gdtp_ptr)
        ljmp    $KERNEL_CS, $V2P(_start64)


        .code64
_start64:
        
        # Set data-segments
        movl    $KERNEL_DS, %eax
        movl    %eax, %ds
        movl    %eax, %es
        movl    %eax, %ss

        # Jump to High Area
        lea     _start_high64, %rax
        jmp     *%rax

_start_high64:

        # Set Stack
        movq    $KERNEL_STACK_AREA, %rsp
        addq    $KERNEL_STACK_SIZE, %rsp

        # Unmap Low Area
        movq    $startup_pml4, %rdi
        movq    $0,   (%rdi)

        # Invalidate TLB
        movq    %cr3, %rcx
        movq    %ecx, %cr3

        # Enable PAT
        movq    $0x277, %rcx
        rdmsr
        andl    $0xFFFFFF00, %edx
        orl     $0x00000001, %edx
        wrmsr

        # Enable SSE
        movq    %cr0, %rcx
        andw    $0xFFFB, %cx
        orl     $2,   %ecx
        movq    %rcx, %cr0
        movq    %cr4, %rcx
        orw     $0x0600, %cx
        movq    %rcx, %cr4

        # Stack unwinding
        pushq   $0
        pushq   $0

        # Restore boot arguments
        leaq    __raw_args, %rcx
        movq    0(%rcx), %rdi
        movq    8(%rcx), %rsi
        addq    $KERNEL_HIGH_AREA, %rbx

        # Reset RFLAGS
        pushq   $0
        popfq

        .extern bmain

        movq $1, %rdi
        movq $2, %rsi
        movq $3, %rdx
        movq $4, %rcx
        movq $5, %r8
        movq $6, %r9
        call    bmain

        jmp .