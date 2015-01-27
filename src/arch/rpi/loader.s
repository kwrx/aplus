.ifdef __rpi__

.section ".text.boot"
.globl _start


_start:
	mov r4, #0x80000

	cps #0x13
	add sp, r4, #0x2400
	
	cps #0x17
	add sp, r4, #0x2800

	cps #0x12
	add sp, r4, #0x2C00
	
	cps #0x1F
	add sp, r4, #0x3C00


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

.endif
