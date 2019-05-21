[BITS 32]
section .text

global x86_get_eip
global x86_switch

extern apic_eoi

x86_get_eip:
    pop edx
    jmp edx


x86_switch:
    mov esi, [esp + 4]
    mov edi, [esp + 8]
    mov ebx, [esp + 12]
    mov ecx, [esp + 16]
    mov edx, [esp + 20]

    push eax
    push ebx
    push ecx
    push edx
    push esi
    push edi
    push ebp

    push ds
    push es
    push fs
    push gs

    mov eax, cr3

    mov [esi], esp
    mov [edi], eax

    mov cr3, ecx
    mov esp, ebx

    pop gs
    pop fs
    pop es
    pop ds

    pop ebp
    pop edi
    pop esi
    pop edx
    pop ecx
    pop ebx
    pop eax

    cmp edx, 0
    je .L0

    call apic_eoi

.L0:
iret

