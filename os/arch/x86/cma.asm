global contiguous_memory_area

section .bss
align 0x1000
contiguous_memory_area:
    resb (1024 * 1024)
