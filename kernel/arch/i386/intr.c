#include <aplus.h>
#include <aplus/ipc.h>
#include <aplus/mm.h>
#include <aplus/debug.h>
#include <aplus/intr.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <libc.h>

#include <arch/i386/i386.h>



extern void gdt_load();
extern void fork_handler(void*);
extern void yield_handler(void*);


extern uint64_t GDT32[6];

extern struct {
    struct {
        uint16_t base_low;
        uint16_t selector;
        uint8_t unused;
        uint8_t flags;
        uint16_t base_high;
    } e[256];

    struct {
        uint16_t limit;
        uint32_t base;
    } p;
} IDT32;

extern struct {
    void* data;
    irq_handler_t* handler;
} IRQ32[16];

extern struct {
    uint32_t link;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldtr;
    uint32_t iopb;
} TSS32;


//static char tmp_stack[CONFIG_STACK_SIZE];
int current_irq = -1;


int intr_init() {
    __asm__ __volatile__ ("cli");

#if 0
    GDT32[5] = (uint64_t) (
               (((uint64_t) ((uintptr_t) &TSS32) & 0x00FFFFFFULL) << 16)        |
               (((uint64_t) ((uintptr_t) &TSS32) & 0xFF000000ULL) << 32)        |
               (0x0000890000000000ULL)                                          |
               (sizeof(TSS32) - 1));


    
    TSS32.es =
    TSS32.ss =
    TSS32.ds =
    TSS32.fs =
    TSS32.gs = 0x10;
    TSS32.cs = 0x08;

    TSS32.ss0 = 0x10;
    TSS32.esp = (uint32_t) &tmp_stack[CONFIG_STACK_SIZE];

    __asm__ __volatile__("ltr %%ax" : : "a"((5 << 3)));
    gdt_load();
#endif


    #define _i(x)                                                               \
        extern void isr##x (void*);                                             \
        IDT32.e[x].base_low = ((uintptr_t) isr##x) & 0xFFFF;                    \
        IDT32.e[x].base_high = ((uintptr_t) isr##x >> 16) & 0xFFFF;             \
        IDT32.e[x].selector = 0x08;                                             \
        IDT32.e[x].unused = 0;                                                  \
        IDT32.e[x].flags = 0x8E


    _i(0);
    _i(1);
    _i(2);
    _i(3);
    _i(4);
    _i(5);
    _i(6);
    _i(7);
    _i(8);
    _i(9);
    _i(10);
    _i(11);
    _i(12);
    _i(13);
    _i(14);
    _i(15);
    _i(16);
    _i(17);
    _i(18);
    _i(19);
    _i(20);
    _i(21);
    _i(22);
    _i(23);
    _i(24);
    _i(25);
    _i(26);
    _i(27);
    _i(28);
    _i(29);
    _i(30);
    _i(31);
    _i(0x7F);
    _i(0x80);

    __asm__ __volatile__ ("lidt [eax]" : : "a" (&IDT32.p));



    #undef _i
    #define _i(x)                                                                   \
        extern void irq##x (void*);                                                 \
        IDT32.e[x + 32].base_low = ((uintptr_t) irq##x) & 0xFFFF;                   \
        IDT32.e[x + 32].base_high = ((uintptr_t) irq##x >> 16) & 0xFFFF;            \
        IDT32.e[x + 32].selector = 0x08;                                            \
        IDT32.e[x + 32].unused = 0;                                                 \
        IDT32.e[x + 32].flags = 0x8E


    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, 0x00);
    outb(0xA1, 0x00);
    

    _i(0);
    _i(1);
    _i(2);
    _i(3);
    _i(4);
    _i(5);
    _i(6);
    _i(7);
    _i(8);
    _i(9);
    _i(10);
    _i(11);
    _i(12);
    _i(13);
    _i(14);
    _i(15);




    __asm__ __volatile__ ("sti");
    return E_OK;
}


void intr_enable(void) {
    __asm__ __volatile__ ("sti");
}

void intr_disable(void) {
    __asm__ __volatile__ ("cli");
}


void irq_enable(int number, irq_handler_t handler) {
    KASSERT(number >= 0 && number < 16);

    IRQ32[number].data = NULL;
    IRQ32[number].handler = handler;
}

void irq_disable(int number) {
    KASSERT(number >= 0 && number < 16);

    IRQ32[number].data = NULL;
    IRQ32[number].handler = NULL;
}

void* irq_set_data(int number, void* data) {
    KASSERT(number >= 0 && number < 16);

    void* tmp = IRQ32[number].data;
    IRQ32[number].data = data;

    return tmp;
}

void* irq_get_data(int number) {
    KASSERT(number >= 0 && number < 16);

    return IRQ32[number].data;
}

void irq_ack(int irq_no) {
    if(current_irq == -1)
        return;
        
    if(irq_no >= 8)
        outb(0xA0, 0x20);

    outb(0x20, 0x20);
    current_irq = -1;
}


void isr_handler(i386_context_t* context) {
#if DEBUG
    static char *exception_messages[] = {
        "Division By Zero",
        "Debug",
        "Non Maskable Interrupt",
        "Breakpoint",
        "Into Detected Overflow",
        "Out of Bounds",
        "Invalid Opcode",
        "No Coprocessor",

        "Double Fault",
        "Coprocessor Segment Overrun",
        "Bad TSS",
        "Segment Not Present",
        "Stack Fault",
        "General Protection Fault",
        "Page Fault",
        "Unknown Interrupt",

        "Coprocessor Fault",
        "Alignment Check",
        "Machine Check",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",

        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved"
    };
#endif




    int signo;
    switch(context->int_no) {
        case 0x80:
            context->eax = syscall_handler(context->eax, context->ebx, context->ecx, context->edx, context->esi, context->edi);
            context->ebx = errno;
            return;
        case 0x7F:
            switch(context->eax) {
                case 0:
                    fork_handler(context);
                    return;
                case 1:
                    schedule_yield();
                    return;
            }
            return;

        case 0x00:
        case 0x04:
        case 0x10:
        case 0x13:
            signo = SIGFPE;
            break;
        case 0x0B:
        case 0x0C:
        case 0x0D:
        case 0x0E:
            signo = SIGSEGV;
            break;
        case 0x06:
        case 0x0A:
            signo = SIGILL;
            break;
        case 0x01:
        case 0x03:
            signo = SIGTRAP;
            break;
        default:
            signo = SIGABRT;
            break;
    }


    debug_dump(context, exception_messages[context->int_no], 0, context->err_code);


    if(unlikely(current_task == kernel_task))
        for(;;) 
            __asm__ __volatile__ ("cli; hlt");
    

    __asm__ __volatile__ ("sti");
    sys_kill(current_task->pid, signo);
    sys_yield();
}


void irq_handler(i386_context_t* context) {
    int irq_no = context->int_no - 32;
    current_irq = irq_no;

    if(likely(IRQ32[irq_no].handler))
        IRQ32[irq_no].handler ((void*) context);
    else
        kprintf(WARN "irq_handler(): unhandled IRQ #%d\n", irq_no);

    irq_ack(irq_no);
}


void x86_intr_kernel_stack(uintptr_t address) {
    TSS32.esp0 = (uint32_t) address;
}

EXPORT(irq_enable);
EXPORT(irq_disable);
EXPORT(irq_set_data);
EXPORT(irq_get_data);
EXPORT(irq_ack);

EXPORT(intr_enable);
EXPORT(intr_disable);
