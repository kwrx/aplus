[BITS 32]
section .text

global x86_get_eip
global x86_enter_on_clone
global x86_enter_on_userspace


x86_get_eip:
    pop edx
    jmp edx


x86_enter_on_clone:
    push ebx
    call ecx
    push eax
    ;call sys_exit ; TODO
    jmp $


x86_enter_on_userspace:
    push ebp
    mov ebp, esp

    mov ecx, [ebp + 8]      ; IP
    mov edx, [ebp + 12]     ; Stack
    mov ebx, [ebp + 16]     ; Arg

    mov ax, 0x23
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov esp, edx
    
    push ebx
    push dword 0

    mov edx, esp

    push dword 0x23
    push edx
    push dword 0x202
    push dword 0x1B
    push ecx

iretd