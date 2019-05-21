[BITS 64]
section .text

global x86_get_rip

x86_get_rip:
    pop rdx
    jmp rdx
