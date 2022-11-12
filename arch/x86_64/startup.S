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




    .code64
    .section .multiboot

multiboot_header: 

    .align MULTIBOOT_INFO_ALIGN
    .globl multiboot_header

  # Multiboot 2
    .long MULTIBOOT2_HEADER_MAGIC
    .long MULTIBOOT_ARCHITECTURE_I386
    .long multiboot_header_end - multiboot_header
    .long -(MULTIBOOT2_HEADER_MAGIC + MULTIBOOT_ARCHITECTURE_I386 + (multiboot_header_end - multiboot_header))


  # Information Request (Optional)
    .short MULTIBOOT_HEADER_TAG_INFORMATION_REQUEST
    .short MULTIBOOT_HEADER_TAG_OPTIONAL
    .long 48
    .long MULTIBOOT_TAG_TYPE_CMDLINE
    .long MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME
    .long MULTIBOOT_TAG_TYPE_MODULE
    .long MULTIBOOT_TAG_TYPE_BOOTDEV
    .long MULTIBOOT_TAG_TYPE_VBE
    .long MULTIBOOT_TAG_TYPE_FRAMEBUFFER
    .long MULTIBOOT_TAG_TYPE_APM
    .long MULTIBOOT_TAG_TYPE_SMBIOS
    .long MULTIBOOT_TAG_TYPE_ACPI_OLD
    .long MULTIBOOT_TAG_TYPE_ACPI_NEW


  # Information Request (Required)
    .short MULTIBOOT_HEADER_TAG_INFORMATION_REQUEST
    .short 0
    .long 24
    .long MULTIBOOT_TAG_TYPE_BASIC_MEMINFO
    .long MULTIBOOT_TAG_TYPE_MMAP
    .long MULTIBOOT_TAG_TYPE_ELF_SECTIONS
    .long MULTIBOOT_TAG_TYPE_LOAD_BASE_ADDR


  # Framebuffer
    .short MULTIBOOT_HEADER_TAG_FRAMEBUFFER
    .short MULTIBOOT_HEADER_TAG_OPTIONAL
    .long 20
    .long 1024
    .long 768
    .long 32
    .align MULTIBOOT_INFO_ALIGN


  # Module Alignment
    .short MULTIBOOT_HEADER_TAG_MODULE_ALIGN
    .short 0
    .long 8


  # End
    .short MULTIBOOT_HEADER_TAG_END
    .short 0
    .long 8

multiboot_header_end:
   




.bss
.align 4096

    bootstrap_pml4:
        .skip 4096
    bootstrap_pml3_low_area: 
        .skip PAGE_COUNT(KERNEL_LOW_SIZE,   3) << 12
    bootstrap_pml3_high_area: 
        .skip PAGE_COUNT(KERNEL_HIGH_SIZE,  3) << 12
    bootstrap_pml3_heap_area:
        .skip PAGE_COUNT(KERNEL_HEAP_SIZE,  3) << 12
    bootstrap_pml2_low_area:
        .skip PAGE_COUNT(KERNEL_LOW_SIZE,   2) << 12
    bootstrap_pml2_high_area:
        .skip PAGE_COUNT(KERNEL_HIGH_SIZE,  2) << 12
    bootstrap_pml2_heap_area:
        .skip PAGE_COUNT(KERNEL_HEAP_SIZE,  2) << 12
    bootstrap_pml2_stack_area:
        .skip PAGE_COUNT(KERNEL_STACK_SIZE, 2) << 12


.align KERNEL_STACK_SIZE

    bootstrap_stack:
        .skip  KERNEL_STACK_SIZE



.data
.align 4096

        .global bootstrap_gdtp
        .global bootstrap_gdtp_ptr
        .global bootstrap_idtp
        .global bootstrap_idtp_ptr
        .global bootstrap_tss
        .global bootstrap_isr
        .global bootstrap_irq

        .extern interrupt_common
        

    bootstrap_gdtp:
        .quad 0x0000000000000000    //?
        .quad 0x00A09A0000000000    //? 0x08 Kernel Code
        .quad 0x0080920000000000    //? 0x10 Kernel Data
        .quad 0x0080F20000000000    //? 0x1B User Data
        .quad 0x00A0FA0000000000    //? 0x23 User Code
        
      .rept KERNEL_TSS_MAX
        .quad 0x0000E90000000068    //? 0x28 + (n * 16), where n is a cpu id
        .quad 0x0000000000000000    //? TSS 64 (Available)
      .endr

    bootstrap_gdtp_ptr:
        .word . - bootstrap_gdtp - 1
        .quad V2P(bootstrap_gdtp)

.align 16

    bootstrap_tss:
      .rept KERNEL_TSS_MAX
        .long 0
        .quad 0                                                      // RSP0: Syscall Stack
        .quad 0
        .quad 0
        .quad 0
        .quad KERNEL_EXCEPTION_STACK + KERNEL_EXCEPTION_STACKSIZE    // IST1: Exception Stack
        .quad KERNEL_INTERRUPT_STACK + KERNEL_INTERRUPT_STACKSIZE    // IST2: Interrupt Stack
        .quad 0
        .quad 0
        .quad 0
        .quad 0
        .quad 0
        .quad 0
        .long 104
        .quad 0                                                      // Padding
        .quad 0
        .quad 0
      .endr

.align 16

    bootstrap_idtp:
      .rept 32
        .word 0
        .word KERNEL_CS
        .byte 0x01    // IST1
        .byte 0x8F    // Trap Gate;       DPL=0
        .quad 0
        .word 0
      .endr

      # Timer Handler - idx: 0x20
        .word 0
        .word KERNEL_CS
        .byte 0x00    // RSP0
        .byte 0x8E    // Interrupt Gate;  DPL=0
        .quad 0
        .word 0

      .rept 221
        .word 0
        .word KERNEL_CS
        .byte 0x02    // IST2
        .byte 0x8E    // Interrupt Gate;  DPL=0
        .quad 0
        .word 0
      .endr

      # SYSCALL Handler - idx: 0xFE
        .word 0
        .word KERNEL_CS
        .byte 0x00    // RSP0
        .byte 0xEE    // Interrupt Gate;  DPL=3
        .quad 0
        .word 0

      # Spourious Interrupt - idx: 0xFF
        .word 0
        .word KERNEL_CS
        .byte 0x01    // IST1
        .byte 0x8E    // Interrupt Gate;  DPL=0
        .quad 0
        .word 0
   
    bootstrap_idtp_ptr:
        .word . - bootstrap_idtp - 1
        .quad bootstrap_idtp

.align 16

    bootstrap_isr:
      .set i, 0
      .rept 256
        .align 16
        
        .if (i == 8) || (i >= 10 && i <= 14) || (i == 17)   // With error code
          nop
          nop
          pushq $i
          jmp   interrupt_common
        .else                                               // Without error code
          pushq $0
          pushq $i
          jmp   interrupt_common
        .endif

        .set i, i + 1
        .align 16
      .endr

.align 16

    bootstrap_irq:
      .rept 224
        .quad 0
        .quad 0
        .quad 0
        .quad 0
      .endr

.align 16

    __raw_args:
        .quad 0, 0


.align 16, 0x90

        .text
        .code32

        .global _start
        .type _start, @function
_start:

        cli
        cld

        # Save boot arguments
        movl    $V2P(__raw_args), %edi
        movl    %eax, 0(%edi)
        movl    %ebx, 8(%edi)


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
    
    

        # Setup Mapping
        # 0x0000000000000000 - 0xFFFFFFFF80000000 - 1GiB (Kernel)
        # XXXXXXXXXXXXXXXXXX - 0xFFFFFFFFC0000000 - 2MiB (Stack)
        # 0x0000000000000000 - 0xFFFF800000000000 - 2TiB (Heap)
        
        # PML4 - Low Area
        movl    $V2P(bootstrap_pml4), %edi
        movl    $V2P(bootstrap_pml3_low_area), %esi
        addl    $(PAGE_INDEX(KERNEL_LOW_AREA, 3) << 3), %edi
        movl    $(PAGE_COUNT(KERNEL_LOW_SIZE, 3) << 0), %ecx
        btsl    $0,   %esi                              // Present
        btsl    $1,   %esi                              // R/W
    .L0:
        movl    %esi, (%edi)
        addl    $8,   %edi
        addl    $4096, %esi
        loop    .L0

        # PML4 - High Area
        movl    $V2P(bootstrap_pml4), %edi
        movl    $V2P(bootstrap_pml3_high_area), %esi
        addl    $(PAGE_INDEX(KERNEL_HIGH_AREA, 3) << 3), %edi
        movl    $(PAGE_COUNT(KERNEL_HIGH_SIZE, 3) << 0), %ecx
        btsl    $0,   %esi                              // Present
        btsl    $1,   %esi                              // R/W
    .L1:
        movl    %esi, (%edi)
        addl    $8,   %edi
        addl    $4096, %esi
        loop    .L1

        # PML4 - Heap Area
        movl    $V2P(bootstrap_pml4), %edi
        movl    $V2P(bootstrap_pml3_heap_area), %esi
        addl    $(PAGE_INDEX(KERNEL_HEAP_AREA, 3) << 3), %edi
        movl    $(PAGE_COUNT(KERNEL_HEAP_SIZE, 3) << 0), %ecx
        btsl    $0,   %esi                              // Present
        btsl    $1,   %esi                              // R/W
    .L2:
        movl    %esi, (%edi)
        addl    $8,   %edi
        addl    $4096, %esi
        loop    .L2


        # PML3 - Low Area
        movl    $V2P(bootstrap_pml3_low_area), %edi
        movl    $V2P(bootstrap_pml2_low_area), %esi
        addl    $(PAGE_INDEX(KERNEL_LOW_AREA, 2) << 3), %edi
        movl    $(PAGE_COUNT(KERNEL_LOW_SIZE, 2) << 0), %ecx
        btsl    $0,   %esi                              // Present
        btsl    $1,   %esi                              // R/W
    .L3:
        movl    %esi, (%edi)
        addl    $8,   %edi
        addl    $4096, %esi
        loop    .L3

        # PML3 - High Area
        movl    $V2P(bootstrap_pml3_high_area), %edi
        movl    $V2P(bootstrap_pml2_high_area), %esi
        addl    $(PAGE_INDEX(KERNEL_HIGH_AREA, 2) << 3), %edi
        movl    $(PAGE_COUNT(KERNEL_HIGH_SIZE, 2) << 0), %ecx
        btsl    $0,   %esi                              // Present
        btsl    $1,   %esi                              // R/W
    .L4:
        movl    %esi, (%edi)
        addl    $8,   %edi
        addl    $4096, %esi
        loop    .L4

        # PML3 - Heap Area
        movl    $V2P(bootstrap_pml3_heap_area), %edi
        movl    $V2P(bootstrap_pml2_heap_area), %esi
        addl    $(PAGE_INDEX(KERNEL_HEAP_AREA, 2) << 3), %edi
        movl    $(PAGE_COUNT(KERNEL_HEAP_SIZE, 2) << 0), %ecx
        btsl    $0,   %esi                              // Present
        btsl    $1,   %esi                              // R/W
    .L5:
        movl    %esi, (%edi)
        addl    $8,   %edi
        addl    $4096, %esi
        loop    .L5

        # PML3 - Stack Area
        movl    $V2P(bootstrap_pml3_high_area), %edi
        movl    $V2P(bootstrap_pml2_stack_area), %esi
        addl    $(PAGE_INDEX(KERNEL_STACK_AREA, 2) << 3), %edi
        movl    $(PAGE_COUNT(KERNEL_STACK_SIZE, 2) << 0), %ecx
        btsl    $0,   %esi                              // Present
        btsl    $1,   %esi                              // R/W
    .L6:
        movl    %esi, (%edi)
        addl    $8,   %edi
        addl    $4096, %esi
        loop    .L6


        # PML2 - Low Area
        movl    $V2P(bootstrap_pml2_low_area), %edi
        movl    $0, %esi
        addl    $(PAGE_INDEX(KERNEL_LOW_AREA, 1) << 3), %edi
        movl    $(PAGE_COUNT(KERNEL_LOW_SIZE, 1) << 0), %ecx
        btsl    $0,   %esi                              // Present
        btsl    $1,   %esi                              // R/W
        btsl    $7,   %esi                              // 2MiB
        btsl    $8,   %esi                              // Global
    .L7:
        movl    %esi, (%edi)
        addl    $8,   %edi
        addl    $2097152, %esi
        loop    .L7

        # PML2 - High Area
        movl    $V2P(bootstrap_pml2_high_area), %edi
        movl    $0, %esi
        addl    $(PAGE_INDEX(KERNEL_HIGH_AREA, 1) << 3), %edi
        movl    $(PAGE_COUNT(KERNEL_HIGH_SIZE, 1) << 0), %ecx
        btsl    $0,   %esi                              // Present
        btsl    $1,   %esi                              // R/W
        btsl    $7,   %esi                              // 2MiB
        btsl    $8,   %esi                              // Global
    .L8:
        movl    %esi, (%edi)
        addl    $8,   %edi
        addl    $2097152, %esi
        loop    .L8

        # PML2 - Heap Area
        movl    $V2P(bootstrap_pml2_heap_area), %edi
        movl    $0, %esi
        addl    $(PAGE_INDEX(KERNEL_HEAP_AREA, 1) << 3), %edi
        movl    $(PAGE_COUNT(KERNEL_HEAP_SIZE, 1) << 0), %ecx
        btsl    $0,   %esi                              // Present
        btsl    $1,   %esi                              // R/W
        btsl    $7,   %esi                              // 2MiB
        btsl    $8,   %esi                              // Global
    .L9:
        movl    %esi, (%edi)
        addl    $8,   %edi
        addl    $2097152, %esi
        loop    .L9

        # PML2 - Stack Area
        movl    $V2P(bootstrap_pml2_stack_area), %edi
        movl    $V2P(bootstrap_stack), %esi
        addl    $(PAGE_INDEX(KERNEL_STACK_AREA, 1) << 3), %edi
        movl    $(PAGE_COUNT(KERNEL_STACK_SIZE, 1) << 0), %ecx
        btsl    $0,   %esi                              // Present
        btsl    $1,   %esi                              // R/W
        btsl    $7,   %esi                              // 2MiB
    .L10:
        movl    %esi, (%edi)
        addl    $8,   %edi
        addl    $2097152, %esi
        loop    .L10

        

        # Enable PAE
        movl    %cr4, %eax
        btsl    $5,   %eax      # PAE
        btrl    $17,  %eax      # ~PCID
        movl    %eax, %cr4

        # Setup startup boot page tables
        movl    $V2P(bootstrap_pml4), %eax
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
        lgdt    V2P(bootstrap_gdtp_ptr)
        ljmp    $KERNEL_CS, $V2P(_start64)


        .code64
_start64:
        
        # Set data-segments
        movq    $KERNEL_DS, %rax
        movq    %rax, %ds
        movq    %rax, %es
        movq    %rax, %ss

        xorq    %rax, %rax
        movq    %rax, %fs
        movq    %rax, %gs


        # Jump to High Area
        lea     _start_high64, %rax
        jmp     *%rax

_start_high64:

        # Set Stack
        movq    $KERNEL_STACK_AREA, %rsp
        addq    $KERNEL_STACK_SIZE, %rsp

        # Unmap Low Area
        movq    $bootstrap_pml4, %rdi
        movq    $0,   (%rdi)

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
        movq    $bootstrap_gdtp_ptr, %rcx
        movq    $bootstrap_gdtp, 2(%rcx)
        lgdt    (bootstrap_gdtp_ptr)

        # Load TSS
        movq    $bootstrap_tss, %rsi
        movq    $bootstrap_gdtp, %rdi
        movq    $KERNEL_TSS_MAX, %rcx
        addq    $KERNEL_TSS, %rdi

    .L12:
        movq    %rsi, %rax
        movw    %ax,  2(%rdi)     // Base 00:16

        shrq    $16,  %rax
        movb    %al,  4(%rdi)     // Base 16:24

        shrq    $8,   %rax
        movb    %al,  7(%rdi)     // Base 24:32
        
        shrq    $8,   %rax
        movl    %eax, 8(%rdi)     // Base 32:64

        addq    $16,  %rdi
        addq    $128, %rsi
        loop    .L12

        movq    $(KERNEL_TSS | 3), %rax
        ltrw    %ax

        # Load IDT
        movq    $bootstrap_idtp, %rdi
        movq    $bootstrap_isr, %rsi
        movq    $256, %rcx
    .L13:
        movq    %rsi, %rax
        movw    %ax,  0(%rdi)     // Offset 00:16

        shrq    $16,  %rax
        movw    %ax,  6(%rdi)     // Offset 16:32

        shrq    $16,  %rax
        movl    %eax, 8(%rdi)     // Offset 32:64

        addq    $16,  %rdi
        addq    $16,  %rsi
        loop    .L13

        movq    $bootstrap_idtp_ptr, %rax
        lidt    (%rax)

        # Stack unwinding
        pushq   $0
        pushq   $0

        # Align Stack
        andq    $-16, %rsp

#if defined(__OPTIMIZE__) && !defined(CONFIG_HAVE_DEBUG)
        addq    $8, %rsp  // BUG: stack not aligned when code is optimized by compiler
#endif

        # Restore boot arguments
        leaq    __raw_args, %rcx
        movq    0(%rcx), %rdi
        movq    8(%rcx), %rsi
        addq    $KERNEL_HIGH_AREA, %rsi     // High Area
        addq    $8,   %rsi                  // Skip reserved bytes

        # Reset RFLAGS
        pushq   $2
        popfq

        # Call Boot Main()
        .extern bmain
        STDCALL(bmain)

        # Call Kernel Main()
        .extern kmain
        STDCALL(kmain)

halt:
        hlt
        jmp halt

END(_start)



