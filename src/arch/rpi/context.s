.ifdef __rpi__


.globl task_context_switch
task_context_switch:
	cps #0x1F
	push {r0-r14}
	str sp, [r0]
	ldr sp, [r1]
	pop {r0-r14}
	mov pc, lr

.endif
