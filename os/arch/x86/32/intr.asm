[BITS 32]
section .text

global x86_lgdt
global x86_ltr
global x86_lidt
global x86_cli
global x86_sti
global gdtp
global idtp
global isrs

extern x86_isr_handler

%macro ISRNC 1
    global isr%1
    isr%1:
        push dword 0
        push dword %1
        jmp isr_stub
%endmacro

%macro ISREC 1
    global isr%1
    isr%1:
        push dword %1
        jmp isr_stub
%endmacro



ISRNC 0
ISRNC 1
ISRNC 2
ISRNC 3
ISRNC 4
ISRNC 5
ISRNC 6
ISRNC 7
ISREC 8
ISRNC 9
ISREC 10
ISREC 11
ISREC 12
ISREC 13
ISREC 14
ISRNC 15
ISRNC 16
ISRNC 17
ISRNC 18
ISRNC 19
ISRNC 20
ISRNC 21
ISRNC 22
ISRNC 23
ISRNC 24
ISRNC 25
ISRNC 26
ISRNC 27
ISRNC 28
ISRNC 29
ISRNC 30
ISRNC 31
ISRNC 0xFD
ISRNC 0xFE



%assign i 0
%rep 224
    global irq%[i]
    irq%[i]:
        push dword 0
        push dword (i + 32)
        jmp isr_stub
%assign i i + 1
%endrep


isrs:
%assign i 0
%rep 32
    dd isr%[i]
%assign i i + 1
%endrep

%assign i 0
%rep 221
    dd irq%[i]
%assign i i + 1
%endrep

    dd isr0xFD
    dd isr0xFE
    dd irq223
    dq 0


isr_stub:
    pushad
    push ds
    push es
    push fs
    push gs

    mov cx, 0x10
    mov ds, cx
    mov es, cx
    mov fs, cx
    mov gs, cx

    push esp
    call x86_isr_handler
    mov esp, eax

    pop gs
    pop fs
    pop es
    pop ds
    popad
    add esp, 8
iretd



x86_lgdt:
    lgdt [gdtp.ptr]
    jmp 0x08:.L0
.L0:
    mov cx, 0x10
    mov ds, cx
    mov es, cx
    mov fs, cx
    mov gs, cx
    mov ss, cx
ret

x86_ltr:
    mov ax, 0x28
    ltr ax
ret

x86_lidt:
    lidt [idtp.ptr]
ret

x86_sti:
    sti
ret

x86_cli:
    cli
ret

section .data
gdtp:
    times 32 dq 0
.ptr:
    dw $ - gdtp - 1
    dd gdtp

idtp:
    times 256 dq 0
.ptr:
    dw $ - idtp - 1
    dd idtp