[BITS 32]
section .text

global x86_get_eip
global x86_enter_on_clone

x86_get_eip:
    pop edx
    jmp edx


x86_enter_on_clone:
    push ebx
    call ecx
    push eax
    ;call sys_exit ; TODO
    jmp $