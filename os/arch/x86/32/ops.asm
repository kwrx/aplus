[BITS 32]
section .text

global x86_get_eip

x86_get_eip:
    pop edx
    jmp edx


