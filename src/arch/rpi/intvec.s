.ifdef __rpi__

.globl interrupt_vector_table

.extern i_reset
.extern i_undef
.extern i_swint
.extern i_abrtp
.extern i_abrtd
.extern i_irq
.extern i_fiq

interrupt_vector_table:
    b i_reset 	@ Reset Handler
    b i_undef 	@ Undefined
    b i_swint 	@ SWI Handler
    b i_abrtp 	@ Prefetch Abort
    b i_abrtd 	@ Data Abort
    b i_irq 	@ IRQ
    b i_fiq 	@ FIQ



.endif
