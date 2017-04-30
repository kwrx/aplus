.section .text.boot
.globl _start


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

	ldmia r8!, {r4-r7}
	stmia r9!, {r4-r7}
	ldmia r8!, {r4-r7}
	stmia r9!, {r4-r7}
	ldmia r8!, {r4-r7}
	stmia r9!, {r4-r7}
	ldmia r8!, {r4-r7}
	stmia r9!, {r4-r7}

	mov r4, #0x80000

	cps #0x13
	add sp, r4, #0x2400
	
	cps #0x17
	add sp, r4, #0x2800

	cps #0x12
	add sp, r4, #0x2C00
	
	cps #0x1F
	add sp, r4, #0x3C00


.fpu:
	ldr r0, =(0xF << 20)
	mcr p15, 0, r0, c1, c0, 2
	mov r3, #0x40000000
	.long 0xEEE83A10

.unaligned_memory:
	mrc p15, #0, r4, c1, c0, #0
	orr r4, #0x400000
	mcr p15, #0, r4, c1, c0, #0
	

	ldr r3, =atags_save_args
	blx r3

	ldr r3, =arm_init
	blx r3

	ldr r3, =main
	blx r3

.L2:
	wfe
	b .L2


