.ifdef __rpi__

.section ".text.boot"
.globl _start
.globl kernel_stack


.extern i_undef
.extern i_swint
.extern i_abrtp
.extern i_abrtd
.extern i_irq
.extern i_fiq


_start:
	ldr pc, __i_reset
	ldr pc, __i_undef
	ldr pc, __i_swint
	ldr pc, __i_abrtp
	ldr pc, __i_abrtd
	ldr pc, __i_irq
	ldr pc, __i_fiq

__i_reset: .word i_reset
__i_undef: .word i_undef
__i_swint: .word i_swint
__i_abrtp: .word i_abrtp
__i_abrtd: .word i_abrtd
__i_irq: .word i_irq
__i_fiq: .word i_fiq


i_reset:
	ldr r8, =_start
	mov r9, #0x0000

	ldmia r8!, {r4, r5, r6, r7}
	stmia r9!, {r4, r5, r6, r7}
	ldmia r8!, {r4, r5, r6, r7}
	stmia r9!, {r4, r5, r6, r7}
	ldmia r8!, {r4, r5, r6, r7}
	stmia r9!, {r4, r5, r6, r7}
	ldmia r8!, {r4, r5, r6, r7}
	stmia r9!, {r4, r5, r6, r7}

	mov r4, #0x80000

	cps #0x13
	add sp, r4, #0x2400
	
	cps #0x17
	add sp, r4, #0x2800

	cps #0x12
	add sp, r4, #0x2C00
	
	cps #0x1F
	add sp, r4, #0x3C00


	str sp, kernel_stack

	mrc p15, #0, r4, c1, c0, #0
	orr r4, #0x400000
	mcr p15, #0, r4, c1, c0, #0
	
	
	ldr r4, =__bss_start__
	ldr r9, =__bss_end__
	
	mov r5, #0
	mov r6, #0
	mov r7, #0
	mov r8, #0

	b .L0

.L0:
	stmia r4!, {r5-r8}
.L1:
	cmp r4, r9
	blo .L0

	ldr r3, =rpi_save_args
	blx r3

	ldr r3, =main
	blx r3

.L2:
	wfe
	b .L2


.align 4
kernel_stack: .long 0

.endif

